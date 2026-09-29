package inventory

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"encoding/json"
	"math"
	"strings"
	"sync"
	"unicode/utf8"

	"divinebeasts/backend/internal/platform/apperror"
)

const (
	DefaultContainerID       = "main"
	DefaultContainerCapacity = 64
	MaxQuickbarSlots         = 12
	MaxSnapshotItems         = 10000
	MaxIdentifierRunes       = 256
)

// ItemInstance（物品实例）是长期背包中的权威持有记录；不包含装备槽位、货币余额或实时战斗状态。
type ItemInstance struct {
	ItemInstanceID   string `json:"itemInstanceId"`
	ItemDefinitionID string `json:"itemDefinitionId"`
	Quantity         int32  `json:"quantity"`
	ContainerID      string `json:"containerId"`
	SlotIndex        int32  `json:"slotIndex"`
	Revision         int64  `json:"revision"`
	InstanceState    string `json:"instanceState"`
	MaxStackSize     int32  `json:"maxStackSize"`
}

// QuickbarSlot（快捷栏槽位）只保存对背包实例的稳定引用。
type QuickbarSlot struct {
	SlotIndex      int32  `json:"slotIndex"`
	ItemInstanceID string `json:"itemInstanceId"`
	Revision       int64  `json:"revision"`
}

// ContainerSnapshot（容器快照）描述容量边界；仓库等新增容器通过数据扩展，不改变背包核心算法。
type ContainerSnapshot struct {
	ContainerID string `json:"containerId"`
	Capacity    int32  `json:"capacity"`
}

// Snapshot（背包快照）是PlayerData返回给调用方的单一长期真源视图。
type Snapshot struct {
	InventoryRevision int64               `json:"inventoryRevision"`
	Containers        []ContainerSnapshot `json:"containers"`
	Items             []ItemInstance      `json:"items"`
	Quickbar          []QuickbarSlot      `json:"quickbar"`
}

// MutationResult（背包写结果）持久保存完整提交快照，供结果未知后的同OperationId安全查询/重放。
type MutationResult struct {
	OperationID       string   `json:"operationId"`
	Snapshot          Snapshot `json:"snapshot"`
	MovedQuantity     int32    `json:"movedQuantity"`
	RemainingQuantity int32    `json:"remainingQuantity"`
	Duplicate         bool     `json:"duplicate"`
}

type MoveCommand struct {
	OperationID       string
	ExpectedRevision  int64
	ItemInstanceID    string
	TargetContainerID string
	TargetSlotIndex   int32
}

type SplitCommand struct {
	OperationID          string
	ExpectedRevision     int64
	SourceItemInstanceID string
	SplitQuantity        int32
	TargetContainerID    string
	TargetSlotIndex      int32
}

type MergeCommand struct {
	OperationID          string
	ExpectedRevision     int64
	SourceItemInstanceID string
	TargetItemInstanceID string
}

type QuickbarCommand struct {
	OperationID      string
	ExpectedRevision int64
	SlotIndex        int32
	ItemInstanceID   string
}

// Repository（背包仓储端口）保证每个Mutation在单一事务/写锁内完成并持久化Operation结果。
type Repository interface {
	GetSnapshot(context.Context, string) (Snapshot, error)
	GetOperation(context.Context, string, string) (MutationResult, error)
	Move(context.Context, string, MoveCommand) (MutationResult, error)
	Split(context.Context, string, SplitCommand) (MutationResult, error)
	Merge(context.Context, string, MergeCommand) (MutationResult, error)
	SetQuickbar(context.Context, string, QuickbarCommand) (MutationResult, error)
	ClearQuickbar(context.Context, string, QuickbarCommand) (MutationResult, error)
	Probe(context.Context) error
}

// Service（背包领域服务）只负责输入约束与仓储编排；身份归属由上游Gateway/PlayerData可信链确定。
type Service struct{ repo Repository }

func NewService(repo Repository) *Service { return &Service{repo: repo} }

func (s *Service) GetSnapshot(ctx context.Context, playerID string) (Snapshot, error) {
	if err := validatePlayer(playerID); err != nil {
		return Snapshot{}, err
	}
	if s == nil || s.repo == nil {
		return Snapshot{}, unavailable()
	}
	return s.repo.GetSnapshot(ctx, playerID)
}

