//go:build productiondeps

package postgres

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/binary"
	"encoding/hex"
	"encoding/json"
	"errors"
	"math"
	"time"

	"github.com/jackc/pgx/v5"

	"divinebeasts/backend/internal/modules/inventory"
	"divinebeasts/backend/internal/platform/apperror"
)

// InventoryRepository（PostgreSQL背包仓储）在一个事务内提交背包状态、修订号和Operation结果。
type InventoryRepository struct{ pool *Pool }

func NewInventoryRepository(pool *Pool) *InventoryRepository {
	return &InventoryRepository{pool: pool}
}

var _ inventory.Repository = (*InventoryRepository)(nil)

func (r *InventoryRepository) available() bool {
	return r != nil && r.pool != nil && r.pool.inner != nil
}

func (r *InventoryRepository) Probe(ctx context.Context) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if !r.available() {
		return inventoryUnavailable()
	}
	rows, err := r.pool.inner.Query(ctx, `SELECT
		i.player_id,i.inventory_revision,
		c.container_id,c.capacity,
		x.item_instance_id,x.item_definition_id,x.quantity,x.container_id,x.slot_index,x.revision,x.instance_state,x.max_stack_size,
		q.slot_index,q.item_instance_id,q.revision,
		o.operation_id,o.operation_type,o.canonical_request,o.response_snapshot
		FROM player_inventories i
		CROSS JOIN player_inventory_containers c
		CROSS JOIN player_inventory_items x
		CROSS JOIN player_inventory_quickbar q
		CROSS JOIN player_inventory_operations o LIMIT 0`)
	if err != nil {
		return inventoryPGError(err)
	}
	rows.Close()
	return inventoryPGError(rows.Err())
}

func (r *InventoryRepository) GetSnapshot(ctx context.Context, playerID string) (inventory.Snapshot, error) {
	if err := ctx.Err(); err != nil {
		return inventory.Snapshot{}, err
	}
	if !r.available() {
		return inventory.Snapshot{}, inventoryUnavailable()
	}
	tx, err := r.pool.inner.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}
	defer rollbackInventory(tx)
	if _, err = ensureInventoryTx(ctx, tx, playerID); err != nil {
		return inventory.Snapshot{}, err
	}
	snapshot, err := readInventorySnapshotTx(ctx, tx, playerID)
	if err != nil {
		return inventory.Snapshot{}, err
	}
	if err = commitInventory(ctx, tx); err != nil {
		return inventory.Snapshot{}, err
	}
	return snapshot, nil
}

func (r *InventoryRepository) GetOperation(ctx context.Context, playerID, operationID string) (inventory.MutationResult, error) {
	if err := ctx.Err(); err != nil {
		return inventory.MutationResult{}, err
	}
	if !r.available() {
		return inventory.MutationResult{}, inventoryUnavailable()
	}
	var snapshot []byte
	err := r.pool.inner.QueryRow(ctx, `SELECT response_snapshot FROM player_inventory_operations
		WHERE player_id=$1 AND operation_id=$2`, playerID, operationID).Scan(&snapshot)
	if errors.Is(err, pgx.ErrNoRows) {
		return inventory.MutationResult{}, apperror.New("INVENTORY_OPERATION_NOT_FOUND", "背包操作不存在", false)
	}
	if err != nil {
		return inventory.MutationResult{}, inventoryPGError(err)
	}
	var result inventory.MutationResult
	if json.Unmarshal(snapshot, &result) != nil || result.OperationID != operationID || result.Snapshot.InventoryRevision < 1 {
		return inventory.MutationResult{}, inventoryUnavailable()
	}
	result.Duplicate = true
	return result, nil
}

