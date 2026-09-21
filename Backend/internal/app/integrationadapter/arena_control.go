// Package integrationadapter（应用集成适配器）提供同一进程内的应用端口桥接，主要用于本地联调和集成测试。
// 正式多进程部署可把本适配器替换为gRPC Client（gRPC客户端），MatchService领域编排无需修改。
package integrationadapter

import (
	"errors"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/app/matchservice"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/servertransfer"
)

// ArenaControl（竞技场控制进程内适配器）把MatchService端口请求映射到GameServerControlService应用接口。
type ArenaControl struct {
	service *gameservercontrol.Service
}

// NewArenaControl（创建竞技场控制适配器）注入GameServerControlService应用服务。
func NewArenaControl(service *gameservercontrol.Service) *ArenaControl {
	if service == nil {
		panic("GameServerControlService不能为空")
	}
	return &ArenaControl{service: service}
}

// AllocateMainArena（分配主竞技场）把MatchService请求转换为GameServerControlService输入。
func (a *ArenaControl) AllocateMainArena(req matchservice.ArenaAllocationRequest) (gameservercontract.Assignment, error) {
	if req.TeamSize <= 0 || req.TotalPlayers != req.TeamSize*2 {
		return gameservercontract.Assignment{}, errors.New("竞技场分配人数参数无效")
	}
	return a.service.AllocateMainArena(gameservercontrol.AllocateMainArenaInput{
		MatchID: req.MatchID, ArenaModeID: req.ArenaModeID, MapID: req.MapID, RegionID: req.RegionID, Roster: req.Roster,
	})
}

// IssuePlayerTransfer（签发玩家迁移票据）把MatchService玩家会话映射到GameServerControlService迁移服务。
func (a *ArenaControl) IssuePlayerTransfer(req matchservice.PlayerTransferRequest) (servertransfer.Ticket, error) {
	return a.service.IssueTransfer(gameservercontrol.IssueTransferInput{
		TicketID: req.TicketID, AssignmentID: req.AssignmentID, GameID: req.GameID, PlayerID: req.PlayerID, SessionID: req.SessionID,
		SourceGameServerID: req.SourceGameServerID, DestinationGameServerID: req.DestinationGameServerID,
		DestinationWorldID: req.DestinationWorldID, DestinationExperienceID: req.DestinationExperienceID, MatchID: req.MatchID, TTL: req.TTL,
	})
}
