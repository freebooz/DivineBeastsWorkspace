//go:build sessionintegration

package postgresadmission

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"errors"
	"fmt"
	"os"
	"strings"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/jackc/pgx/v5/pgconn"
	_ "github.com/jackc/pgx/v5/stdlib"
)

// 只允许独占临时容器中的指定测试库；测试夹具的授权/Ready行不是UE启动证据。
func integrationStore(t *testing.T) (*Store, *sql.DB) {
	t.Helper()
	if os.Getenv("SESSION_INTEGRATION_ISOLATED") != "1" {
		t.Fatal("必须由隔离测试脚本设置SESSION_INTEGRATION_ISOLATED")
	}
	database, err := sql.Open("pgx", "postgres://postgres@127.0.0.1:5432/session_integration?sslmode=disable")
	if err != nil {
		t.Fatal(err)
	}
	t.Cleanup(func() { _ = database.Close() })
	database.SetMaxOpenConns(24)
	store, err := NewStore(database)
	if err != nil {
		t.Fatal(err)
	}
	return store, database
}

func execute(t *testing.T, database *sql.DB, query string, arguments ...any) {
	t.Helper()
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if _, err := database.ExecContext(ctx, query, arguments...); err != nil {
		var databaseError *pgconn.PgError
		if errors.As(err, &databaseError) {
			t.Logf("SQL position=%d internalPosition=%d context=%s", databaseError.Position, databaseError.InternalPosition, databaseError.Where)
		}
		t.Fatal(err)
	}
}

func prepare(t *testing.T, database *sql.DB, prefix string, capacity int) {
	t.Helper()
	execute(t, database, `INSERT INTO session_authorizations VALUES($1,'game',$2,'auth','owned-character',NOW()+INTERVAL '10 minutes',FALSE)`, prefix, prefix)
	execute(t, database, `INSERT INTO session_instances VALUES($1,'boot','game','world','1',$2,TRUE,NOW()+INTERVAL '10 minutes')`, prefix, capacity)
}

func reservation(prefix string) (Reservation, []byte) {
	proof := []byte("test-only-never-used-for-admission-" + prefix)
	return Reservation{ID: prefix, OperationID: "operation-" + prefix, AuthorizationID: prefix, InstanceID: prefix, BootID: "boot", AttemptID: "attempt", CredentialDigest: sha256.Sum256(proof), ProtocolVersion: "1", TTLSeconds: 30}, proof
}

func connection(request Reservation, identifier string) Connection {
	return Connection{ReservationID: request.ID, InstanceID: request.InstanceID, BootID: request.BootID, ConnectionID: identifier}
}

// TestMigrate只执行在新容器中；没有清空既有数据库或重放全部迁移的行为。
func TestMigrate(t *testing.T) {
	_, database := integrationStore(t)
	migration, err := os.ReadFile("/workspace/Backend/migrations/000003_session_admission.sql")
	if err != nil {
		t.Fatal(err)
	}
	execute(t, database, string(migration))
}

