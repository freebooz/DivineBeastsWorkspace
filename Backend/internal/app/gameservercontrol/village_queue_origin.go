package gameservercontrol

import (
	"context"
	"crypto/sha256"
	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
	"encoding/hex"
	"errors"
	"strings"
	"time"
)

// VillageQueueOriginInput由Village专用服务器对当前已准入Pawn采样。玩家只能触发采样，不能提供坐标。
type VillageQueueOriginInput struct {
	QueueTicketID string              `json:"queueTicketId"`       // 本轮报名的服务端票据ID。
	PlayerID      string              `json:"playerId"`            // 当前源服务器已验证玩家。
	SessionID     string              `json:"sessionId"`           // 当前源服务器已验证会话。
	SessionEpoch  uint64              `json:"sessionEpoch,string"` // 当前源服务器的已准入代次，以十进制文本保留精度。
	Spawn         gc.WorldReturnSpawn `json:"spawn"`               // 服务端已落地角色的真实变换。
}

// VillageQueueOriginRef只由Match根据本人在固定候选中的报名和锁定角色生成；不向客户端公开。
type VillageQueueOriginRef struct {
	QueueTicketID      string `json:"queueTicketId"`                       // 当前候选给本人的实际报名票据。
	SourceGameServerID string `json:"sourceGameServerId"`                  // 确认报名角色时的可信Village实例ID。
	SessionID          string `json:"sessionId"`                           // 本轮锁定的原源会话，允许新会话恢复同一角色后返程。
	CharacterID        string `json:"characterId"`                         // 本轮锁定的角色。
	SourceServerBootID string `json:"sourceServerBootId,omitempty"`        // Match锁定的源Boot，历史报名没有此字段。
	SourceSessionEpoch uint64 `json:"sourceSessionEpoch,omitempty,string"` // Match锁定的源准入代次。
}

// VillageQueueOriginAuthority只读核对Match报名、当前角色和源准入代次。
type VillageQueueOriginAuthority interface {
	ValidateVillageQueueOrigin(context.Context, matchmaking.VillageQueueOriginEnrollment) (bool, error)
}

// ConfigureVillageQueueOriginAuthority在服务启动前注入Match的唯一报名事实来源。
func (s *Service) ConfigureVillageQueueOriginAuthority(authority VillageQueueOriginAuthority) {
	s.villageQueueOriginAuthority = authority
}

// VillageQueueOriginRecord是首次受理的不可变位置，限定报名、玩家、源准入和地图。
type VillageQueueOriginRecord struct {
	Spawn        gc.WorldReturnSpawn // 可由目标地图进一步校验的落点。
	Presence     PlayerPresence      // 写入时已验证的源实例、启动代次、会话及逻辑世界。
	ExpiresAtUTC time.Time           // 有限保留窗口，缺失时沿用常规出生。
}

// MatchesPresence在仓储原子写入前复核同一源票和代次，避免迟到报告与迁出互相覆盖。
func (r VillageQueueOriginRecord) MatchesPresence(current PlayerPresence) bool {
	return current.PlayerID == r.Presence.PlayerID && current.SessionID == r.Presence.SessionID &&
		current.GameServerID == r.Presence.GameServerID && current.ServerBootID == r.Presence.ServerBootID &&
		current.TicketID == r.Presence.TicketID && current.SessionEpoch == r.Presence.SessionEpoch
}

// VillageQueueOriginStore在控制面已验证源连接后原子保存首次采样。生产与准入事实共用共享控制仓储。
type VillageQueueOriginStore interface {
	SaveVillageQueueOrigin(context.Context, string, VillageQueueOriginRecord, time.Duration) error
	GetVillageQueueOrigin(context.Context, string) (VillageQueueOriginRecord, bool, error)
}

func villageQueueOriginKey(queueID, playerID, sessionID string) string {
	digest := sha256.Sum256([]byte(strings.Join([]string{queueID, playerID, sessionID}, "\x00")))
	return hex.EncodeToString(digest[:])
}

