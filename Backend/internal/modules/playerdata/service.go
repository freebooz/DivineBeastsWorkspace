// Package playerdata（玩家数据领域）管理玩家长期资料与乐观并发版本。
package playerdata

import (
	"context"
	"errors"
	"strings"
	"sync"
	"unicode/utf8"

	"divinebeasts/backend/internal/platform/apperror"
)

// Profile（玩家资料）只保存跨局长期业务数据，不保存当前生命值、Buff、技能冷却等实时Gameplay状态。
type Profile struct {
	PlayerID          string   // PlayerID（玩家ID）。
	GameID            string   // GameID（游戏ID）。
	DisplayName       string   // DisplayName（显示名称）。
	DataVersion       int      // DataVersion（数据结构版本）。
	Revision          int64    // Revision（乐观并发修订版本）。
	TutorialCompleted bool     // TutorialCompleted（是否完成新手教学）。
	DefaultWorldID    string   // DefaultWorldID（默认世界ID）。
	OwnedCharacterIDs []string // OwnedCharacterIDs（已拥有角色ID列表）。
}

// Repository（玩家资料仓储接口）隔离PostgreSQL/sqlc具体实现。
type Repository interface {
	Get(ctx context.Context, playerID string) (Profile, error)
	Save(ctx context.Context, profile Profile, expectedRevision int64) (Profile, error)
}

// Service（玩家资料领域服务）封装Profile修改规则。
type Service struct{ repo Repository }

// NewService（创建玩家资料服务）创建领域服务。
func NewService(repo Repository) *Service { return &Service{repo: repo} }

// GetProfile（获取玩家资料）按PlayerID返回长期资料只读快照。
func (s *Service) GetProfile(ctx context.Context, playerID string) (Profile, error) {
	if err := ctx.Err(); err != nil {
		return Profile{}, err
	}
	if !validIdentity(playerID) {
		return Profile{}, invalidRequest()
	}
	if s.repo == nil {
		return Profile{}, unavailable()
	}
	profile, err := s.repo.Get(ctx, playerID)
	if err != nil {
		return Profile{}, classifyOnlineError(err)
	}
	return profile, nil
}

// UpdateDisplayName（更新显示名称）使用Revision实现Optimistic Concurrency（乐观并发控制）。
func (s *Service) UpdateDisplayName(ctx context.Context, playerID, displayName string, expectedRevision int64) (Profile, error) {
	name := strings.TrimSpace(displayName)
	if name == "" || utf8.RuneCountInString(name) > 24 {
		return Profile{}, errors.New("显示名称必须为1到24个字符")
	}
	profile, err := s.repo.Get(ctx, playerID)
	if err != nil {
		return Profile{}, err
	}
	profile.DisplayName = name
	return s.repo.Save(ctx, profile, expectedRevision)
}

// MemoryRepository（内存玩家资料仓储）用于单元测试和本地开发。
type MemoryRepository struct {
	mu    sync.RWMutex
	items map[string]Profile
}

// NewMemoryRepository（创建内存仓储）创建线程安全玩家资料仓储。
func NewMemoryRepository() *MemoryRepository { return &MemoryRepository{items: map[string]Profile{}} }

// Seed（预置资料）仅用于测试或本地开发初始化。
func (r *MemoryRepository) Seed(profile Profile) {
	r.mu.Lock()
	defer r.mu.Unlock()
	r.items[profile.PlayerID] = cloneProfile(profile)
}

// Get（获取资料）按PlayerID读取玩家资料。
func (r *MemoryRepository) Get(_ context.Context, playerID string) (Profile, error) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	profile, ok := r.items[playerID]
	if !ok {
		return Profile{}, apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
	}
	return cloneProfile(profile), nil
}

// Save（保存资料）仅当Revision仍等于expectedRevision时更新，防止并发覆盖。
func (r *MemoryRepository) Save(_ context.Context, profile Profile, expectedRevision int64) (Profile, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	current, ok := r.items[profile.PlayerID]
	if !ok {
		return Profile{}, errors.New("玩家资料不存在")
	}
	if current.Revision != expectedRevision {
		return Profile{}, errors.New("PLAYER_DATA_CONFLICT: 玩家资料Revision冲突")
	}
	profile.Revision = expectedRevision + 1
	r.items[profile.PlayerID] = cloneProfile(profile)
	return cloneProfile(profile), nil
}

func cloneProfile(profile Profile) Profile {
	profile.OwnedCharacterIDs = append([]string(nil), profile.OwnedCharacterIDs...)
	return profile
}