func (r *InventoryRepository) Move(ctx context.Context, playerID string, command inventory.MoveCommand) (inventory.MutationResult, error) {
	return r.mutate(ctx, playerID, command.OperationID, "Move", command.ExpectedRevision, command,
		func(ctx context.Context, tx pgx.Tx) (int32, int32, error) {
			var sourceContainer string
			var sourceSlot, quantity int32
			err := tx.QueryRow(ctx, `SELECT container_id,slot_index,quantity FROM player_inventory_items
				WHERE player_id=$1 AND item_instance_id=$2 FOR UPDATE`, playerID, command.ItemInstanceID).
				Scan(&sourceContainer, &sourceSlot, &quantity)
			if errors.Is(err, pgx.ErrNoRows) {
				return 0, 0, inventoryItemNotFound()
			}
			if err != nil {
				return 0, 0, inventoryPGError(err)
			}
			capacity, err := inventoryContainerCapacity(ctx, tx, playerID, command.TargetContainerID)
			if err != nil {
				return 0, 0, err
			}
			if command.TargetSlotIndex < 0 || command.TargetSlotIndex >= capacity {
				return 0, 0, apperror.New("INVENTORY_SLOT_OUT_OF_RANGE", "背包槽位越界", false)
			}
			if sourceContainer == command.TargetContainerID && sourceSlot == command.TargetSlotIndex {
				return 0, quantity, apperror.New("INVALID_REQUEST", "物品已位于目标槽位", false)
			}

			var targetID string
			err = tx.QueryRow(ctx, `SELECT item_instance_id FROM player_inventory_items
				WHERE player_id=$1 AND container_id=$2 AND slot_index=$3 FOR UPDATE`,
				playerID, command.TargetContainerID, command.TargetSlotIndex).Scan(&targetID)
			if err != nil && !errors.Is(err, pgx.ErrNoRows) {
				return 0, 0, inventoryPGError(err)
			}
			if targetID != "" {
				if _, err = tx.Exec(ctx, `UPDATE player_inventory_items
					SET container_id=$3,slot_index=$4,revision=revision+1
					WHERE player_id=$1 AND item_instance_id=$2`,
					playerID, targetID, sourceContainer, sourceSlot); err != nil {
					return 0, 0, inventoryPGError(err)
				}
			}
			if _, err = tx.Exec(ctx, `UPDATE player_inventory_items
				SET container_id=$3,slot_index=$4,revision=revision+1
				WHERE player_id=$1 AND item_instance_id=$2`,
				playerID, command.ItemInstanceID, command.TargetContainerID, command.TargetSlotIndex); err != nil {
				return 0, 0, inventoryPGError(err)
			}
			return quantity, 0, nil
		})
}

func (r *InventoryRepository) Split(ctx context.Context, playerID string, command inventory.SplitCommand) (inventory.MutationResult, error) {
	return r.mutate(ctx, playerID, command.OperationID, "Split", command.ExpectedRevision, command,
		func(ctx context.Context, tx pgx.Tx) (int32, int32, error) {
			var item inventory.ItemInstance
			err := tx.QueryRow(ctx, `SELECT item_instance_id,item_definition_id,quantity,container_id,slot_index,revision,instance_state,max_stack_size
				FROM player_inventory_items WHERE player_id=$1 AND item_instance_id=$2 FOR UPDATE`,
				playerID, command.SourceItemInstanceID).
				Scan(&item.ItemInstanceID, &item.ItemDefinitionID, &item.Quantity, &item.ContainerID, &item.SlotIndex, &item.Revision, &item.InstanceState, &item.MaxStackSize)
			if errors.Is(err, pgx.ErrNoRows) {
				return 0, 0, inventoryItemNotFound()
			}
			if err != nil {
				return 0, 0, inventoryPGError(err)
			}
			if command.SplitQuantity <= 0 || command.SplitQuantity >= item.Quantity {
				return 0, item.Quantity, apperror.New("INVENTORY_INSUFFICIENT_QUANTITY", "拆分数量必须小于源堆叠数量", false)
			}
			capacity, err := inventoryContainerCapacity(ctx, tx, playerID, command.TargetContainerID)
			if err != nil {
				return 0, item.Quantity, err
			}
			if command.TargetSlotIndex < 0 || command.TargetSlotIndex >= capacity {
				return 0, item.Quantity, apperror.New("INVENTORY_SLOT_OUT_OF_RANGE", "背包槽位越界", false)
			}
			var occupied bool
			if err = tx.QueryRow(ctx, `SELECT EXISTS(SELECT 1 FROM player_inventory_items
				WHERE player_id=$1 AND container_id=$2 AND slot_index=$3)`,
				playerID, command.TargetContainerID, command.TargetSlotIndex).Scan(&occupied); err != nil {
				return 0, item.Quantity, inventoryPGError(err)
			}
			if occupied {
				return 0, item.Quantity, apperror.New("INVENTORY_SLOT_OCCUPIED", "目标槽位已占用", false)
			}
			newID := inventorySplitItemID(command.OperationID, command.SourceItemInstanceID)
			if _, err = tx.Exec(ctx, `UPDATE player_inventory_items SET quantity=quantity-$3,revision=revision+1
				WHERE player_id=$1 AND item_instance_id=$2`, playerID, item.ItemInstanceID, command.SplitQuantity); err != nil {
				return 0, item.Quantity, inventoryPGError(err)
			}
			if _, err = tx.Exec(ctx, `INSERT INTO player_inventory_items
				(player_id,item_instance_id,item_definition_id,quantity,container_id,slot_index,revision,instance_state,max_stack_size)
				VALUES($1,$2,$3,$4,$5,$6,1,$7,$8)`,
				playerID, newID, item.ItemDefinitionID, command.SplitQuantity, command.TargetContainerID,
				command.TargetSlotIndex, item.InstanceState, item.MaxStackSize); err != nil {
				return 0, item.Quantity, inventoryPGError(err)
			}
			return command.SplitQuantity, item.Quantity - command.SplitQuantity, nil
		})
}

