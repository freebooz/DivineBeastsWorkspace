// Package postgresadmission（会话准入数据库适配）调用PostgreSQL原子预留、领取和绑定内核。
// 目前未挂载HTTP/RPC；上层必须先认证玩家/服务器并绑定真实连接，不得把本包当作认证服务。
package postgresadmission

import (
	"context"
	"crypto/sha256"
	"database/sql"
	"errors"
	"time"
)

// Store（持久化准入端口）不持有账号令牌或服务器密钥；数据库连接由装配层拥有。
type Store struct{ database *sql.DB }

// NewStore（创建事务端口）要求已执行000003迁移的PostgreSQL连接；不自动迁移或关闭连接。
func NewStore(database *sql.DB) (*Store, error) {
	if database == nil {
		return nil, errors.New("SESSION_DATABASE_REQUIRED")
	}
	return &Store{database: database}, nil
}

// Reservation（预留请求）仅能由可信应用层构造；身份授权、实例选择和版本策略不由客户端决定。
type Reservation struct {
	ID               string   // ID：后端生成的预留身份。
	OperationID      string   // OperationID：不可换主体/换参数的幂等身份。
	AuthorizationID  string   // AuthorizationID：已核验认证会话和角色的短期决策记录。
	InstanceID       string   // InstanceID：后端选出的目标实例。
	BootID           string   // BootID：目标进程启动代次。
	AttemptID        string   // AttemptID：单次客户端尝试关联，不证明真实连接。
	CredentialDigest [32]byte // CredentialDigest：随机准入材料的摘要，原文不进入持久化快照。
	ProtocolVersion  string   // ProtocolVersion：已协商的公共网络协议版本。
	TTLSeconds       int      // TTLSeconds：预留秒数，数据库限定1至120秒且不超过授权到期。
}

// Connection（服务器连接身份）由受认证服务器在真实握手中产生，不能转发客户端自报值。
type Connection struct {
	ReservationID string // ReservationID：待领取的预留身份。
	InstanceID    string // InstanceID：从服务器认证主体提取的实例身份。
	BootID        string // BootID：认证主体绑定的本次启动身份。
	ConnectionID  string // ConnectionID：服务器生成且绑定实际网络连接的不可复用身份。
}

// Snapshot（非敏感准入状态）仅供同一授权主体查询；不返回材料摘要或服务器连接关联。
type Snapshot struct {
	ReservationID  string    // ReservationID：原预留身份。
	State          string    // State：持久化状态，须结合ExpiresAt和AuthorityUntil判定有效性。
	InstanceID     string    // InstanceID：本次分配实例，不包含端点或运维地址。
	BootID         string    // BootID：目标启动代次。
	Epoch          int64     // Epoch：已提交代次，未提交为0。
	ExpiresAt      time.Time // ExpiresAt：预留/领取截止UTC时间。
	AuthorityUntil time.Time // AuthorityUntil：对应绑定仍有效的UTC租约截止，无活动绑定为Unix纪元。
}

// Lookup（查询不确定结果）核对短期授权主体，失败不重签材料；Admitted不等于UE已确认本地Ready。
func (store *Store) Lookup(ctx context.Context, reservationID, authorizationID string) (Snapshot, error) {
	var snapshot Snapshot
	err := store.database.QueryRowContext(ctx, `SELECT reservation.reservation_id,reservation.state,reservation.instance_id,reservation.boot_id,
COALESCE(reservation.admitted_epoch,0),reservation.expires_at,COALESCE(NULLIF(binding.authority_until,'-infinity'::timestamptz),'epoch'::timestamptz)
FROM session_reservations reservation JOIN session_authorizations grant_record USING(authorization_id)
LEFT JOIN session_bindings binding ON binding.reservation_id=reservation.reservation_id
WHERE reservation.reservation_id=$1 AND reservation.authorization_id=$2 AND NOT grant_record.revoked AND grant_record.expires_at>clock_timestamp()`, reservationID, authorizationID).Scan(
		&snapshot.ReservationID, &snapshot.State, &snapshot.InstanceID, &snapshot.BootID, &snapshot.Epoch, &snapshot.ExpiresAt, &snapshot.AuthorityUntil)
	return snapshot, err
}

// Reserve（预留容量）重试必须使用完全相同的身份/材料摘要；只返回状态，不重发原始准入材料。
func (store *Store) Reserve(ctx context.Context, request Reservation) (string, error) {
	var state string
	err := store.database.QueryRowContext(ctx, "SELECT session_reserve($1,$2,$3,$4,$5,$6,$7,$8,$9)", request.ID, request.OperationID,
		request.AuthorizationID, request.InstanceID, request.BootID, request.AttemptID, request.CredentialDigest[:], request.ProtocolVersion, request.TTLSeconds).Scan(&state)
	return state, err
}

// Claim（原子领取）原始材料仅用于本次摘要比对，不存储、不写日志；同连接重试幂等，第二连接拒绝。
func (store *Store) Claim(ctx context.Context, connection Connection, attemptID string, proof []byte) (string, error) {
	if len(proof) < 32 {
		return "", errors.New("SESSION_PROOF_INVALID")
	}
	digest := sha256.Sum256(proof)
	var state string
	err := store.database.QueryRowContext(ctx, "SELECT session_claim($1,$2,$3,$4,$5,$6)", connection.ReservationID, connection.InstanceID,
		connection.BootID, connection.ConnectionID, attemptID, digest[:]).Scan(&state)
	return state, err
}

// Commit（提交实际连接）只有来源租约结束才成功；返回后端绑定代次，租约秒数1至30。
// 调用者须在确认提交后激活玩家，并在租约截止前停止权威；目前UE租约执行器尚未接通。
func (store *Store) Commit(ctx context.Context, connection Connection, leaseSeconds int) (int64, error) {
	var epoch int64
	err := store.database.QueryRowContext(ctx, "SELECT session_commit($1,$2,$3,$4,$5)", connection.ReservationID, connection.InstanceID,
		connection.BootID, connection.ConnectionID, leaseSeconds).Scan(&epoch)
	return epoch, err
}

// Release（释放确切绑定）不匹配或旧代次返回false；重放同一已释放绑定幂等，不影响新绑定。
func (store *Store) Release(ctx context.Context, connection Connection, epoch int64) (bool, error) {
	var released bool
	err := store.database.QueryRowContext(ctx, "SELECT session_release($1,$2,$3,$4,$5)", connection.ReservationID, connection.InstanceID,
		connection.BootID, connection.ConnectionID, epoch).Scan(&released)
	return released, err
}

// Cancel（撤销未接入预留）返回Admitted表示提交先赢，上层必须查询绑定后执行Leave，不能伪称取消成功。
func (store *Store) Cancel(ctx context.Context, reservationID, authorizationID string) (string, error) {
	var state string
	err := store.database.QueryRowContext(ctx, "SELECT session_cancel($1,$2)", reservationID, authorizationID).Scan(&state)
	return state, err
}
