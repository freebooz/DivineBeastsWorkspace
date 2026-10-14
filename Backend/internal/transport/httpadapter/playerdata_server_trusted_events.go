package httpadapter

// PlayerData可信GameServer库存事件的独立内网HTTP入口。
// 只有在生产组合根同时装配受控凭据、正式准入租约验证器、事件日志与
// 可靠库存事务时才启用。普通玩家Gateway及NewPlayerDataHandler绝不注册该路由。
// 生产还应通过服务网格/mTLS和网络策略限制只有Dedicated Server可访问。

import (
	"crypto/subtle"

	"mime"
	"net/http"
	"strings"
	"divinebeasts/backend/internal/app/gateway"

	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/modules/playerdata"
)

const trustedInventoryEventPath = "POST /internal/v1/playerdata/inventory/trusted-events"

// NewPlayerDataHandlerWithTrustedEvents（带可信事件摄入的PlayerData Handler）
// 不改变现有公网友关API和普通PlayerData原路由。secret必须是独立部署的强随机令牌。
func NewPlayerDataHandlerWithTrustedEvents(
	service *playerdata.Service, processor *inventory.TrustedEventProcessor, secret string,
) http.Handler {
	if service==nil || processor==nil || len(secret)<32 {
		panic("可信库存事件必须装配PlayerData、验证器及至少32字节独立内部凭据")
	}
	mux:=http.NewServeMux()
	mux.Handle(trustedInventoryEventPath,NewTrustedInventoryEventHandler(processor,secret))
	// 已实现真实平台会话续租端口时才能暴露此额外内部接口；默认普通Handler不注册。
	if renewer,ok:=processor.Verifier.(inventory.ServerLeaseRenewer);ok {
		mux.Handle(trustedInventoryRenewPath,NewTrustedInventorySessionRenewHandler(renewer,secret))
	}
	mux.Handle("/",NewPlayerDataHandler(service))
	return mux
}

// NewTrustedInventoryEventHandler（受控服务器事件HTTP Handler）仅验证传输层来源，
// 游戏服、玩家会话、事件真实性还必须由TrustedEventProcessor和PostgreSQL再次校验。
func NewTrustedInventoryEventHandler(processor *inventory.TrustedEventProcessor,secret string) http.Handler {
	if processor==nil || len(secret)<32 {
		panic("可信事件处理器或独立凭据不存在")
	}
	return http.HandlerFunc(func(w http.ResponseWriter,r *http.Request){
		if r.Method != http.MethodPost {
			w.Header().Set("Allow", http.MethodPost)
			writeError(w,http.StatusMethodNotAllowed,"METHOD_NOT_ALLOWED",nil)
			return
		}
		// 鉴权必须早于读取任意请求体，且不能将凭据原文写入日志。
		pieces:=strings.Fields(r.Header.Get("Authorization"))
		if len(pieces)!=2 || !strings.EqualFold(pieces[0],"Bearer") ||
			subtle.ConstantTimeCompare([]byte(pieces[1]),[]byte(secret))!=1 {
			w.Header().Set("WWW-Authenticate","Bearer")
			writeError(w,http.StatusUnauthorized,"INVENTORY_TRUST_AUTH_REQUIRED",nil)
			return
		}
		// 必须解析实际MediaType（媒体类型），拒绝application/json-malformed之类伪前缀。
		mediaType, _, mediaErr := mime.ParseMediaType(r.Header.Get("Content-Type"))
		if mediaErr != nil || !strings.EqualFold(mediaType,"application/json") {
			writeError(w,http.StatusUnsupportedMediaType,"INVALID_REQUEST",nil)
			return
		}
		r.Body=http.MaxBytesReader(w,r.Body,8192)
		var event inventory.ConfirmedInventoryEvent
		// 复用平台已有严格JSON解码：禁止重复key、未知字段、null必填和尾随数据。
		// 避免Go标准Decoder默认后值覆盖前值造成服务器身份审计歧义。
		allowed := []string{"eventKind","gameId","gameServerId","serverBootId",
			"playerId","sessionEpoch","connectionId","sourceEventId","expectedRevision",
			"itemDefinitionId","itemInstanceId","quantity","reasonCode"}
		required := []string{"eventKind","gameId","gameServerId","serverBootId",
			"playerId","sessionEpoch","connectionId","sourceEventId","expectedRevision",
			"itemDefinitionId","quantity","reasonCode"}
		if err:=gateway.DecodeOnlineJSON(r,&event,allowed,required);err!=nil {
			writeError(w,http.StatusBadRequest,"INVALID_REQUEST",nil)
			return
		}
		// 请求体字段不能代替传输端认证，必须和当前GameServer内部身份头一致。
		serverID:=strings.TrimSpace(r.Header.Get("X-Game-Server-Id"))
		bootID:=strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))
		if serverID=="" || bootID=="" ||
			serverID!=event.GameServerID || bootID!=event.ServerBootID {
			writeError(w,http.StatusForbidden,"GAME_SERVER_IDENTITY_MISMATCH",nil)
			return
		}
		result,err:=processor.Process(r.Context(),event)
		if err!=nil {
			writeOnlineDomainError(w,err)
			return
		}
		writeJSON(w,http.StatusOK,result)
	})
}