func (s *Service) GetOperation(ctx context.Context, playerID, operationID string) (MutationResult, error) {
	if err := validatePlayer(playerID); err != nil {
		return MutationResult{}, err
	}
	if err := validateOperationID(operationID); err != nil {
		return MutationResult{}, err
	}
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	return s.repo.GetOperation(ctx, playerID, operationID)
}

func (s *Service) Move(ctx context.Context, playerID string, cmd MoveCommand) (MutationResult, error) {
	if err := validateMutationBase(playerID, cmd.OperationID, cmd.ExpectedRevision); err != nil {
		return MutationResult{}, err
	}
	if !validID(cmd.ItemInstanceID) || !validID(cmd.TargetContainerID) || cmd.TargetSlotIndex < 0 {
		return MutationResult{}, invalidRequest()
	}
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	return s.repo.Move(ctx, playerID, cmd)
}

func (s *Service) Split(ctx context.Context, playerID string, cmd SplitCommand) (MutationResult, error) {
	if err := validateMutationBase(playerID, cmd.OperationID, cmd.ExpectedRevision); err != nil {
		return MutationResult{}, err
	}
	if !validID(cmd.SourceItemInstanceID) || !validID(cmd.TargetContainerID) || cmd.SplitQuantity <= 0 || cmd.TargetSlotIndex < 0 {
		return MutationResult{}, invalidQuantity()
	}
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	return s.repo.Split(ctx, playerID, cmd)
}

func (s *Service) Merge(ctx context.Context, playerID string, cmd MergeCommand) (MutationResult, error) {
	if err := validateMutationBase(playerID, cmd.OperationID, cmd.ExpectedRevision); err != nil {
		return MutationResult{}, err
	}
	if !validID(cmd.SourceItemInstanceID) || !validID(cmd.TargetItemInstanceID) || cmd.SourceItemInstanceID == cmd.TargetItemInstanceID {
		return MutationResult{}, invalidRequest()
	}
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	return s.repo.Merge(ctx, playerID, cmd)
}

func (s *Service) SetQuickbar(ctx context.Context, playerID string, cmd QuickbarCommand) (MutationResult, error) {
	if err := validateMutationBase(playerID, cmd.OperationID, cmd.ExpectedRevision); err != nil {
		return MutationResult{}, err
	}
	if cmd.SlotIndex < 0 || cmd.SlotIndex >= MaxQuickbarSlots || !validID(cmd.ItemInstanceID) {
		return MutationResult{}, slotOutOfRange()
	}
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	return s.repo.SetQuickbar(ctx, playerID, cmd)
}

func (s *Service) ClearQuickbar(ctx context.Context, playerID string, cmd QuickbarCommand) (MutationResult, error) {
	if err := validateMutationBase(playerID, cmd.OperationID, cmd.ExpectedRevision); err != nil {
		return MutationResult{}, err
	}
	if cmd.SlotIndex < 0 || cmd.SlotIndex >= MaxQuickbarSlots {
		return MutationResult{}, slotOutOfRange()
	}
	cmd.ItemInstanceID = ""
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	return s.repo.ClearQuickbar(ctx, playerID, cmd)
}

func (s *Service) Probe(ctx context.Context) error {
	if s == nil || s.repo == nil {
		return unavailable()
	}
	return s.repo.Probe(ctx)
}

func validateMutationBase(playerID, operationID string, revision int64) error {
	if err := validatePlayer(playerID); err != nil {
		return err
	}
	if err := validateOperationID(operationID); err != nil {
		return err
	}
	if revision < 1 {
		return revisionConflict()
	}
	return nil
}

func validatePlayer(playerID string) error {
	if !validID(playerID) {
		return invalidRequest()
	}
	return nil
}

func validateOperationID(value string) error {
	if !validID(value) || utf8.RuneCountInString(value) > 128 {
		return invalidRequest()
	}
	return nil
}

func validID(value string) bool {
	value = strings.TrimSpace(value)
	return value != "" &&
		utf8.ValidString(value) &&
		!strings.ContainsRune(value, 0) &&
		utf8.RuneCountInString(value) <= MaxIdentifierRunes
}