func (r *InventoryRepository) Merge(ctx context.Context, playerID string, command inventory.MergeCommand) (inventory.MutationResult, error) {
	return r.mutate(ctx, playerID, command.OperationID, "Merge", command.ExpectedRevision, command,
		func(ctx context.Context, tx pgx.Tx) (int32, int32, error) {
			source, err := inventoryItemForUpdate(ctx, tx, playerID, command.SourceItemInstanceID)
			if err != nil {
				return 0, 0, err
			}
			target, err := inventoryItemForUpdate(ctx, tx, playerID, command.TargetItemInstanceID)
			if err != nil {
				return 0, source.Quantity, err
			}
			if source.ItemDefinitionID != target.ItemDefinitionID {
				return 0, source.Quantity, apperror.New("INVALID_REQUEST", "不同定义物品不能合并", false)
			}
			total := int64(source.Quantity) + int64(target.Quantity)
			if target.MaxStackSize < 1 || total > int64(target.MaxStackSize) || total > math.MaxInt32 {
				return 0, source.Quantity, apperror.New("INVENTORY_STACK_LIMIT_EXCEEDED", "合并后超过权威堆叠上限", false)
			}
			if _, err = tx.Exec(ctx, `UPDATE player_inventory_items SET quantity=$3,revision=revision+1
				WHERE player_id=$1 AND item_instance_id=$2`, playerID, target.ItemInstanceID, int32(total)); err != nil {
				return 0, source.Quantity, inventoryPGError(err)
			}
			if _, err = tx.Exec(ctx, `UPDATE player_inventory_quickbar
				SET item_instance_id=$3,revision=revision+1
				WHERE player_id=$1 AND item_instance_id=$2`, playerID, source.ItemInstanceID, target.ItemInstanceID); err != nil {
				return 0, source.Quantity, inventoryPGError(err)
			}
			if _, err = tx.Exec(ctx, `DELETE FROM player_inventory_items WHERE player_id=$1 AND item_instance_id=$2`,
				playerID, source.ItemInstanceID); err != nil {
				return 0, source.Quantity, inventoryPGError(err)
			}
			return source.Quantity, 0, nil
		})
}

