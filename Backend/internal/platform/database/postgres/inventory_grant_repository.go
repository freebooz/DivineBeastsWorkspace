//go:build productiondeps

package postgres

// 本文件为PostgreSQL长期背包可信授予持久化适配：在玩家聚合事务内
// 同时写入堆叠/新实例/修订号/幂等操作和集成事件Outbox。
// 服务器奖励规则由上层Authorizer核验，这里绝不接受公网请求或项目UI内容。

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"

	"github.com/jackc/pgx/v5"

	"divinebeasts/backend/internal/modules/inventory"
)

// Grant（可信授予仓储）先生成完整纯计划，背包放不下时整体失败且不留下物品。
// 操作号与权威MaxStackSize一同参与幂等请求指纹；上层已经校验服务器身份。
func (r *InventoryRepository) Grant(
	ctx context.Context, playerID string, command inventory.GrantCommand, rule inventory.GrantRule,
) (inventory.MutationResult, error) {
	canonical := struct {
		Command inventory.GrantCommand
		Rule    inventory.GrantRule
	}{Command: command, Rule: rule}
	return r.mutate(ctx, playerID, command.OperationID, "Grant", command.ExpectedRevision, canonical,
		func(ctx context.Context, tx pgx.Tx) (int32, int32, error) {
			// mutate已经以FOR UPDATE锁定玩家聚合根，其他写操作不能插队占用计划槽位。
			before, err := readInventorySnapshotTx(ctx, tx, playerID)
			if err != nil {
				return 0, command.Quantity, err
			}
			plan, err := inventory.PlanTrustedGrant(playerID, before, command, rule)
			if err != nil {
				return 0, command.Quantity, err
			}
			for _, item := range plan.Updates {
				tag, updateErr := tx.Exec(ctx, `UPDATE player_inventory_items
					SET quantity=$3,revision=revision+1
					WHERE player_id=$1 AND item_instance_id=$2 AND revision=$4`,
					playerID, item.ItemInstanceID, item.Quantity, item.Revision-1)
				if updateErr != nil {
					return 0, command.Quantity, inventoryPGError(updateErr)
				}
				if tag.RowsAffected() != 1 {
					return 0, command.Quantity, inventoryUnavailable()
				}
			}
			for _, item := range plan.Creates {
				if _, insertErr := tx.Exec(ctx, `INSERT INTO player_inventory_items
					(player_id,item_instance_id,item_definition_id,quantity,container_id,slot_index,revision,instance_state,max_stack_size)
					VALUES($1,$2,$3,$4,$5,$6,$7,$8,$9)`,
					playerID, item.ItemInstanceID, item.ItemDefinitionID, item.Quantity,
					item.ContainerID, item.SlotIndex, item.Revision, item.InstanceState, item.MaxStackSize); insertErr != nil {
					return 0, command.Quantity, inventoryPGError(insertErr)
				}
			}
			// Outbox与背包操作处于同一个tx；消费者必须按message_id去重，
			// 事件表示库存授予事实，不代表用户已看到提示或完成视觉播放。
			event := inventoryOutboxEvent{
				SchemaVersion:    1,
				OperationID:      command.OperationID,
				SourceEventID:    command.SourceEventID,
				GameServerID:     command.GameServerID,
				ServerBootID:     command.ServerBootID,
				PlayerID:         playerID,
				ItemDefinitionID: command.ItemDefinitionID,
				Quantity:         command.Quantity,
				ReasonCode:       command.ReasonCode,
			}
			if err := enqueueInventoryOutboxTx(ctx, tx, "Inventory.Granted",
				playerID, command.OperationID, event); err != nil {
				return 0, command.Quantity, err
			}
			return command.Quantity, 0, nil
		})
}

// inventoryOutboxEvent（内部背包事件信封）仅限Go后端集成事件。
// 不把游戏服身份文本当认证凭据，也不包含客户端可自行提出的奖励指令。
type inventoryOutboxEvent struct {
	SchemaVersion     int    `json:"schemaVersion"`
	OperationID       string `json:"operationId"`
	SourceEventID     string `json:"sourceEventId,omitempty"`
	GameServerID      string `json:"gameServerId,omitempty"`
	ServerBootID      string `json:"serverBootId,omitempty"`
	PlayerID          string `json:"playerId"`
	ItemDefinitionID  string `json:"itemDefinitionId"`
	Quantity          int32  `json:"quantity"`
	RemainingQuantity int32  `json:"remainingQuantity,omitempty"`
	ReasonCode        string `json:"reasonCode"`
}

// enqueueInventoryOutboxTx（事务内发件箱）不能在业务tx外调用；事件ID跨玩家隔离且可重算。
// 这里只持久保存pending消息，正式Dispatcher从既有Outbox发布到NATS JetStream。
func enqueueInventoryOutboxTx(
	ctx context.Context, tx pgx.Tx, topic, playerID, operationID string, body inventoryOutboxEvent,
) error {
	payload, err := json.Marshal(body)
	if err != nil {
		return inventoryUnavailable()
	}
	digest := sha256.Sum256([]byte(playerID + "\x00" + topic + "\x00" + operationID))
	messageID := "inventory-" + hex.EncodeToString(digest[:16])
	_, err = tx.Exec(ctx, `INSERT INTO outbox_messages
		(message_id,topic,aggregate_id,payload,occurred_at)
		VALUES($1,$2,$3,$4::jsonb,NOW())`,
		messageID, topic, playerID, string(payload))
	return inventoryPGError(err)
}
