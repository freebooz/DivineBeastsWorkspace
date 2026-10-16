// 本文件把控制面已接受的本人位置注入匹配意图；不接受客户端SourceServerID，安全返回只记录体验类别并重新分配。
package gateway

import (
	"context"
	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
)

// MatchmakingWorldPresence是后端内部中立位置摘要，不包含玩家凭据或旧地址。
type MatchmakingWorldPresence struct {
	GameServerID string // 控制面已确认当前实例。
	ExperienceID string // 当前真实Village/OpenWorld体验，用于返回重新分配。
}

// MatchmakingPresencePort由同一控制面客户端实现，仅允许认证会话本人查询。
type MatchmakingPresencePort interface {
	LookupMatchmakingPresence(context.Context, string, string) (MatchmakingWorldPresence, error)
}

// attachMatchmakingPresence只允许已进入可用常驻世界的会话报名/确认；故障返回不走此门禁，避免源服务器崩溃后无法返回。
func (a *api) attachMatchmakingPresence(ctx context.Context, actor *matchmaking.ActorContext) error {
	port, ok := a.worldEntry.(MatchmakingPresencePort)
	if !ok {
		return ServiceError("SERVICE_UNAVAILABLE")
	}
	value, err := port.LookupMatchmakingPresence(ctx, actor.PlayerID, actor.SessionID)
	if err != nil {
		if err.Error() == "PLAYER_NOT_IN_WORLD" {
			return ServiceError("MATCH_WORLD_REQUIRED")
		}
		return err
	}
	role, known := gameservercontract.RoleForExperience(value.ExperienceID)
	if !known || role == gameservercontract.RoleMainArena || value.GameServerID == "" {
		return ServiceError("MATCH_WORLD_REQUIRED")
	}
	actor.SourceGameServerID = value.GameServerID
	actor.SourceExperienceID = value.ExperienceID
	return nil
}