func validVillageQueueTicketID(id string) bool {
	if len(id) > 128 || !(strings.HasPrefix(id, "matchmaking-") && len(id) > len("matchmaking-") ||
		strings.HasPrefix(id, "challenge-") && len(id) > len("challenge-")) {
		return false
	}
	for _, c := range id {
		if !(c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '-' || c == '_') {
			return false
		}
	}
	return true
}

// RecordVillageQueueOrigin只接受当前已准入的本人及同一Village启动代次；重复命令保留首次位置。
func (s *Service) RecordVillageQueueOrigin(ctx context.Context, serverID, bootID string, input VillageQueueOriginInput) error {
	store, ok := s.arenaLifecycle.(VillageQueueOriginStore)
	if !ok || !validVillageQueueTicketID(input.QueueTicketID) || !input.Spawn.IsValid() || input.SessionEpoch == 0 {
		return errors.New("VILLAGE_QUEUE_ORIGIN_INVALID: 无法采集排队位置")
	}
	presence, err := s.LookupPlayerPresence(ctx, input.PlayerID, input.SessionID)
	if err != nil {
		return err
	}
	role, _ := gc.RoleForExperience(presence.ExperienceID)
	if role != gc.RoleVillage || presence.GameServerID != serverID || presence.ServerBootID != bootID || presence.SessionEpoch != input.SessionEpoch || presence.MatchID != "" {
		return errors.New("VILLAGE_QUEUE_ORIGIN_FORBIDDEN: 当前准入不属于此Village服务器")
	}
	if s.villageQueueOriginAuthority == nil {
		return errors.New("VILLAGE_QUEUE_ORIGIN_FORBIDDEN: 缺少Match报名验证")
	}
	allowed, err := s.villageQueueOriginAuthority.ValidateVillageQueueOrigin(ctx, matchmaking.VillageQueueOriginEnrollment{
		QueueTicketID: input.QueueTicketID, PlayerID: presence.PlayerID, CharacterID: input.Spawn.CharacterID, SessionID: presence.SessionID,
		SourceGameServerID: presence.GameServerID, SourceExperienceID: presence.ExperienceID, SourceServerBootID: presence.ServerBootID, SourceSessionEpoch: presence.SessionEpoch,
	})
	if err != nil {
		return err
	}
	if !allowed {
		return errors.New("VILLAGE_QUEUE_ORIGIN_FORBIDDEN: 报名无效或与当前准入不匹配")
	}
	const retention = 2 * time.Hour // 包含排队、比赛与返程恢复窗口。
	record := VillageQueueOriginRecord{Spawn: input.Spawn, Presence: presence, ExpiresAtUTC: s.now().UTC().Add(retention)}
	return store.SaveVillageQueueOrigin(ctx, villageQueueOriginKey(input.QueueTicketID, input.PlayerID, input.SessionID), record, retention)
}

// villageQueueOrigin只按实际匹配的已锁定玩家、来源会话、角色和目标世界选择；旧报名或不可用仓储回退常规出生。
func (s *Service) villageQueueOrigin(ctx context.Context, input AllocateWorldTransferInput, now time.Time) *VillageQueueOriginRecord {
	store, ok := s.arenaLifecycle.(VillageQueueOriginStore)
	ref := input.VillageQueueOrigin
	if !ok || ref == nil || !validVillageQueueTicketID(ref.QueueTicketID) || ref.SourceServerBootID == "" || ref.SourceSessionEpoch == 0 {
		return nil
	}
	origin, found, err := store.GetVillageQueueOrigin(ctx, villageQueueOriginKey(ref.QueueTicketID, input.Transfer.PlayerID, ref.SessionID))
	if err != nil || !found || !now.Before(origin.ExpiresAtUTC) || !origin.Spawn.IsValid() || origin.Spawn.CharacterID != ref.CharacterID ||
		origin.Presence.PlayerID != input.Transfer.PlayerID || origin.Presence.SessionID != ref.SessionID || origin.Presence.GameServerID != ref.SourceGameServerID ||
		origin.Presence.ExperienceID != input.World.ExperienceID || origin.Presence.WorldID != input.World.WorldID ||
		origin.Presence.ServerBootID != ref.SourceServerBootID || origin.Presence.SessionEpoch != ref.SourceSessionEpoch {
		return nil
	}
	return &origin
}
