// 本文件把可信比赛编排端口映射到已有内部HTTP控制面；Bearer只进入请求头，所有调用受Context取消约束。
package httpadapter

import (
	"context"
	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// AllocateMainArenaContext不在客户端选实例，服务端按不可拆分固定名单和真实容量分配。
func (c *GameServerControlClient) AllocateMainArenaContext(ctx context.Context, input gameservercontrol.AllocateMainArenaInput) (gameservercontract.Assignment, error) {
	var value gameservercontract.Assignment
	err := c.postAuthorized(ctx, "/internal/v1/gameservers/allocate-mainarena", input, &value)
	return value, err
}

// IssueTransferContext按稳定TicketID恢复同一短期票据，不能重新生成随机准入身份掩盖未知响应。
func (c *GameServerControlClient) IssueTransferContext(ctx context.Context, input gameservercontrol.IssueTransferInput) (servertransfer.Ticket, error) {
	var value servertransfer.Ticket
	err := c.postAuthorized(ctx, "/internal/v1/gameservers/issue-transfer", input, &value)
	return value, err
}

// GetArenaStatus读取控制面真实Boot/心跳/结果，不把UI本地倒计时当成已入场。
func (c *GameServerControlClient) GetArenaStatus(ctx context.Context, id string) (gameservercontrol.ArenaStatus, error) {
	return c.QueryArenaStatus(ctx, id)
}

// AllocateWorldTransfer每次按来源逻辑体验重新找安全实例；不信任历史连接地址。
func (c *GameServerControlClient) AllocateWorldTransfer(ctx context.Context, input gameservercontrol.AllocateWorldTransferInput) (gameservercontrol.WorldTransferResult, error) {
	request := map[string]any{"world": input.World, "ticketId": input.Transfer.TicketID, "gameId": input.Transfer.GameID, "playerId": input.Transfer.PlayerID, "sessionId": input.Transfer.SessionID, "sourceGameServerId": input.Transfer.SourceGameServerID, "ttlMilliseconds": input.Transfer.TTL.Milliseconds()}
	var result gameservercontrol.WorldTransferResult
	err := c.postAuthorized(ctx, "/internal/v1/gameservers/allocate-world-transfer", request, &result)
	return result, err
}

var _ matchservice.RuntimeControl = (*GameServerControlClient)(nil)
