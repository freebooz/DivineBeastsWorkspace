package playerdata

import (
	"context"
	"errors"
	"strings"
	"testing"

	"divinebeasts/backend/internal/platform/apperror"
)

// onlineUseCases用于在实现前断言真实Service缺少能力，而不是依赖编译错误作为红灯。
type onlineUseCases interface {
	UpdateDisplayNameIdempotent(context.Context, string, string, int64, string) (Profile, error)
	EnsureProfile(context.Context, string, string) error
	Probe(context.Context) error
}

// onlineRepositoryRecorder仅替换外部持久端口；不模拟或证明PG事务与幂等成功。
type onlineRepositoryRecorder struct {
	*MemoryRepository
	playerID, name, key, gameID string
	revision                    int64
	updates, ensures, probes    int
	err                         error
}

func (r *onlineRepositoryRecorder) UpdateDisplayNameIdempotent(_ context.Context, playerID, name string, revision int64, key string) (Profile, error) {
	r.updates++
	r.playerID, r.name, r.revision, r.key = playerID, name, revision, key
	return Profile{PlayerID: playerID, DisplayName: name, Revision: revision + 1}, r.err
}
func (r *onlineRepositoryRecorder) EnsureProfile(_ context.Context, playerID, gameID string) error {
	r.ensures++
	r.playerID, r.gameID = playerID, gameID
	return r.err
}
func (r *onlineRepositoryRecorder) Probe(context.Context) error { r.probes++; return r.err }

func requireOnline(t *testing.T, service *Service) onlineUseCases {
	t.Helper()
	api, ok := any(service).(onlineUseCases)
	if !ok {
		t.Fatal("现有Service尚未提供三个Online精确用例签名")
	}
	return api
}
func requireCode(t *testing.T, err error, code string) {
	t.Helper()
	var app *apperror.Error
	if !errors.As(err, &app) || app.Code != code {
		t.Fatalf("错误分类应为%s，实际%v", code, err)
	}
}

// 防止新用例回落到Get+Save（无法原子持久幂等），并验证只向持久端口传规范名称。
func TestOnlineUpdateNormalizesAndRequiresAtomicCapability(t *testing.T) {
	repo := &onlineRepositoryRecorder{MemoryRepository: NewMemoryRepository()}
	api := requireOnline(t, NewService(repo))
	profile, err := api.UpdateDisplayNameIdempotent(context.Background(), "player-a", "  新名称\t", 7, "key-a")
	if err != nil || profile.DisplayName != "新名称" || repo.name != "新名称" || repo.playerID != "player-a" || repo.revision != 7 || repo.key != "key-a" || repo.updates != 1 {
		t.Fatalf("规范化或原子端口参数不正确: %+v %+v %v", profile, repo, err)
	}
	_, err = requireOnline(t, NewService(NewMemoryRepository())).UpdateDisplayNameIdempotent(context.Background(), "a", "名称", 0, "k")
	requireCode(t, err, "SERVICE_UNAVAILABLE")
}

// 非法输入不能触达持久端口；Unicode按字符数约束，不按UTF-8字节数限制姓名。
func TestOnlineUpdateInputValidation(t *testing.T) {
	tests := []struct {
		name, player, display, key string
		revision                   int64
	}{
		{"empty-player", "", "姓名", "k", 0},
		{"blank-player", " ", "姓名", "k", 0},
		{"empty-name", "p", " \t", "k", 0},
		{"long-name", "p", strings.Repeat("名", 25), "k", 0},
		{"bad-utf8", "p", string([]byte{255}), "k", 0},
		{"nul-name", "p", "名\x00称", "k", 0},
		{"negative-revision", "p", "姓名", "k", -1},
		{"empty-key", "p", "姓名", "", 0},
		{"blank-key", "p", "姓名", " \t", 0},
		{"long-key", "p", "姓名", strings.Repeat("k", 129), 0},
		{"nul-key", "p", "姓名", "k\x00", 0},
	}
	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			repo := &onlineRepositoryRecorder{MemoryRepository: NewMemoryRepository()}
			_, err := requireOnline(t, NewService(repo)).UpdateDisplayNameIdempotent(context.Background(), tc.player, tc.display, tc.revision, tc.key)
			requireCode(t, err, "INVALID_REQUEST")
			if repo.updates != 0 {
				t.Fatal("非法输入触达了仓储")
			}
		})
	}
	repo := &onlineRepositoryRecorder{MemoryRepository: NewMemoryRepository()}
	_, err := requireOnline(t, NewService(repo)).UpdateDisplayNameIdempotent(context.Background(), "p", strings.Repeat("名", 24), 0, strings.Repeat("k", 128))
	if err != nil {
		t.Fatal("合法边界被拒绝", err)
	}
}