// 两个真实数据库连接竞争同一凭据，仅同一服务器连接身份的网络重试允许幂等。
func TestAtomicClaimAndCommit(t *testing.T) {
	store, database := integrationStore(t)
	prepare(t, database, "claim", 1)
	request, proof := reservation("claim")
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	if _, err := store.Reserve(ctx, request); err != nil {
		t.Fatal(err)
	}
	var winners atomic.Int32
	var winner string
	var winnerLock sync.Mutex
	var group sync.WaitGroup
	for index := 0; index < 16; index++ {
		identifier := fmt.Sprintf("server-connection-%d", index)
		group.Add(1)
		go func() {
			defer group.Done()
			_, err := store.Claim(ctx, connection(request, identifier), "attempt", proof)
			if err == nil {
				winners.Add(1)
				winnerLock.Lock()
				winner = identifier
				winnerLock.Unlock()
			}
		}()
	}
	group.Wait()
	if winners.Load() != 1 {
		t.Fatalf("领取成功数=%d，期望1", winners.Load())
	}
	actual := connection(request, winner)
	if _, err := store.Claim(ctx, actual, "attempt", proof); err != nil {
		t.Fatalf("同连接重试:%v", err)
	}
	epoch, err := store.Commit(ctx, actual, 30)
	if err != nil || epoch != 1 {
		t.Fatalf("提交失败epoch=%d error=%v", epoch, err)
	}
	retry, err := store.Commit(ctx, actual, 30)
	if err != nil || retry != epoch {
		t.Fatalf("提交幂等失败:%v", err)
	}
	snapshot, err := store.Lookup(ctx, request.ID, request.AuthorizationID)
	if err != nil || snapshot.Epoch != epoch || snapshot.State != "Admitted" {
		t.Fatalf("提交结果查询失败:%+v %v", snapshot, err)
	}
	if _, err := store.Lookup(ctx, request.ID, "someone-else"); err == nil {
		t.Fatal("越权查询未拒绝")
	}
	execute(t, database, `INSERT INTO session_authorizations VALUES('other','game','other','auth','owned',NOW()+INTERVAL '10 minutes',FALSE)`)
	other, _ := reservation("other")
	other.InstanceID = "claim"
	if _, err := store.Reserve(ctx, other); err == nil {
		t.Fatal("提交后不得释放已占用容量")
	}
	state, err := store.Cancel(ctx, request.ID, request.AuthorizationID)
	if err != nil || state != "Admitted" {
		t.Fatalf("提交先赢，取消必须报告Admitted:%s %v", state, err)
	}
	released, err := store.Release(ctx, actual, epoch)
	if err != nil || !released {
		t.Fatal("释放失败", err)
	}
	if _, err := store.Reserve(ctx, other); err != nil {
		t.Fatalf("释放后容量未恢复:%v", err)
	}
}

func TestCapacityAndIdentity(t *testing.T) {
	store, database := integrationStore(t)
	prepare(t, database, "capacity", 2)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	var winners atomic.Int32
	var group sync.WaitGroup
	for index := 0; index < 12; index++ {
		prefix := fmt.Sprintf("capacity-player-%d", index)
		execute(t, database, `INSERT INTO session_authorizations VALUES($1,'game',$1,'auth','owned',NOW()+INTERVAL '10 minutes',FALSE)`, prefix)
		request, _ := reservation(prefix)
		request.InstanceID = "capacity"
		group.Add(1)
		go func() {
			defer group.Done()
			if _, err := store.Reserve(ctx, request); err == nil {
				winners.Add(1)
			}
		}()
	}
	group.Wait()
	if winners.Load() != 2 {
		t.Fatalf("容量2并发成功=%d", winners.Load())
	}
	prepare(t, database, "identity", 1)
	request, proof := reservation("identity")
	if _, err := store.Reserve(ctx, request); err != nil {
		t.Fatal(err)
	}
	changed := request
	changed.TTLSeconds++
	if _, err := store.Reserve(ctx, changed); err == nil {
		t.Fatal("同操作不同TTL未拒绝")
	}
	changed = request
	changed.AuthorizationID = "capacity"
	if _, err := store.Reserve(ctx, changed); err == nil {
		t.Fatal("跨主体操作未拒绝")
	}
	actual := connection(request, "connection")
	wrong := actual
	wrong.BootID = "old-boot"
	if _, err := store.Claim(ctx, wrong, "attempt", proof); err == nil {
		t.Fatal("错误启动代次放行")
	}
	if _, err := store.Claim(ctx, actual, "wrong-attempt", proof); err == nil {
		t.Fatal("错误尝试放行")
	}
	if _, err := store.Claim(ctx, actual, "attempt", []byte(strings.Repeat("x", 32))); err == nil {
		t.Fatal("错误材料放行")
	}
	execute(t, database, `UPDATE session_authorizations SET revoked=TRUE WHERE authorization_id='identity'`)
	if _, err := store.Claim(ctx, actual, "attempt", proof); err == nil {
		t.Fatal("撤销授权未生效")
	}
}