func invalidRequest() error {
	return apperror.New("INVALID_REQUEST", "背包请求参数非法", false)
}
func invalidQuantity() error {
	return apperror.New("INVENTORY_INVALID_QUANTITY", "背包数量非法", false)
}
func itemNotFound() error {
	return apperror.New("INVENTORY_ITEM_NOT_FOUND", "背包物品不存在", false)
}
func operationNotFound() error {
	return apperror.New("INVENTORY_OPERATION_NOT_FOUND", "背包操作不存在", false)
}
func revisionConflict() error {
	return apperror.New("INVENTORY_REVISION_CONFLICT", "背包修订冲突", false)
}
func slotOutOfRange() error {
	return apperror.New("INVENTORY_SLOT_OUT_OF_RANGE", "背包槽位越界", false)
}
func slotOccupied() error {
	return apperror.New("INVENTORY_SLOT_OCCUPIED", "目标槽位已占用", false)
}
func containerNotFound() error {
	return apperror.New("INVENTORY_CONTAINER_NOT_FOUND", "背包容器不存在", false)
}
func stackLimitExceeded() error {
	return apperror.New("INVENTORY_STACK_LIMIT_EXCEEDED", "合并后超过权威堆叠上限", false)
}
func insufficientQuantity() error {
	return apperror.New("INVENTORY_INSUFFICIENT_QUANTITY", "物品数量不足", false)
}
func idempotencyConflict() error {
	return apperror.New("IDEMPOTENCY_CONFLICT", "OperationId已绑定不同背包请求", false)
}
func unavailable() error {
	return apperror.New("SERVICE_UNAVAILABLE", "背包持久服务暂不可用", true)
}

// MemoryRepository（内存背包仓储）用于本地联调和单元测试；语义与生产仓储保持一致，不作为生产真源。
type MemoryRepository struct {
	mu     sync.RWMutex
	states map[string]*memoryState
}

type memoryState struct {
	revision   int64
	containers map[string]ContainerSnapshot
	items      map[string]ItemInstance
	quickbar   map[int32]QuickbarSlot
	operations map[string]memoryOperation
}

type memoryOperation struct {
	signature string
	result    MutationResult
}

func NewMemoryRepository() *MemoryRepository {
	return &MemoryRepository{states: map[string]*memoryState{}}
}

func newMemoryState() *memoryState {
	return &memoryState{
		revision: 1,
		containers: map[string]ContainerSnapshot{
			DefaultContainerID: {ContainerID: DefaultContainerID, Capacity: DefaultContainerCapacity},
		},
		items:      map[string]ItemInstance{},
		quickbar:   map[int32]QuickbarSlot{},
		operations: map[string]memoryOperation{},
	}
}

func (r *MemoryRepository) stateLocked(playerID string) *memoryState {
	state := r.states[playerID]
	if state == nil {
		state = newMemoryState()
		r.states[playerID] = state
	}
	return state
}

