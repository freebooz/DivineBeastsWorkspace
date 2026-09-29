package playerdata

import (
	"context"

	"divinebeasts/backend/internal/modules/inventory"
)

// InventoryService（背包服务端口）让PlayerDataService托管长期背包，而不把背包规则复制进玩家资料领域。
type InventoryService interface {
	GetSnapshot(context.Context, string) (inventory.Snapshot, error)
	GetOperation(context.Context, string, string) (inventory.MutationResult, error)
	Move(context.Context, string, inventory.MoveCommand) (inventory.MutationResult, error)
	Split(context.Context, string, inventory.SplitCommand) (inventory.MutationResult, error)
	Merge(context.Context, string, inventory.MergeCommand) (inventory.MutationResult, error)
	SetQuickbar(context.Context, string, inventory.QuickbarCommand) (inventory.MutationResult, error)
	ClearQuickbar(context.Context, string, inventory.QuickbarCommand) (inventory.MutationResult, error)
	Probe(context.Context) error
}

// AttachInventory（装配背包服务）只允许Composition Root显式调用；nil表示当前部署未启用背包。
func (s *Service) AttachInventory(service InventoryService) {
	if s != nil {
		s.inventory = service
	}
}

func (s *Service) GetInventorySnapshot(ctx context.Context, playerID string) (inventory.Snapshot, error) {
	if s == nil || s.inventory == nil {
		return inventory.Snapshot{}, unavailable()
	}
	return s.inventory.GetSnapshot(ctx, playerID)
}

func (s *Service) GetInventoryOperation(ctx context.Context, playerID, operationID string) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.GetOperation(ctx, playerID, operationID)
}

func (s *Service) MoveInventory(ctx context.Context, playerID string, command inventory.MoveCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.Move(ctx, playerID, command)
}

func (s *Service) SplitInventory(ctx context.Context, playerID string, command inventory.SplitCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.Split(ctx, playerID, command)
}

func (s *Service) MergeInventory(ctx context.Context, playerID string, command inventory.MergeCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.Merge(ctx, playerID, command)
}

func (s *Service) SetInventoryQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.SetQuickbar(ctx, playerID, command)
}

func (s *Service) ClearInventoryQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.ClearQuickbar(ctx, playerID, command)
}

func (s *Service) ProbeInventory(ctx context.Context) error {
	if s == nil || s.inventory == nil {
		return unavailable()
	}
	return s.inventory.Probe(ctx)
}
