//go:build productiondeps

package postgres

// PlayerData生产可信事件来源：使用000003 session_bindings权威租约和000011事件日志。
// 不以Event JSON、HTTP头或服务端Boot字符串本身当身份认证；外层需要独立内部凭据。
// 此处读取真实数据库，授权失败和依赖故障均失败关闭。

import (
	"context"
	"encoding/json"
	"errors"
	"strings"

	"github.com/jackc/pgx/v5"

	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/platform/apperror"
)

// InventoryAuthorityRepository（可信事件与会话仓储）不属于普通Gateway端口。
type InventoryAuthorityRepository struct{ pool *Pool }

// NewInventoryAuthorityRepository（创建服务器可信事件仓储）仅由生产PlayerData装配。
func NewInventoryAuthorityRepository(pool *Pool) *InventoryAuthorityRepository {
	return &InventoryAuthorityRepository{pool:pool}
}

var _ inventory.ServerPlayerVerifier = (*InventoryAuthorityRepository)(nil)
var _ inventory.ConfirmedEventJournal = (*InventoryAuthorityRepository)(nil)
var _ inventory.GrantEvidenceSource = (*InventoryAuthorityRepository)(nil)
var _ inventory.ConsumeEvidenceSource = (*InventoryAuthorityRepository)(nil)

// sessionInventoryLeaseSQL（可信会话校验）同时要求当前连接、已准入预留、
// 实例Ready/健康租约、未吊销的准入决策以及在线登录的Refresh会话都有效。
// 注意短期session_authorizations.expires_at属于入服签票窗口；已提交会话的持续权限
// 必须依赖30秒实时绑定租约+在线身份会话（未登出/未封禁），不能永久按签票TTL限制游戏。
// FOR SHARE阻止并发迁服修改绑定，绝不只依据请求自报的GameServerID授权。
const sessionInventoryLeaseSQL = `SELECT b.player_id
	FROM session_bindings b
	JOIN session_instances i
	  ON i.instance_id=b.instance_id AND i.boot_id=b.boot_id AND i.game_id=b.game_id
	JOIN session_reservations r
	  ON r.reservation_id=b.reservation_id AND r.instance_id=b.instance_id
	  AND r.boot_id=b.boot_id AND r.state='Admitted' AND r.admitted_epoch=b.session_epoch
	  AND r.connection_id=b.connection_id
	JOIN session_authorizations a ON a.authorization_id=r.authorization_id
	  AND a.player_id=b.player_id AND a.game_id=b.game_id
	JOIN online_identity_sessions os ON os.session_id=a.auth_session_id
	  AND os.game_id=b.game_id AND os.player_id=b.player_id AND NOT os.revoked
	JOIN online_identity_accounts account ON account.game_id=b.game_id
	  AND account.player_id=b.player_id AND NOT account.disabled
	WHERE b.game_id=$1 AND b.player_id=$2 AND b.instance_id=$3 AND b.boot_id=$4
	  AND b.session_epoch=$5 AND b.connection_id=$6
	  AND b.authority_until>clock_timestamp()
	  AND i.ready AND i.healthy_until>clock_timestamp()
	  AND NOT a.revoked AND os.refresh_expires_at>clock_timestamp()
	FOR SHARE OF b,i,r,a,os,account`

func inventoryTrustDenied() error {
	return apperror.New("INVENTORY_TRUST_DENIED","服务器、玩家会话或事件授权无效",false)
}

// verifyInventoryLeaseTx（事务内玩家租约）在Grant/Consume变更完成前持有绑定行的共享锁。
// 登出与跨地图迁移无法在同一事务中间把玩家换到另一个连接。
func verifyInventoryLeaseTx(ctx context.Context, tx pgx.Tx, lease inventory.TrustedPlayerLease) error {
	if strings.TrimSpace(lease.GameID)=="" || strings.TrimSpace(lease.PlayerID)=="" ||
		strings.TrimSpace(lease.GameServerID)=="" || strings.TrimSpace(lease.ServerBootID)=="" ||
		strings.TrimSpace(lease.ConnectionID)=="" || lease.SessionEpoch<1 {
		return inventoryTrustDenied()
	}
	var playerID string
	err:=tx.QueryRow(ctx,sessionInventoryLeaseSQL,
		lease.GameID,lease.PlayerID,lease.GameServerID,lease.ServerBootID,
		lease.SessionEpoch,lease.ConnectionID).Scan(&playerID)
	if errors.Is(err,pgx.ErrNoRows) {return inventoryTrustDenied()}
	if err!=nil {return inventoryPGError(err)}
	if playerID!=lease.PlayerID{return inventoryTrustDenied()}
	return nil
}

// VerifyServerPlayer（当前真实数据库租约验证），不是伪造内存白名单。
func (r *InventoryAuthorityRepository) VerifyServerPlayer(ctx context.Context, lease inventory.TrustedPlayerLease) error {
	if err:=ctx.Err();err!=nil{return err}
	if r==nil || r.pool==nil || r.pool.inner==nil{return inventoryTrustDenied()}
	tx,err:=r.pool.inner.BeginTx(ctx,pgx.TxOptions{IsoLevel:pgx.ReadCommitted})
	if err!=nil{return inventoryPGError(err)}
	defer rollbackInventory(tx)
	if err=verifyInventoryLeaseTx(ctx,tx,lease);err!=nil{return err}
	return commitInventory(ctx,tx)
}

