package playerdata

import (
	"context"
	"errors"
	"strings"
	"unicode/utf8"

	"divinebeasts/backend/internal/platform/apperror"
)

// IdempotentDisplayNameRepository是增量原子写端口；旧Repository实现无需改变。
// 实现必须先重放主体/操作/键对应的持久结果，再检查修订；资料与结果必须同事务。
type IdempotentDisplayNameRepository interface {
	UpdateDisplayNameIdempotent(ctx context.Context, playerID, displayName string, expectedRevision int64, key string) (Profile, error)
}

// ProfileInitializer仅由玩家资料所属服务的显式初始化用例调用，读取不触发建档。
type ProfileInitializer interface {
	EnsureProfile(ctx context.Context, playerID, gameID string) error
}

// RepositoryProbe必须真实探测持久依赖；缺少能力不得回落为固定成功。
type RepositoryProbe interface {
	Probe(ctx context.Context) error
}

// UpdateDisplayNameIdempotent仅允许更新名称；playerID必须由上游可信认证上下文提供。
// 名称trim后1..24个Unicode字符，revision非负，键非空且最多128字符。
// 同键同规范内容重放原结果，即使当前revision已增加；同键异内容或新键旧修订返回冲突。
// 取消保留context错误；取消/超时不保证远端未提交，重试必须使用原键和原请求。
func (s *Service) UpdateDisplayNameIdempotent(ctx context.Context, playerID, displayName string, expectedRevision int64, key string) (Profile, error) {
	if err := ctx.Err(); err != nil {
		return Profile{}, err
	}
	name, err := NormalizeDisplayNameUpdate(playerID, displayName, expectedRevision, key)
	if err != nil {
		return Profile{}, err
	}
	repo, ok := s.repo.(IdempotentDisplayNameRepository)
	if !ok {
		return Profile{}, unavailable()
	}
	profile, err := repo.UpdateDisplayNameIdempotent(ctx, playerID, name, expectedRevision, key)
	if err != nil {
		return Profile{}, classifyOnlineError(err)
	}
	return profile, nil
}

// EnsureProfile由所属服务建立默认资料；同玩家同游戏重复调用不重置已有字段或修订。
// 上游必须先完成真实身份校验；本用例不接受客户端自行选择其他玩家的身份。
func (s *Service) EnsureProfile(ctx context.Context, playerID, gameID string) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if !validIdentity(playerID) || !validIdentity(gameID) {
		return invalidRequest()
	}
	repo, ok := s.repo.(ProfileInitializer)
	if !ok {
		return unavailable()
	}
	return classifyOnlineError(repo.EnsureProfile(ctx, playerID, gameID))
}

// Probe只读探测真实仓储，不创建资料或执行迁移；依赖不可用返回SERVICE_UNAVAILABLE。
func (s *Service) Probe(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	repo, ok := s.repo.(RepositoryProbe)
	if !ok {
		return unavailable()
	}
	return classifyOnlineError(repo.Probe(ctx))
}

// NormalizeDisplayNameUpdate供领域与持久适配共用输入约束，防止直接仓储调用绕过校验。
// 只规范化可编辑名称；玩家身份与幂等键保持原值，不能trim后意外合并不同主体/键。
func NormalizeDisplayNameUpdate(playerID, displayName string, expectedRevision int64, key string) (string, error) {
	name := strings.TrimSpace(displayName)
	if !validIdentity(playerID) || !validIdentity(key) || utf8.RuneCountInString(key) > 128 ||
		!utf8.ValidString(name) || strings.ContainsRune(name, 0) || name == "" || utf8.RuneCountInString(name) > 24 || expectedRevision < 0 {
		return "", invalidRequest()
	}
	return name, nil
}

func validIdentity(value string) bool {
	return utf8.ValidString(value) && strings.TrimSpace(value) != "" && !strings.ContainsRune(value, 0)
}
func invalidRequest() error {
	return apperror.New("INVALID_REQUEST", "玩家资料请求参数非法", false)
}
func unavailable() error {
	return apperror.New("SERVICE_UNAVAILABLE", "玩家资料持久服务暂不可用", true)
}
func classifyOnlineError(err error) error {
	if err == nil {
		return nil
	}
	if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
		return err
	}
	var app *apperror.Error
	if errors.As(err, &app) {
		return app
	}
	return unavailable()
}
