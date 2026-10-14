package httpadapter

// PlayerData内部会话续租HTTP端口：只服务已签发且仍然有效的真实Dedicated Server租约。
// 绝不对普通Gateway注册，也不接受客户端PlayerID/Boot文字作为独立身份凭证。

import (
    "crypto/subtle"
    "mime"
    "net/http"
    "strings"

    "divinebeasts/backend/internal/app/gateway"
    "divinebeasts/backend/internal/modules/inventory"
)

const trustedInventoryRenewPath = "POST /internal/v1/playerdata/inventory/session-renew"

type trustedInventoryRenewRequest struct {
    GameID string `json:"gameId"`
    PlayerID string `json:"playerId"`
    GameServerID string `json:"gameServerId"`
    ServerBootID string `json:"serverBootId"`
    SessionEpoch int64 `json:"sessionEpoch"`
    ConnectionID string `json:"connectionId"`
    LeaseSeconds int32 `json:"leaseSeconds"`
}

// NewTrustedInventorySessionRenewHandler（创建已绑定玩家的续租端口）。
// 只有已在数据库成功完成保留→领取→提交的连接可以续租，不能授予新的登录资格。
func NewTrustedInventorySessionRenewHandler(
    renewer inventory.ServerLeaseRenewer, secret string,
) http.Handler {
    if renewer==nil || len(secret)<32 {
        panic("可信会话续租需要专用服务器验证器及独立强令牌")
    }
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Cache-Control","no-store")
        if r.Method!=http.MethodPost {
            w.Header().Set("Allow",http.MethodPost)
            writeError(w,http.StatusMethodNotAllowed,"METHOD_NOT_ALLOWED",nil)
            return
        }
        tokenFields:=strings.Fields(r.Header.Get("Authorization"))
        if len(tokenFields)!=2 || !strings.EqualFold(tokenFields[0],"Bearer") ||
            subtle.ConstantTimeCompare([]byte(tokenFields[1]),[]byte(secret))!=1 {
            writeError(w,http.StatusUnauthorized,"INVENTORY_TRUST_AUTH_REQUIRED",nil)
            return
        }
        media,_,err:=mime.ParseMediaType(r.Header.Get("Content-Type"))
        if err!=nil || !strings.EqualFold(media,"application/json") || r.URL.RawQuery!="" {
            writeError(w,http.StatusBadRequest,"INVALID_REQUEST",nil)
            return
        }
        r.Body=http.MaxBytesReader(w,r.Body,2048)
        var req trustedInventoryRenewRequest
        allowed:=[]string{"gameId","playerId","gameServerId","serverBootId",
            "sessionEpoch","connectionId","leaseSeconds"}
        if err:=gateway.DecodeOnlineJSON(r,&req,allowed,allowed);err!=nil {
            writeError(w,http.StatusBadRequest,"INVALID_REQUEST",nil)
            return
        }
        if req.LeaseSeconds<1 || req.LeaseSeconds>inventory.MaxInventorySessionLeaseSeconds ||
            req.SessionEpoch<1 || req.GameServerID=="" || req.ServerBootID=="" ||
            req.PlayerID=="" || req.GameID=="" || req.ConnectionID=="" {
            writeError(w,http.StatusBadRequest,"INVALID_REQUEST",nil)
            return
        }
        // 请求头由受控内网代理注入或专用服务器签发，并必须与请求租约完全匹配。
        if strings.TrimSpace(r.Header.Get("X-Game-Server-Id"))!=req.GameServerID ||
            strings.TrimSpace(r.Header.Get("X-Game-Server-Boot-Id"))!=req.ServerBootID {
            writeError(w,http.StatusForbidden,"INVENTORY_TRUST_DENIED",nil)
            return
        }
        lease:=inventory.TrustedPlayerLease{
            GameID:req.GameID, PlayerID:req.PlayerID,
            GameServerID:req.GameServerID,ServerBootID:req.ServerBootID,
            SessionEpoch:req.SessionEpoch,ConnectionID:req.ConnectionID,
        }
        authorityUntil,err:=renewer.RenewServerLease(r.Context(),lease,req.LeaseSeconds)
        if err!=nil {
            writeOnlineDomainError(w,err)
            return
        }
        writeJSON(w,http.StatusOK,map[string]any{
            "playerId":req.PlayerID, "sessionEpoch":req.SessionEpoch,
            "authorityUntilUtc":authorityUntil.UTC().Format("2006-01-02T15:04:05.000000000Z07:00"),
        })
    })
}