// RecordConfirmedEvent（可信事件日志）先验证来源玩家租约，后在同一事务记入事件。
// 首次插入之后同EventID同内容幂等重放，不同内容严格拒绝，不能重复领奖。
func (r *InventoryAuthorityRepository) RecordConfirmedEvent(
	ctx context.Context, event inventory.ConfirmedInventoryEvent,
) error {
	if err:=ctx.Err();err!=nil{return err}
	if r==nil || r.pool==nil || r.pool.inner==nil{return inventoryTrustDenied()}
	if err:=event.Validate();err!=nil{return err}
	payload,err:=json.Marshal(event)
	if err!=nil{return inventoryUnavailable()}
	tx,err:=r.pool.inner.BeginTx(ctx,pgx.TxOptions{IsoLevel:pgx.ReadCommitted})
	if err!=nil{return inventoryPGError(err)}
	defer rollbackInventory(tx)
	if err=verifyInventoryLeaseTx(ctx,tx,event.Lease());err!=nil{return err}
	tag,err:=tx.Exec(ctx,`INSERT INTO player_inventory_trusted_events
		(source_event_id,event_kind,game_id,player_id,game_server_id,server_boot_id,
		 session_epoch,connection_id,expected_revision,item_definition_id,
		 item_instance_id,quantity,reason_code,canonical_event)
		VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14::jsonb)
		ON CONFLICT(source_event_id) DO NOTHING`,
		event.SourceEventID,event.EventKind,event.GameID,event.PlayerID,
		event.GameServerID,event.ServerBootID,event.SessionEpoch,event.ConnectionID,
		event.ExpectedRevision,event.ItemDefinitionID,event.ItemInstanceID,
		event.Quantity,event.ReasonCode,string(payload))
	if err!=nil{return inventoryPGError(err)}
	if tag.RowsAffected()==0 {
		var stored []byte
		err=tx.QueryRow(ctx,`SELECT canonical_event
			FROM player_inventory_trusted_events WHERE source_event_id=$1`,
			event.SourceEventID).Scan(&stored)
		if err!=nil{return inventoryPGError(err)}
		var previous inventory.ConfirmedInventoryEvent
		if json.Unmarshal(stored,&previous)!=nil { return inventoryUnavailable() }
		// 首次期望修订号留作审计，但服务器仅可在同一已确认事件身份下更新并发前提。
		previous.ExpectedRevision=event.ExpectedRevision
		if previous!=event {
			return apperror.New("INVENTORY_EVENT_ID_CONFLICT",
				"同一权威事件编号对应不同请求，拒绝重复处理",false)
		}
	}
	return commitInventory(ctx,tx)
}

// FindGrantEvidence（可信授予证据）只从受控服务器事件库读，不读取客户端自报正文。
func (r *InventoryAuthorityRepository) FindGrantEvidence(
	ctx context.Context, eventID string,
)(inventory.GrantEvidence,error){
	if err:=ctx.Err();err!=nil{return inventory.GrantEvidence{},err}
	if r==nil || r.pool==nil || r.pool.inner==nil{return inventory.GrantEvidence{},inventoryTrustDenied()}
	var row inventory.GrantEvidence
	err:=r.pool.inner.QueryRow(ctx,`SELECT source_event_id,game_id,game_server_id,
		server_boot_id,player_id,session_epoch,connection_id,reason_code,item_definition_id,quantity
		FROM player_inventory_trusted_events WHERE source_event_id=$1 AND event_kind='grant'`,
		eventID).Scan(&row.SourceEventID,&row.GameID,&row.GameServerID,&row.ServerBootID,
		&row.PlayerID,&row.SessionEpoch,&row.ConnectionID,&row.ReasonCode,
		&row.ItemDefinitionID,&row.Quantity)
	if errors.Is(err,pgx.ErrNoRows){return inventory.GrantEvidence{},inventoryTrustDenied()}
	if err!=nil{return inventory.GrantEvidence{},inventoryPGError(err)}
	return row,nil
}

// FindConsumeEvidence（可信使用证据）同时锁定物品实例，避免跨物品错误扣除。
func (r *InventoryAuthorityRepository) FindConsumeEvidence(
	ctx context.Context,eventID string,
)(inventory.ConsumeEvidence,error){
	if err:=ctx.Err();err!=nil{return inventory.ConsumeEvidence{},err}
	if r==nil || r.pool==nil || r.pool.inner==nil{return inventory.ConsumeEvidence{},inventoryTrustDenied()}
	var row inventory.ConsumeEvidence
	err:=r.pool.inner.QueryRow(ctx,`SELECT source_event_id,game_id,game_server_id,
		server_boot_id,player_id,session_epoch,connection_id,reason_code,
		item_definition_id,item_instance_id,quantity
		FROM player_inventory_trusted_events WHERE source_event_id=$1 AND event_kind='consume'`,
		eventID).Scan(&row.SourceEventID,&row.GameID,&row.GameServerID,&row.ServerBootID,
		&row.PlayerID,&row.SessionEpoch,&row.ConnectionID,&row.ReasonCode,
		&row.ItemDefinitionID,&row.ItemInstanceID,&row.Quantity)
	if errors.Is(err,pgx.ErrNoRows){return inventory.ConsumeEvidence{},inventoryTrustDenied()}
	if err!=nil{return inventory.ConsumeEvidence{},inventoryPGError(err)}
	return row,nil
}
