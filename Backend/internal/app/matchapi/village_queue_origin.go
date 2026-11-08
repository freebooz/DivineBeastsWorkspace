package matchapi

import (
	"context"
	"errors"
	"slices"
	"time"

	gc "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/matchmaking"
)

// MatchmakingQueueReader只读访问Match的原子队列，供同一报名事实在可信控制面核验。
type MatchmakingQueueReader interface {
	ReadMatchmaking(context.Context) (*matchmaking.QueueState, error)
}

// VillageQueueOriginAuthority从Match持久仓储的当前报名验证来源，只接受有效期内的本人资格。
type VillageQueueOriginAuthority struct{ reader MatchmakingQueueReader }

func NewVillageQueueOriginAuthority(reader MatchmakingQueueReader) VillageQueueOriginAuthority {
	return VillageQueueOriginAuthority{reader: reader}
}

func (s *Service) ValidateVillageQueueOrigin(ctx context.Context, proof matchmaking.VillageQueueOriginEnrollment) (bool, error) {
	reader, _ := s.parties.(MatchmakingQueueReader)
	return NewVillageQueueOriginAuthority(reader).ValidateVillageQueueOrigin(ctx, proof)
}

func (a VillageQueueOriginAuthority) ValidateVillageQueueOrigin(ctx context.Context, proof matchmaking.VillageQueueOriginEnrollment) (bool, error) {
	if a.reader == nil {
		return false, errors.New("MATCH_ATOMIC_STORE_REQUIRED: 缺少报名来源仓储")
	}
	queue, err := a.reader.ReadMatchmaking(ctx)
	if err != nil {
		return false, err
	}
	return villageQueueOriginEnrollmentActive(queue, proof, time.Now().UTC()), nil
}

func villageQueueOriginEnrollmentActive(queue *matchmaking.QueueState, proof matchmaking.VillageQueueOriginEnrollment, now time.Time) bool {
	role, _ := gc.RoleForExperience(proof.SourceExperienceID)
	if queue == nil || role != gc.RoleVillage || proof.QueueTicketID == "" || proof.SourceServerBootID == "" || proof.SourceSessionEpoch == 0 ||
		queue.Reservations[proof.PlayerID] != proof.QueueTicketID {
		return false
	}
	entry, found := queue.Entries[proof.QueueTicketID]
	if !found || !now.Before(entry.DeadlineUTC) || !slices.Contains(entry.Ticket.PartyMemberIDs, proof.PlayerID) {
		return false
	}
	switch entry.Phase {
	case matchmaking.PhaseSearching, matchmaking.PhaseConfirmation, matchmaking.PhaseAllocating, matchmaking.PhaseTransferReady:
	default:
		return false
	}
	actor, found := queue.Contexts[proof.PlayerID]
	return found && actor.QueueTicketID == proof.QueueTicketID && actor.PlayerID == proof.PlayerID && actor.CharacterID == proof.CharacterID &&
		actor.SessionID == proof.SessionID && actor.SourceGameServerID == proof.SourceGameServerID && actor.SourceExperienceID == proof.SourceExperienceID &&
		actor.SourceServerBootID == proof.SourceServerBootID && actor.SourceSessionEpoch == proof.SourceSessionEpoch
}