func (r *InventoryRepository) SetQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	return r.mutate(ctx, playerID, command.OperationID, "SetQuickbar", command.ExpectedRevision, command,
		func(ctx context.Context, tx pgx.Tx) (int32, int32, error) {
			var exists bool
			if err := tx.QueryRow(ctx, `SELECT EXISTS(SELECT 1 FROM player_inventory_items
				WHERE player_id=$1 AND item_instance_id=$2)`, playerID, command.ItemInstanceID).Scan(&exists); err != nil {
				return 0, 0, inventoryPGError(err)
			}
			if !exists {
				return 0, 0, inventoryItemNotFound()
			}
			if command.SlotIndex < 0 || command.SlotIndex >= inventory.MaxQuickbarSlots {
				return 0, 0, apperror.New("INVENTORY_SLOT_OUT_OF_RANGE", "快捷栏槽位越界", false)
			}
			_, err := tx.Exec(ctx, `INSERT INTO player_inventory_quickbar(player_id,slot_index,item_instance_id,revision)
				VALUES($1,$2,$3,1)
				ON CONFLICT(player_id,slot_index) DO UPDATE SET item_instance_id=EXCLUDED.item_instance_id,
					revision=player_inventory_quickbar.revision+1`, playerID, command.SlotIndex, command.ItemInstanceID)
			return 0, 0, inventoryPGError(err)
		})
}

func (r *InventoryRepository) ClearQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	return r.mutate(ctx, playerID, command.OperationID, "ClearQuickbar", command.ExpectedRevision, command,
		func(ctx context.Context, tx pgx.Tx) (int32, int32, error) {
			if command.SlotIndex < 0 || command.SlotIndex >= inventory.MaxQuickbarSlots {
				return 0, 0, apperror.New("INVENTORY_SLOT_OUT_OF_RANGE", "快捷栏槽位越界", false)
			}
			_, err := tx.Exec(ctx, `DELETE FROM player_inventory_quickbar WHERE player_id=$1 AND slot_index=$2`,
				playerID, command.SlotIndex)
			return 0, 0, inventoryPGError(err)
		})
}

func (r *InventoryRepository) mutate(
	ctx context.Context,
	playerID, operationID, operationType string,
	expectedRevision int64,
	canonical any,
	apply func(context.Context, pgx.Tx) (int32, int32, error),
) (inventory.MutationResult, error) {
	if err := ctx.Err(); err != nil {
		return inventory.MutationResult{}, err
	}
	if !r.available() {
		return inventory.MutationResult{}, inventoryUnavailable()
	}
	request, err := json.Marshal(canonical)
	if err != nil {
		return inventory.MutationResult{}, inventoryUnavailable()
	}

	tx, err := r.pool.inner.BeginTx(ctx, pgx.TxOptions{IsoLevel: pgx.ReadCommitted})
	if err != nil {
		return inventory.MutationResult{}, inventoryPGError(err)
	}
	defer rollbackInventory(tx)

	if _, err = tx.Exec(ctx, `SELECT pg_advisory_xact_lock($1)`, inventoryLockID(playerID, operationID)); err != nil {
		return inventory.MutationResult{}, inventoryPGError(err)
	}
	revision, err := ensureInventoryTx(ctx, tx, playerID)
	if err != nil {
		return inventory.MutationResult{}, err
	}

	var previousRequest, previousSnapshot []byte
	err = tx.QueryRow(ctx, `SELECT canonical_request,response_snapshot FROM player_inventory_operations
		WHERE player_id=$1 AND operation_id=$2`, playerID, operationID).Scan(&previousRequest, &previousSnapshot)
	if err == nil {
		if !bytes.Equal(previousRequest, request) {
			return inventory.MutationResult{}, apperror.New("IDEMPOTENCY_CONFLICT", "OperationId已绑定不同背包请求", false)
		}
		var replay inventory.MutationResult
		if json.Unmarshal(previousSnapshot, &replay) != nil || replay.OperationID != operationID {
			return inventory.MutationResult{}, inventoryUnavailable()
		}
		replay.Duplicate = true
		return replay, nil
	}
	if !errors.Is(err, pgx.ErrNoRows) {
		return inventory.MutationResult{}, inventoryPGError(err)
	}
	if revision != expectedRevision {
		return inventory.MutationResult{}, apperror.New("INVENTORY_REVISION_CONFLICT", "背包修订冲突", false)
	}

	moved, remaining, err := apply(ctx, tx)
	if err != nil {
		return inventory.MutationResult{}, err
	}
	if revision == math.MaxInt64 {
		return inventory.MutationResult{}, apperror.New("INVENTORY_REVISION_CONFLICT", "背包修订已达上限", false)
	}
	var newRevision int64
	if err = tx.QueryRow(ctx, `UPDATE player_inventories SET inventory_revision=inventory_revision+1,updated_at=NOW()
		WHERE player_id=$1 AND inventory_revision=$2 RETURNING inventory_revision`,
		playerID, expectedRevision).Scan(&newRevision); errors.Is(err, pgx.ErrNoRows) {
		return inventory.MutationResult{}, apperror.New("INVENTORY_REVISION_CONFLICT", "背包修订冲突", false)
	} else if err != nil {
		return inventory.MutationResult{}, inventoryPGError(err)
	}

	snapshot, err := readInventorySnapshotTx(ctx, tx, playerID)
	if err != nil {
		return inventory.MutationResult{}, err
	}
	if snapshot.InventoryRevision != newRevision {
		return inventory.MutationResult{}, inventoryUnavailable()
	}
	result := inventory.MutationResult{
		OperationID: operationID, Snapshot: snapshot,
		MovedQuantity: moved, RemainingQuantity: remaining,
	}
	response, err := json.Marshal(result)
	if err != nil {
		return inventory.MutationResult{}, inventoryUnavailable()
	}
	if _, err = tx.Exec(ctx, `INSERT INTO player_inventory_operations
		(player_id,operation_id,operation_type,canonical_request,response_snapshot)
		VALUES($1,$2,$3,$4,$5::jsonb)`,
		playerID, operationID, operationType, request, string(response)); err != nil {
		return inventory.MutationResult{}, inventoryPGError(err)
	}
	if err = commitInventory(ctx, tx); err != nil {
		return inventory.MutationResult{}, err
	}
	return result, nil
}

