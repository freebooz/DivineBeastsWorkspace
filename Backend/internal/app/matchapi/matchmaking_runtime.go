// 本文件为匹配应用连接持久编排运行端口；公共快照与秘密迁移严格分离，UI没有取票/改地址权威。
package matchapi

import (
	"context"
	"divinebeasts/backend/internal/modules/matchmaking"
	"errors"
)

// MatchmakingRuntime由MatchService正式组合根注入，拥有跨服务步骤恢复、签票/撤销及安全返回。
type MatchmakingRuntime interface {
	ProcessCandidate(context.Context, matchmaking.Candidate) error
	GetMigration(context.Context, matchmaking.ActorContext, matchmaking.MigrationRequest) (matchmaking.Migration, error)
}

// AttachMatchmakingRuntime在启动之前装配一次，不允许请求期间替换或静默安装内存生产替身。
func (s *Service) AttachMatchmakingRuntime(runtime MatchmakingRuntime) error {
	if runtime == nil || s.matchmakingRuntime != nil {
		return errors.New("MATCH_RUNTIME_INVALID: 编排器缺失或已经装配")
	}
	s.matchmakingRuntime = runtime
	return nil
}

// RunConfiguredMatchmaking保证组合根真的启动已装配消费者；未装配直接失败，不留下看似可用的空后台。
func (s *Service) RunConfiguredMatchmaking(ctx context.Context, onError func(error)) error {
	if s.matchmakingRuntime == nil {
		return errors.New("MATCH_RUNTIME_NOT_CONFIGURED: 未装配持久编排器")
	}
	return s.RunMatchmaking(ctx, s.matchmakingRuntime.ProcessCandidate, onError)
}

// GetMatchmakingMigration仅为候选中本人已验证会话/角色取票；票据只瞬时交给传输层，不进入观察快照。
func (s *Service) GetMatchmakingMigration(ctx context.Context, actor matchmaking.ActorContext, request matchmaking.MigrationRequest) (matchmaking.Migration, error) {
	if !validMatchID(actor.PlayerID) || !validMatchID(actor.SessionID) || !validMatchID(actor.CharacterID) || !validMatchID(request.CandidateID) || !validMatchID(request.ClientRequestID) || (request.Direction != "arena" && request.Direction != "return") {
		return matchmaking.Migration{}, errors.New("MATCH_COMMAND_INVALID: 迁移身份或方向无效")
	}
	if s.matchmakingRuntime == nil {
		return matchmaking.Migration{}, errors.New("MATCH_RUNTIME_NOT_CONFIGURED: 真实迁移编排未装配")
	}
	repo, ok := s.parties.(AtomicMatchmakingRepository)
	if !ok {
		return matchmaking.Migration{}, errors.New("MATCH_ATOMIC_STORE_REQUIRED: 未装配事务")
	}
	queue, err := repo.ReadMatchmaking(ctx)
	if err != nil {
		return matchmaking.Migration{}, err
	}
	candidate, found := queue.Candidates[request.CandidateID]
	if !found {
		return matchmaking.Migration{}, errors.New("MATCH_NOT_FOUND: 候选不存在")
	}
	member := false
	for _, slot := range candidate.Roster {
		member = member || slot.PlayerID == actor.PlayerID
	}
	if !member {
		return matchmaking.Migration{}, errors.New("MATCH_FORBIDDEN: 只能取本人迁移")
	}
	locked, found := queue.Contexts[actor.PlayerID]
	if !found || locked.CharacterID != actor.CharacterID || locked.HeroDefinitionID != actor.HeroDefinitionID {
		return matchmaking.Migration{}, errors.New("MATCH_CHARACTER_REQUIRED: 必须使用本局已确认角色")
	}
	if request.Direction == "arena" && candidate.Phase != matchmaking.PhaseTransferReady && candidate.Phase != matchmaking.PhaseInMatch {
		return matchmaking.Migration{}, errors.New("MATCH_PHASE_CONFLICT: 全员签票尚未提交")
	}
	if request.Direction == "return" && candidate.Phase != matchmaking.PhaseReturning {
		return matchmaking.Migration{}, errors.New("MATCH_PHASE_CONFLICT: 可信结果尚未确认")
	}
	return s.matchmakingRuntime.GetMigration(ctx, actor, request)
}
