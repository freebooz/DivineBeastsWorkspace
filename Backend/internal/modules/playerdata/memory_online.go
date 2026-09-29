package playerdata

import (
	"context"

	"divinebeasts/backend/internal/platform/apperror"
)

// memoryDisplayNameRecord（内存名称幂等记录）用于本地联调和单元测试重放。
// 生产环境对应online_profile_idempotency表；这里保持相同的“先重放、后校验Revision”语义。
type memoryDisplayNameRecord struct {
	DisplayName      string
	ExpectedRevision int64
	Result           Profile
}

// EnsureProfile（内存建档）与生产PostgreSQL语义一致：同玩家同游戏重复调用不覆盖已有字段。
func (r *MemoryRepository) EnsureProfile(_ context.Context, playerID, gameID string) error {
	if !validIdentity(playerID) || !validIdentity(gameID) {
		return invalidRequest()
	}

	r.mu.Lock()
	defer r.mu.Unlock()
	if r.items == nil {
		r.items = map[string]Profile{}
	}
	if current, exists := r.items[playerID]; exists {
		if current.GameID != gameID {
			return apperror.New("PLAYER_DATA_CONFLICT", "玩家资料已绑定其他游戏", false)
		}
		return nil
	}
	r.items[playerID] = Profile{
		PlayerID:       playerID,
		GameID:         gameID,
		DisplayName:    "新玩家",
		DataVersion:    1,
		Revision:       1,
		DefaultWorldID: "World.OpenWorld.Hub",
	}
	return nil
}

// Probe（内存仓储探测）不制造假外部依赖；内存实现自身始终可用。
// 生产装配不会使用该实现，因此这里仅服务本地联调和单元测试。
func (r *MemoryRepository) Probe(ctx context.Context) error {
	return ctx.Err()
}

// UpdateDisplayNameIdempotent（内存名称幂等更新）与生产仓储保持同键重放语义。
func (r *MemoryRepository) UpdateDisplayNameIdempotent(
	_ context.Context,
	playerID, displayName string,
	expectedRevision int64,
	key string,
) (Profile, error) {
	name, err := NormalizeDisplayNameUpdate(playerID, displayName, expectedRevision, key)
	if err != nil {
		return Profile{}, err
	}

	r.mu.Lock()
	defer r.mu.Unlock()
	if r.displayNameRecords == nil {
		r.displayNameRecords = map[string]memoryDisplayNameRecord{}
	}
	recordKey := playerID + "\x00" + key
	if previous, exists := r.displayNameRecords[recordKey]; exists {
		if previous.DisplayName != name || previous.ExpectedRevision != expectedRevision {
			return Profile{}, apperror.New("IDEMPOTENCY_CONFLICT", "幂等键已绑定其他资料更新请求", false)
		}
		return cloneProfile(previous.Result), nil
	}

	profile, exists := r.items[playerID]
	if !exists {
		return Profile{}, apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
	}
	if profile.Revision != expectedRevision {
		return Profile{}, apperror.New("PLAYER_DATA_CONFLICT", "玩家资料Revision冲突", false)
	}
	profile.DisplayName = name
	profile.Revision++
	r.items[playerID] = cloneProfile(profile)
	r.displayNameRecords[recordKey] = memoryDisplayNameRecord{
		DisplayName:      name,
		ExpectedRevision: expectedRevision,
		Result:           cloneProfile(profile),
	}
	return cloneProfile(profile), nil
}