func ensureInventoryTx(ctx context.Context, tx pgx.Tx, playerID string) (int64, error) {
	tag, err := tx.Exec(ctx, `INSERT INTO player_inventories(player_id)
		SELECT player_id FROM player_profiles WHERE player_id=$1
		ON CONFLICT(player_id) DO NOTHING`, playerID)
	if err != nil {
		return 0, inventoryPGError(err)
	}
	_ = tag
	if _, err = tx.Exec(ctx, `INSERT INTO player_inventory_containers(player_id,container_id,capacity)
		SELECT player_id,$2,$3 FROM player_inventories WHERE player_id=$1
		ON CONFLICT(player_id,container_id) DO NOTHING`,
		playerID, inventory.DefaultContainerID, inventory.DefaultContainerCapacity); err != nil {
		return 0, inventoryPGError(err)
	}
	var revision int64
	err = tx.QueryRow(ctx, `SELECT inventory_revision FROM player_inventories WHERE player_id=$1 FOR UPDATE`, playerID).Scan(&revision)
	if errors.Is(err, pgx.ErrNoRows) {
		return 0, apperror.New("PLAYER_PROFILE_NOT_FOUND", "玩家资料不存在", false)
	}
	if err != nil {
		return 0, inventoryPGError(err)
	}
	return revision, nil
}

func readInventorySnapshotTx(ctx context.Context, tx pgx.Tx, playerID string) (inventory.Snapshot, error) {
	var snapshot inventory.Snapshot
	if err := tx.QueryRow(ctx, `SELECT inventory_revision FROM player_inventories WHERE player_id=$1`, playerID).
		Scan(&snapshot.InventoryRevision); err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}

	rows, err := tx.Query(ctx, `SELECT container_id,capacity FROM player_inventory_containers
		WHERE player_id=$1 ORDER BY container_id`, playerID)
	if err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}
	for rows.Next() {
		var item inventory.ContainerSnapshot
		if err = rows.Scan(&item.ContainerID, &item.Capacity); err != nil {
			rows.Close()
			return inventory.Snapshot{}, inventoryPGError(err)
		}
		snapshot.Containers = append(snapshot.Containers, item)
	}
	rows.Close()
	if err = rows.Err(); err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}

	rows, err = tx.Query(ctx, `SELECT item_instance_id,item_definition_id,quantity,container_id,slot_index,revision,instance_state,max_stack_size
		FROM player_inventory_items WHERE player_id=$1 ORDER BY container_id,slot_index,item_instance_id`, playerID)
	if err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}
	for rows.Next() {
		var item inventory.ItemInstance
		if err = rows.Scan(&item.ItemInstanceID, &item.ItemDefinitionID, &item.Quantity, &item.ContainerID,
			&item.SlotIndex, &item.Revision, &item.InstanceState, &item.MaxStackSize); err != nil {
			rows.Close()
			return inventory.Snapshot{}, inventoryPGError(err)
		}
		snapshot.Items = append(snapshot.Items, item)
		if len(snapshot.Items) > inventory.MaxSnapshotItems {
			rows.Close()
			return inventory.Snapshot{}, inventoryUnavailable()
		}
	}
	rows.Close()
	if err = rows.Err(); err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}

	rows, err = tx.Query(ctx, `SELECT slot_index,item_instance_id,revision FROM player_inventory_quickbar
		WHERE player_id=$1 ORDER BY slot_index`, playerID)
	if err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}
	for rows.Next() {
		var item inventory.QuickbarSlot
		if err = rows.Scan(&item.SlotIndex, &item.ItemInstanceID, &item.Revision); err != nil {
			rows.Close()
			return inventory.Snapshot{}, inventoryPGError(err)
		}
		snapshot.Quickbar = append(snapshot.Quickbar, item)
	}
	rows.Close()
	if err = rows.Err(); err != nil {
		return inventory.Snapshot{}, inventoryPGError(err)
	}
	return snapshot, nil
}