func (r *MemoryRepository) GetSnapshot(ctx context.Context, playerID string) (Snapshot, error) {
	if err := ctx.Err(); err != nil {
		return Snapshot{}, err
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	return snapshotOf(r.stateLocked(playerID)), nil
}

func (r *MemoryRepository) GetOperation(ctx context.Context, playerID, operationID string) (MutationResult, error) {
	if err := ctx.Err(); err != nil {
		return MutationResult{}, err
	}
	r.mu.RLock()
	defer r.mu.RUnlock()
	state := r.states[playerID]
	if state == nil {
		return MutationResult{}, operationNotFound()
	}
	record, ok := state.operations[operationID]
	if !ok {
		return MutationResult{}, operationNotFound()
	}
	result := cloneResult(record.result)
	result.Duplicate = true
	return result, nil
}

func (r *MemoryRepository) Move(ctx context.Context, playerID string, cmd MoveCommand) (MutationResult, error) {
	return r.mutate(ctx, playerID, cmd.OperationID, signature("move", cmd), cmd.ExpectedRevision,
		func(state *memoryState) (int32, int32, error) {
			item, ok := state.items[cmd.ItemInstanceID]
			if !ok {
				return 0, 0, itemNotFound()
			}
			container, ok := state.containers[cmd.TargetContainerID]
			if !ok {
				return 0, 0, containerNotFound()
			}
			if cmd.TargetSlotIndex >= container.Capacity {
				return 0, 0, slotOutOfRange()
			}
			if item.ContainerID == cmd.TargetContainerID && item.SlotIndex == cmd.TargetSlotIndex {
				return 0, item.Quantity, invalidRequest()
			}
			targetID := findAt(state.items, cmd.TargetContainerID, cmd.TargetSlotIndex)
			if targetID != "" {
				target := state.items[targetID]
				target.ContainerID, target.SlotIndex = item.ContainerID, item.SlotIndex
				target.Revision++
				state.items[targetID] = target
			}
			item.ContainerID, item.SlotIndex = cmd.TargetContainerID, cmd.TargetSlotIndex
			item.Revision++
			state.items[cmd.ItemInstanceID] = item
			return item.Quantity, 0, nil
		})
}

func (r *MemoryRepository) Split(ctx context.Context, playerID string, cmd SplitCommand) (MutationResult, error) {
	return r.mutate(ctx, playerID, cmd.OperationID, signature("split", cmd), cmd.ExpectedRevision,
		func(state *memoryState) (int32, int32, error) {
			source, ok := state.items[cmd.SourceItemInstanceID]
			if !ok {
				return 0, 0, itemNotFound()
			}
			if cmd.SplitQuantity <= 0 || cmd.SplitQuantity >= source.Quantity {
				return 0, source.Quantity, insufficientQuantity()
			}
			container, ok := state.containers[cmd.TargetContainerID]
			if !ok {
				return 0, source.Quantity, containerNotFound()
			}
			if cmd.TargetSlotIndex >= container.Capacity {
				return 0, source.Quantity, slotOutOfRange()
			}
			if findAt(state.items, cmd.TargetContainerID, cmd.TargetSlotIndex) != "" {
				return 0, source.Quantity, slotOccupied()
			}
			newID := deterministicSplitID(cmd.OperationID, cmd.SourceItemInstanceID)
			if _, exists := state.items[newID]; exists {
				return 0, source.Quantity, idempotencyConflict()
			}
			source.Quantity -= cmd.SplitQuantity
			source.Revision++
			state.items[source.ItemInstanceID] = source
			state.items[newID] = ItemInstance{
				ItemInstanceID: newID, ItemDefinitionID: source.ItemDefinitionID, Quantity: cmd.SplitQuantity,
				ContainerID: cmd.TargetContainerID, SlotIndex: cmd.TargetSlotIndex, Revision: 1,
				InstanceState: source.InstanceState, MaxStackSize: source.MaxStackSize,
			}
			return cmd.SplitQuantity, source.Quantity, nil
		})
}

func (r *MemoryRepository) Merge(ctx context.Context, playerID string, cmd MergeCommand) (MutationResult, error) {
	return r.mutate(ctx, playerID, cmd.OperationID, signature("merge", cmd), cmd.ExpectedRevision,
		func(state *memoryState) (int32, int32, error) {
			source, sourceOK := state.items[cmd.SourceItemInstanceID]
			target, targetOK := state.items[cmd.TargetItemInstanceID]
			if !sourceOK || !targetOK {
				return 0, 0, itemNotFound()
			}
			if source.ItemDefinitionID != target.ItemDefinitionID {
				return 0, source.Quantity, invalidRequest()
			}
			maxStack := target.MaxStackSize
			if maxStack < 1 {
				maxStack = 1
			}
			total := int64(source.Quantity) + int64(target.Quantity)
			if total > int64(maxStack) || total > math.MaxInt32 {
				return 0, source.Quantity, stackLimitExceeded()
			}
			target.Quantity = int32(total)
			target.Revision++
			state.items[target.ItemInstanceID] = target
			delete(state.items, source.ItemInstanceID)
			for slot, entry := range state.quickbar {
				if entry.ItemInstanceID == source.ItemInstanceID {
					entry.ItemInstanceID = target.ItemInstanceID
					entry.Revision++
					state.quickbar[slot] = entry
				}
			}
			return source.Quantity, 0, nil
		})
}

func (r *MemoryRepository) SetQuickbar(ctx context.Context, playerID string, cmd QuickbarCommand) (MutationResult, error) {
	return r.mutate(ctx, playerID, cmd.OperationID, signature("quickbar-set", cmd), cmd.ExpectedRevision,
		func(state *memoryState) (int32, int32, error) {
			if _, ok := state.items[cmd.ItemInstanceID]; !ok {
				return 0, 0, itemNotFound()
			}
			entry := state.quickbar[cmd.SlotIndex]
			entry.SlotIndex = cmd.SlotIndex
			entry.ItemInstanceID = cmd.ItemInstanceID
			entry.Revision++
			if entry.Revision < 1 {
				entry.Revision = 1
			}
			state.quickbar[cmd.SlotIndex] = entry
			return 0, 0, nil
		})
}

func (r *MemoryRepository) ClearQuickbar(ctx context.Context, playerID string, cmd QuickbarCommand) (MutationResult, error) {
	return r.mutate(ctx, playerID, cmd.OperationID, signature("quickbar-clear", cmd), cmd.ExpectedRevision,
		func(state *memoryState) (int32, int32, error) {
			delete(state.quickbar, cmd.SlotIndex)
			return 0, 0, nil
		})
}

func (r *MemoryRepository) Probe(ctx context.Context) error {
	return ctx.Err()
}

// SeedForTest（测试预置）只用于自动化/本地夹具；生产代码不得用它授予物品。
func (r *MemoryRepository) SeedForTest(playerID string, items []ItemInstance) error {
	if !validID(playerID) || len(items) > MaxSnapshotItems {
		return invalidRequest()
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	state := r.stateLocked(playerID)
	for _, item := range items {
		if !validID(item.ItemInstanceID) || !validID(item.ItemDefinitionID) || item.Quantity < 1 || item.Revision < 1 {
			return invalidRequest()
		}
		if item.ContainerID == "" {
			item.ContainerID = DefaultContainerID
		}
		if item.InstanceState == "" {
			item.InstanceState = "active"
		}
		if item.MaxStackSize < item.Quantity {
			item.MaxStackSize = item.Quantity
		}
		if findAt(state.items, item.ContainerID, item.SlotIndex) != "" {
			return slotOccupied()
		}
		state.items[item.ItemInstanceID] = item
	}
	return nil
}

func (r *MemoryRepository) mutate(
	ctx context.Context,
	playerID, operationID, sig string,
	expectedRevision int64,
	apply func(*memoryState) (int32, int32, error),
) (MutationResult, error) {
	if err := ctx.Err(); err != nil {
		return MutationResult{}, err
	}
	r.mu.Lock()
	defer r.mu.Unlock()

	state := r.stateLocked(playerID)
	if record, exists := state.operations[operationID]; exists {
		if record.signature != sig {
			return MutationResult{}, idempotencyConflict()
		}
		result := cloneResult(record.result)
		result.Duplicate = true
		return result, nil
	}
	if state.revision != expectedRevision {
		return MutationResult{}, revisionConflict()
	}

	moved, remaining, err := apply(state)
	if err != nil {
		return MutationResult{}, err
	}
	if state.revision == math.MaxInt64 {
		return MutationResult{}, revisionConflict()
	}
	state.revision++
	result := MutationResult{
		OperationID: operationID, Snapshot: snapshotOf(state),
		MovedQuantity: moved, RemainingQuantity: remaining,
	}
	state.operations[operationID] = memoryOperation{signature: sig, result: cloneResult(result)}
	return result, nil
}

func snapshotOf(state *memoryState) Snapshot {
	out := Snapshot{InventoryRevision: state.revision}
	out.Containers = make([]ContainerSnapshot, 0, len(state.containers))
	for _, container := range state.containers {
		out.Containers = append(out.Containers, container)
	}
	out.Items = make([]ItemInstance, 0, len(state.items))
	for _, item := range state.items {
		out.Items = append(out.Items, item)
	}
	out.Quickbar = make([]QuickbarSlot, 0, len(state.quickbar))
	for _, entry := range state.quickbar {
		out.Quickbar = append(out.Quickbar, entry)
	}
	return out
}

func cloneResult(in MutationResult) MutationResult {
	out := in
	out.Snapshot.Containers = append([]ContainerSnapshot(nil), in.Snapshot.Containers...)
	out.Snapshot.Items = append([]ItemInstance(nil), in.Snapshot.Items...)
	out.Snapshot.Quickbar = append([]QuickbarSlot(nil), in.Snapshot.Quickbar...)
	return out
}

func findAt(items map[string]ItemInstance, containerID string, slot int32) string {
	for id, item := range items {
		if item.ContainerID == containerID && item.SlotIndex == slot {
			return id
		}
	}
	return ""
}

func deterministicSplitID(operationID, sourceID string) string {
	sum := sha256.Sum256([]byte(operationID + "\x00" + sourceID))
	return "item-" + hex.EncodeToString(sum[:16])
}

func signature(kind string, value any) string {
	data, _ := json.Marshal(struct {
		Kind  string `json:"kind"`
		Value any    `json:"value"`
	}{kind, value})
	sum := sha256.Sum256(data)
	return hex.EncodeToString(sum[:])
}
