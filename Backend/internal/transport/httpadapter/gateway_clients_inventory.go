package httpadapter

import (
	"context"
	"net/url"

	"divinebeasts/backend/internal/modules/inventory"
)

func (c *PlayerDataClient) GetInventorySnapshot(ctx context.Context, playerID string) (inventory.Snapshot, error) {
	var out inventory.Snapshot
	if err := c.get(ctx, "/internal/v1/playerdata/inventory?playerId="+url.QueryEscape(playerID), &out); err != nil {
		return inventory.Snapshot{}, err
	}
	return out, nil
}

func (c *PlayerDataClient) GetInventoryOperation(ctx context.Context, playerID, operationID string) (inventory.MutationResult, error) {
	var out inventory.MutationResult
	path := "/internal/v1/playerdata/inventory/operations/" + url.PathEscape(operationID) + "?playerId=" + url.QueryEscape(playerID)
	if err := c.get(ctx, path, &out); err != nil {
		return inventory.MutationResult{}, err
	}
	return out, nil
}

func (c *PlayerDataClient) MoveInventory(ctx context.Context, playerID string, command inventory.MoveCommand) (inventory.MutationResult, error) {
	return c.inventoryMutation(ctx, "/internal/v1/playerdata/inventory/move", map[string]any{
		"playerId": playerID, "operationId": command.OperationID, "expectedRevision": command.ExpectedRevision,
		"itemInstanceId": command.ItemInstanceID, "targetContainerId": command.TargetContainerID, "targetSlotIndex": command.TargetSlotIndex,
	})
}

func (c *PlayerDataClient) SplitInventory(ctx context.Context, playerID string, command inventory.SplitCommand) (inventory.MutationResult, error) {
	return c.inventoryMutation(ctx, "/internal/v1/playerdata/inventory/split", map[string]any{
		"playerId": playerID, "operationId": command.OperationID, "expectedRevision": command.ExpectedRevision,
		"sourceItemInstanceId": command.SourceItemInstanceID, "splitQuantity": command.SplitQuantity,
		"targetContainerId": command.TargetContainerID, "targetSlotIndex": command.TargetSlotIndex,
	})
}

func (c *PlayerDataClient) MergeInventory(ctx context.Context, playerID string, command inventory.MergeCommand) (inventory.MutationResult, error) {
	return c.inventoryMutation(ctx, "/internal/v1/playerdata/inventory/merge", map[string]any{
		"playerId": playerID, "operationId": command.OperationID, "expectedRevision": command.ExpectedRevision,
		"sourceItemInstanceId": command.SourceItemInstanceID, "targetItemInstanceId": command.TargetItemInstanceID,
	})
}

func (c *PlayerDataClient) SetInventoryQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	return c.inventoryMutation(ctx, "/internal/v1/playerdata/inventory/quickbar/set", map[string]any{
		"playerId": playerID, "operationId": command.OperationID, "expectedRevision": command.ExpectedRevision,
		"slotIndex": command.SlotIndex, "itemInstanceId": command.ItemInstanceID,
	})
}

func (c *PlayerDataClient) ClearInventoryQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	return c.inventoryMutation(ctx, "/internal/v1/playerdata/inventory/quickbar/clear", map[string]any{
		"playerId": playerID, "operationId": command.OperationID, "expectedRevision": command.ExpectedRevision,
		"slotIndex": command.SlotIndex,
	})
}

func (c *PlayerDataClient) inventoryMutation(ctx context.Context, path string, payload map[string]any) (inventory.MutationResult, error) {
	var out inventory.MutationResult
	if err := c.post(ctx, path, payload, &out); err != nil {
		return inventory.MutationResult{}, err
	}
	return out, nil
}
