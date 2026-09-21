//go:build productiondeps

package postgres

import (
	"context"
	"errors"
	"testing"

	"divinebeasts/backend/internal/modules/playerdata"
	"divinebeasts/backend/internal/platform/apperror"
)

func requirePlayerCode(t *testing.T, err error, code string) {
	t.Helper()
	var app *apperror.Error
	if !errors.As(err, &app) || app.Code != code {
		t.Fatalf("期望%s，实际%v", code, err)
	}
}

// 本测试不连接PG，只证明生产仓储对直接调用的输入/取消/缺失连接分类；不证明事务成功。
func TestOnlinePlayerRepositoryRejectsBeforeDatabase(t *testing.T) {
	repo := NewPlayerRepository(&Pool{})
	writer, ok := any(repo).(playerdata.IdempotentDisplayNameRepository)
	if !ok {
		t.Fatal("生产仓储缺少原子幂等更新能力")
	}
	_, err := writer.UpdateDisplayNameIdempotent(context.Background(), "p", "", 0, "k")
	requirePlayerCode(t, err, "INVALID_REQUEST")
	_, err = writer.UpdateDisplayNameIdempotent(context.Background(), "p", "名字", 0, "k")
	requirePlayerCode(t, err, "SERVICE_UNAVAILABLE")
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	_, err = writer.UpdateDisplayNameIdempotent(ctx, "p", "名字", 0, "k")
	if !errors.Is(err, context.Canceled) {
		t.Fatal("取消未保持", err)
	}
	probe, ok := any(repo).(playerdata.RepositoryProbe)
	if !ok {
		t.Fatal("生产仓储缺少真实探测能力")
	}
	requirePlayerCode(t, probe.Probe(context.Background()), "SERVICE_UNAVAILABLE")
}