func TestOnlineErrorsDoNotExposeInfrastructureDetails(t *testing.T) {
	repo := &onlineRepositoryRecorder{MemoryRepository: NewMemoryRepository(), err: errors.New("private database connection details")}
	api := requireOnline(t, NewService(repo))
	_, err := api.UpdateDisplayNameIdempotent(context.Background(), "p", "名字", 1, "k")
	requireCode(t, err, "SERVICE_UNAVAILABLE")
	if strings.Contains(err.Error(), "private") {
		t.Fatal("泄漏底层错误")
	}
	for _, code := range []string{"PLAYER_DATA_CONFLICT", "IDEMPOTENCY_CONFLICT", "PLAYER_PROFILE_NOT_FOUND"} {
		repo.err = apperror.New(code, "稳定领域错误", false)
		_, err = api.UpdateDisplayNameIdempotent(context.Background(), "p", "名字", 1, "k")
		requireCode(t, err, code)
	}
}

func TestOnlineEnsureProbeAndCancellation(t *testing.T) {
	repo := &onlineRepositoryRecorder{MemoryRepository: NewMemoryRepository()}
	service := NewService(repo)
	api := requireOnline(t, service)
	if err := api.EnsureProfile(context.Background(), "p", "game"); err != nil || repo.ensures != 1 || repo.gameID != "game" {
		t.Fatal("未调用所属仓储初始化", err)
	}
	requireCode(t, api.EnsureProfile(context.Background(), "p", ""), "INVALID_REQUEST")
	if err := api.Probe(context.Background()); err != nil || repo.probes != 1 {
		t.Fatal("未真实探测依赖", err)
	}
	repo.err = errors.New("offline")
	requireCode(t, api.Probe(context.Background()), "SERVICE_UNAVAILABLE")
	_, _ = service.GetProfile(context.Background(), "missing")
	if repo.ensures != 1 {
		t.Fatal("Get隐式初始化了资料")
	}
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	_, err := api.UpdateDisplayNameIdempotent(ctx, "p", "姓名", 0, "k")
	if !errors.Is(err, context.Canceled) || repo.updates != 0 {
		t.Fatal("取消未保留或仍触达仓储", err)
	}
	if !errors.Is(api.EnsureProfile(ctx, "p", "g"), context.Canceled) {
		t.Fatal("初始化吞掉取消")
	}
	if !errors.Is(api.Probe(ctx), context.Canceled) {
		t.Fatal("探测吞掉取消")
	}
}

// 新Online读取链沿用GetProfile，错误必须可供传输层区分400/404，且不隐式建档。
func TestOnlineReadClassifiesWithoutCreating(t *testing.T) {
	service := NewService(NewMemoryRepository())
	_, err := service.GetProfile(context.Background(), " ")
	requireCode(t, err, "INVALID_REQUEST")
	_, err = service.GetProfile(context.Background(), "missing")
	requireCode(t, err, "PLAYER_PROFILE_NOT_FOUND")
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	_, err = service.GetProfile(ctx, "p")
	if !errors.Is(err, context.Canceled) {
		t.Fatal("读取吞掉取消", err)
	}
}
