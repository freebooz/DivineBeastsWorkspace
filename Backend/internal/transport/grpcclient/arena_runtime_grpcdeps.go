//go:build grpcdeps

// 本文件将可取消的持久比赛步骤映射到经过Bearer认证的内部gRPC生命周期桥；不记录秘密正文。
package grpcclient

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/gateway"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	gameservercontrolv1 "divinebeasts/backend/internal/generated/gameservercontrol/v1"
	"divinebeasts/backend/internal/modules/servertransfer"
	"encoding/json"
	"google.golang.org/grpc"
	"google.golang.org/grpc/metadata"
)

// NewRuntimeControlClient只供Match组合根；Gateway应使用带默认区域的WorldEntryClient。
func NewRuntimeControlClient(conn grpc.ClientConnInterface, bearer string) *ArenaControlClient {
	return &ArenaControlClient{client: gameservercontrolv1.NewGameServerControlInternalServiceClient(conn), runtimeBearer: bearer}
}

// runtimeCall每次转发上游租约Context，并只把内部凭据放进metadata；应用失败保持安全稳定码。
func (c *ArenaControlClient) runtimeCall(ctx context.Context, action string, input, output any) error {
	if c.runtimeBearer == "" {
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	payload, err := json.Marshal(input)
	if err != nil {
		return err
	}
	ctx = metadata.AppendToOutgoingContext(ctx, "authorization", "Bearer "+c.runtimeBearer)
	response, err := c.client.ArenaLifecycle(ctx, &gameservercontrolv1.ArenaLifecycleRequest{Action: action, InputJson: string(payload)})
	if err != nil {
		return err
	}
	if !response.GetSuccess() {
		return gateway.ServiceError(response.GetErrorCode())
	}
	if err = json.Unmarshal([]byte(response.GetResultJson()), output); err != nil {
		return gateway.ServiceError("SERVICE_UNAVAILABLE")
	}
	return nil
}

// AllocateMainArenaContext保留固定名单/规则元数据，旧非持久端口不参与新完整流程。
func (c *ArenaControlClient) AllocateMainArenaContext(ctx context.Context, input gameservercontrol.AllocateMainArenaInput) (gameservercontract.Assignment, error) {
	var value gameservercontract.Assignment
	err := c.runtimeCall(ctx, "allocate", input, &value)
	return value, err
}

// IssueTransferContext恢复稳定签票意图，不在客户端生成权威准入。
func (c *ArenaControlClient) IssueTransferContext(ctx context.Context, input gameservercontrol.IssueTransferInput) (servertransfer.Ticket, error) {
	var value servertransfer.Ticket
	err := c.runtimeCall(ctx, "issue", input, &value)
	return value, err
}

// GetArenaStatus查询真实实例/结果和位置，只有后端编排器读取。
func (c *ArenaControlClient) GetArenaStatus(ctx context.Context, id string) (gameservercontrol.ArenaStatus, error) {
	var value gameservercontrol.ArenaStatus
	err := c.runtimeCall(ctx, "status", map[string]string{"matchId": id}, &value)
	return value, err
}

// CancelArena只停止本比赛准入和排空，人数门禁仍由控制面执行。
func (c *ArenaControlClient) CancelArena(ctx context.Context, id string) error {
	var value struct{ Accepted bool }
	return c.runtimeCall(ctx, "cancel", map[string]string{"matchId": id}, &value)
}

// FinalizeArena在真实迁出后回收，不能由客户端超时强行释放。
func (c *ArenaControlClient) FinalizeArena(ctx context.Context, id string) error {
	var value struct{ Accepted bool }
	return c.runtimeCall(ctx, "finalize", map[string]string{"matchId": id}, &value)
}

// RevokeTransfer撤销已提交的稳定意图，覆盖未知签票响应。
func (c *ArenaControlClient) RevokeTransfer(ctx context.Context, id string) error {
	var value struct{ Accepted bool }
	return c.runtimeCall(ctx, "revoke", map[string]string{"ticketId": id}, &value)
}

// AllocateWorldTransfer重新选择合法返回实例并签票，不使用历史地址。
func (c *ArenaControlClient) AllocateWorldTransfer(ctx context.Context, input gameservercontrol.AllocateWorldTransferInput) (gameservercontrol.WorldTransferResult, error) {
	var value gameservercontrol.WorldTransferResult
	err := c.runtimeCall(ctx, "worldTransfer", input, &value)
	return value, err
}

// LookupMatchmakingPresence验证当前会话已被真实世界接受，Gateway不会信任玩家自报来源。
func (c *ArenaControlClient) LookupMatchmakingPresence(ctx context.Context, player, session string) (gateway.MatchmakingWorldPresence, error) {
	var value gameservercontrol.PlayerPresence
	err := c.runtimeCall(ctx, "presence", map[string]string{"playerId": player, "sessionId": session}, &value)
	if err != nil {
		return gateway.MatchmakingWorldPresence{}, err
	}
	if value.PlayerID != player || value.SessionID != session || value.GameServerID == "" || value.ExperienceID == "" {
		return gateway.MatchmakingWorldPresence{}, gateway.ServiceError("MATCH_WORLD_REQUIRED")
	}
	return gateway.MatchmakingWorldPresence{GameServerID: value.GameServerID, ExperienceID: value.ExperienceID}, nil
}

var _ matchservice.RuntimeControl = (*ArenaControlClient)(nil)
var _ gateway.MatchmakingPresencePort = (*ArenaControlClient)(nil)
