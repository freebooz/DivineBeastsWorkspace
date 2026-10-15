// 本文件复用严格平坦JSON解析器供内部HTTP/gRPC适配消费；不联网，不允许公开客户端转发actor。
package gateway

import (
	"divinebeasts/backend/internal/modules/matchmaking"
	"io"
	"net/http"
	"strings"
)

// DecodeMatchmakingActorJSON只供可信内部Envelope；公开Gateway必须由认证/角色服务构造上下文。
func DecodeMatchmakingActorJSON(payload string) (matchmaking.ActorContext, error) {
	var actor matchmaking.ActorContext
	request := &http.Request{Header: make(http.Header), Body: io.NopCloser(strings.NewReader(payload))}
	err := DecodeOnlineJSON(request, &actor, []string{"playerId", "sessionId", "characterId", "heroDefinitionId", "characterRevision", "sourceGameServerId", "sourceExperienceId"}, []string{"playerId", "sessionId", "characterId", "heroDefinitionId", "characterRevision", "sourceGameServerId", "sourceExperienceId"})
	return actor, err
}

// DecodeMatchmakingCommandJSON与公网使用相同字段、重复键和精确整数限制，拒绝缺失的逐人接受意图。
func DecodeMatchmakingCommandJSON(payload string) (matchmaking.Command, error) {
	var command matchmaking.Command
	request := &http.Request{Header: make(http.Header), Body: io.NopCloser(strings.NewReader(payload))}
	err := DecodeOnlineJSON(request, &command, []string{"action", "clientRequestId", "expectedRevision", "ticketId", "candidateId", "arenaModeId", "partyId", "regionId", "accept"}, []string{"action", "clientRequestId", "expectedRevision"})
	return command, err
}

// DecodeMatchmakingMigrationJSON禁止目标地址/旧票据，仅接受本人候选、方向和幂等身份。
func DecodeMatchmakingMigrationJSON(payload string) (matchmaking.MigrationRequest, error) {
	var command matchmaking.MigrationRequest
	request := &http.Request{Header: make(http.Header), Body: io.NopCloser(strings.NewReader(payload))}
	err := DecodeOnlineJSON(request, &command, []string{"candidateId", "direction", "clientRequestId"}, []string{"candidateId", "direction", "clientRequestId"})
	return command, err
}