func inventoryContainerCapacity(ctx context.Context, tx pgx.Tx, playerID, containerID string) (int32, error) {
	var capacity int32
	err := tx.QueryRow(ctx, `SELECT capacity FROM player_inventory_containers
		WHERE player_id=$1 AND container_id=$2`, playerID, containerID).Scan(&capacity)
	if errors.Is(err, pgx.ErrNoRows) {
		return 0, apperror.New("INVENTORY_CONTAINER_NOT_FOUND", "背包容器不存在", false)
	}
	if err != nil {
		return 0, inventoryPGError(err)
	}
	return capacity, nil
}

func inventoryItemForUpdate(ctx context.Context, tx pgx.Tx, playerID, itemID string) (inventory.ItemInstance, error) {
	var item inventory.ItemInstance
	err := tx.QueryRow(ctx, `SELECT item_instance_id,item_definition_id,quantity,container_id,slot_index,revision,instance_state,max_stack_size
		FROM player_inventory_items WHERE player_id=$1 AND item_instance_id=$2 FOR UPDATE`,
		playerID, itemID).Scan(&item.ItemInstanceID, &item.ItemDefinitionID, &item.Quantity, &item.ContainerID,
		&item.SlotIndex, &item.Revision, &item.InstanceState, &item.MaxStackSize)
	if errors.Is(err, pgx.ErrNoRows) {
		return inventory.ItemInstance{}, inventoryItemNotFound()
	}
	if err != nil {
		return inventory.ItemInstance{}, inventoryPGError(err)
	}
	return item, nil
}

func inventorySplitItemID(operationID, sourceID string) string {
	sum := sha256.Sum256([]byte(operationID + "\x00" + sourceID))
	return "item-" + hex.EncodeToString(sum[:16])
}

func inventoryLockID(playerID, operationID string) int64 {
	encoded, _ := json.Marshal([2]string{playerID, operationID})
	digest := sha256.Sum256(encoded)
	return int64(binary.BigEndian.Uint64(digest[:8]))
}

func inventoryItemNotFound() error {
	return apperror.New("INVENTORY_ITEM_NOT_FOUND", "背包物品不存在", false)
}

func inventoryUnavailable() error {
	return apperror.New("SERVICE_UNAVAILABLE", "背包持久服务暂不可用", true)
}

func inventoryPGError(err error) error {
	if err == nil {
		return nil
	}
	if errors.Is(err, context.Canceled) || errors.Is(err, context.DeadlineExceeded) {
		return err
	}
	return inventoryUnavailable()
}

func rollbackInventory(tx pgx.Tx) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	_ = tx.Rollback(ctx)
}

func commitInventory(ctx context.Context, tx pgx.Tx) error {
	if err := tx.Commit(ctx); err != nil {
		return apperror.New("SERVICE_UNAVAILABLE", "背包事务提交结果未确认；必须保留原OperationId查询或重试", true)
	}
	return nil
}