func TestMigrationFenceAndExpiry(t *testing.T) {
	store, database := integrationStore(t)
	prepare(t, database, "migration", 2)
	execute(t, database, `INSERT INTO session_instances VALUES('target','boot','game','world','1',2,TRUE,NOW()+INTERVAL '10 minutes')`)
	ctx, cancel := context.WithTimeout(context.Background(), 20*time.Second)
	defer cancel()
	source, proof := reservation("migration")
	sourceConnection := connection(source, "source-connection")
	if _, err := store.Reserve(ctx, source); err != nil {
		t.Fatal(err)
	}
	if _, err := store.Claim(ctx, sourceConnection, "attempt", proof); err != nil {
		t.Fatal(err)
	}
	epoch, err := store.Commit(ctx, sourceConnection, 30)
	if err != nil {
		t.Fatal(err)
	}
	target := source
	target.ID = "target-reservation"
	target.OperationID = "target-operation"
	target.InstanceID = "target"
	targetConnection := connection(target, "target-connection")
	if _, err := store.Reserve(ctx, target); err != nil {
		t.Fatal(err)
	}
	if _, err := store.Claim(ctx, targetConnection, "attempt", proof); err != nil {
		t.Fatal(err)
	}
	if _, err := store.Commit(ctx, targetConnection, 30); err == nil {
		t.Fatal("来源仍有租约，不得双活")
	}
	if released, err := store.Release(ctx, sourceConnection, epoch); err != nil || !released {
		t.Fatal("来源释放失败", err)
	}
	nextEpoch, err := store.Commit(ctx, targetConnection, 30)
	if err != nil || nextEpoch != epoch+1 {
		t.Fatal("目标代次提交失败", err)
	}
	if _, err := store.Release(ctx, sourceConnection, epoch); err != nil {
		t.Fatal(err)
	}
	var current string
	if err := database.QueryRow(`SELECT reservation_id FROM session_bindings WHERE player_id='migration'`).Scan(&current); err != nil || current != target.ID {
		t.Fatal("旧来源影响目标", err)
	}
	prepare(t, database, "expired", 1)
	expired, expiredProof := reservation("expired")
	if _, err := store.Reserve(ctx, expired); err != nil {
		t.Fatal(err)
	}
	execute(t, database, `UPDATE session_reservations SET expires_at=NOW()-INTERVAL '1 second' WHERE reservation_id='expired'`)
	if _, err := store.Claim(ctx, connection(expired, "connection"), "attempt", expiredProof); err == nil {
		t.Fatal("到期材料放行")
	}
	expired.ID = "replacement"
	expired.OperationID = "replacement-operation"
	if _, err := store.Reserve(ctx, expired); err != nil {
		t.Fatal("过期预留仍占用容量", err)
	}
}

func TestPersistencePrepare(t *testing.T) {
	store, database := integrationStore(t)
	prepare(t, database, "persistent", 1)
	request, proof := reservation("persistent")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if _, err := store.Reserve(ctx, request); err != nil {
		t.Fatal(err)
	}
	if _, err := store.Claim(ctx, connection(request, "original-connection"), "attempt", proof); err != nil {
		t.Fatal(err)
	}
}

// 由脚本真正重启本次PostgreSQL容器后执行，不能用重新创建Go对象代替数据库重启。
func TestPersistenceAfterRestart(t *testing.T) {
	store, database := integrationStore(t)
	var state string
	if err := database.QueryRow(`SELECT state FROM session_reservations WHERE reservation_id='persistent'`).Scan(&state); err != nil || state != "Claimed" {
		t.Fatal("领取记录未持久化", err)
	}
	request, proof := reservation("persistent")
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()
	if _, err := store.Claim(ctx, connection(request, "second-connection"), "attempt", proof); err == nil {
		t.Fatal("重启后重放放行")
	}
}
