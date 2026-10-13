package inventory

// 本文件属于GamePlatform长期背包领域：可信授予的授权合同与纯放置计划。
// 只有后端验证过的服务器事件可以调用GrantTrusted；计划器不是权限边界，
// 物品显示DataAsset（客户端数据资产）不得决定MaxStackSize（权威堆叠上限）。

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
	"math"
	"sort"

	"divinebeasts/backend/internal/platform/apperror"
)

const (
	// MaxGrantQuantity（单次授予最大数量）防止恶意批量分配和整数计算溢出。
	MaxGrantQuantity int32 = 1000000
	// MaxGrantStackSize（权威单堆最大数量）由可信规则目录决定，上限仅是安全约束。
	MaxGrantStackSize int32 = 1000000
)

// GrantCommand（可信服务器授予命令）承载服务器事件引用、玩家与库存乐观锁。
// GameServerID/ServerBootID/SourceEventID仅是待核验声明，不代表身份认证成功。
// ReasonCode允许pickup（拾取）、quest_reward（任务奖励）、scripted_reward（剧情奖励）。
type GrantCommand struct {
	OperationID      string
	ExpectedRevision int64
	GameServerID     string
	ServerBootID     string
	SourceEventID    string
	ReasonCode       string
	ItemDefinitionID string
	Quantity         int32
}

// GrantRule（可信物品规则）只由服务端正式目录/授权器返回，不得由HTTP客户端传入。
type GrantRule struct {
	MaxStackSize int32
}

// TrustedGrantAuthorizer（可信授予授权器）必须先证明服务器身份/启动代次、
// 当前玩家所属世界实例和来源事件未伪造，再匹配权威定义、奖励数量与堆叠上限。
// 返回的GrantRule必须来自后端规则库；没有真实授权器时Service默认拒绝。
type TrustedGrantAuthorizer interface {
	AuthorizeInventoryGrant(context.Context, string, GrantCommand) (GrantRule, error)
}

// GrantPlan（授予写计划）不包含任何越权逻辑；纯函数事先计算受影响的堆叠和新实例。
// Repository必须在持有玩家聚合锁/数据库事务后使用此计划，全部成功才提交。
type GrantPlan struct {
	Updates []ItemInstance
	Creates []ItemInstance
}

// NewTrustedGrantService（可信授予服务装配）只用于后端受控服务组合根。
// 不提供外部HTTP路由。未注入authorizer时Service.GrantTrusted始终拒绝。
func NewTrustedGrantService(repo Repository, grantAuthorizer TrustedGrantAuthorizer, consumeAuthorizer TrustedConsumeAuthorizer) *Service {
	return &Service{
		repo: repo,
		trustedGrantAuthorizer: grantAuthorizer,
		trustedConsumeAuthorizer: consumeAuthorizer,
	}
}

// GrantOperationID（可信事件确定性操作号）由玩家ID与已核实的来源事件ID形成。
// 服务端不得为同一掉落/任务完成事件重新创建随机操作号，以免重复发奖。
func GrantOperationID(playerID, sourceEventID string) string {
	sum := sha256.Sum256([]byte(playerID + "\x00" + sourceEventID))
	return "grant-" + hex.EncodeToString(sum[:16])
}

// GrantTrusted（可信授予）先校验稳定事件、乐观锁和服务端Authorizer，再进入仓储原子写。
// 领取任务奖励、拾取和邮件发奖只能通过经校验的权威事件进入，不接受公网客户端自报。
func (s *Service) GrantTrusted(ctx context.Context, playerID string, cmd GrantCommand) (MutationResult, error) {
	if err := validateMutationBase(playerID, cmd.OperationID, cmd.ExpectedRevision); err != nil {
		return MutationResult{}, err
	}
	if !validID(cmd.GameServerID) || !validID(cmd.ServerBootID) ||
		!validID(cmd.SourceEventID) || !validID(cmd.ItemDefinitionID) ||
		!validID(cmd.ReasonCode) || cmd.Quantity < 1 || cmd.Quantity > MaxGrantQuantity ||
		cmd.OperationID != GrantOperationID(playerID, cmd.SourceEventID) {
		return MutationResult{}, invalidRequest()
	}
	if s == nil || s.repo == nil {
		return MutationResult{}, unavailable()
	}
	if s.trustedGrantAuthorizer == nil {
		return MutationResult{}, grantNotAllowed()
	}
	rule, err := s.trustedGrantAuthorizer.AuthorizeInventoryGrant(ctx, playerID, cmd)
	if err != nil {
		return MutationResult{}, err
	}
	if rule.MaxStackSize < 1 || rule.MaxStackSize > MaxGrantStackSize {
		return MutationResult{}, grantNotAllowed()
	}
	return s.repo.Grant(ctx, playerID, cmd, rule)
}

func grantNotAllowed() error {
	return apperror.New("INVENTORY_GRANT_NOT_ALLOWED", "未验证的服务器奖励事件不能授予背包物品", false)
}

func inventoryFull() error {
	return apperror.New("INVENTORY_FULL", "背包剩余容量不足，授予未执行", false)
}

// PlanTrustedGrant（可信授予纯放置计划）：先填充同定义且同权威堆叠上限的活跃物品，
// 再按容器ID、槽位递增选择空格；不修改入参，所有无法完整安置的奖励整体失败。
// 真实规则来源、玩家和事件身份由GrantTrusted或上游可信组合根完成验证。
func PlanTrustedGrant(playerID string, before Snapshot, cmd GrantCommand, rule GrantRule) (GrantPlan, error) {
	if !validID(playerID) || !validID(cmd.ItemDefinitionID) ||
		cmd.Quantity < 1 || cmd.Quantity > MaxGrantQuantity ||
		rule.MaxStackSize < 1 || rule.MaxStackSize > MaxGrantStackSize ||
		len(before.Items) > MaxSnapshotItems || len(before.Containers) == 0 {
		return GrantPlan{}, invalidRequest()
	}

	items := append([]ItemInstance(nil), before.Items...)
	sort.Slice(items, func(i, j int) bool {
		if items[i].ContainerID != items[j].ContainerID {
			return items[i].ContainerID < items[j].ContainerID
		}
		if items[i].SlotIndex != items[j].SlotIndex {
			return items[i].SlotIndex < items[j].SlotIndex
		}
		return items[i].ItemInstanceID < items[j].ItemInstanceID
	})
	containers := append([]ContainerSnapshot(nil), before.Containers...)
	sort.Slice(containers, func(i, j int) bool {
		return containers[i].ContainerID < containers[j].ContainerID
	})
	occupied := make(map[string]map[int32]bool, len(containers))
	capacities := make(map[string]int32, len(containers))
	for _, c := range containers {
		if !validID(c.ContainerID) || c.Capacity < 1 || c.Capacity > MaxSnapshotItems {
			return GrantPlan{}, invalidRequest()
		}
		if _, duplicate := occupied[c.ContainerID]; duplicate {
			return GrantPlan{}, invalidRequest()
		}
		occupied[c.ContainerID] = make(map[int32]bool)
		capacities[c.ContainerID] = c.Capacity
	}
	knownIDs := make(map[string]bool, len(items))
	for _, item := range items {
		slots, exists := occupied[item.ContainerID]
		if !exists || item.SlotIndex < 0 || item.SlotIndex >= capacities[item.ContainerID] ||
			item.Quantity < 1 || item.MaxStackSize < item.Quantity ||
			knownIDs[item.ItemInstanceID] || slots[item.SlotIndex] {
			return GrantPlan{}, invalidRequest()
		}
		slots[item.SlotIndex] = true
		knownIDs[item.ItemInstanceID] = true
	}

	remaining := int64(cmd.Quantity)
	plan := GrantPlan{}
	for _, item := range items {
		if remaining == 0 {
			break
		}
		if item.ItemDefinitionID != cmd.ItemDefinitionID ||
			item.InstanceState != "active" ||
			item.MaxStackSize != rule.MaxStackSize ||
			item.Quantity >= item.MaxStackSize {
			continue
		}
		if item.Revision == math.MaxInt64 {
			return GrantPlan{}, revisionConflict()
		}
		canAdd := int64(item.MaxStackSize - item.Quantity)
		if canAdd > remaining {
			canAdd = remaining
		}
		item.Quantity += int32(canAdd)
		item.Revision++
		plan.Updates = append(plan.Updates, item)
		remaining -= canAdd
	}

	// 直到确保全部数量都有可用位置才返回计划；内存仓储与PG均不会半发奖励。
	for _, container := range containers {
		for slot := int32(0); slot < container.Capacity && remaining > 0; slot++ {
			if occupied[container.ContainerID][slot] {
				continue
			}
			if len(items)+len(plan.Creates) >= MaxSnapshotItems {
				return GrantPlan{}, inventoryFull()
			}
			quantity := int64(rule.MaxStackSize)
			if quantity > remaining {
				quantity = remaining
			}
			id := deterministicGrantID(playerID, cmd.OperationID, int32(len(plan.Creates)))
			if knownIDs[id] {
				return GrantPlan{}, idempotencyConflict()
			}
			knownIDs[id] = true
			plan.Creates = append(plan.Creates, ItemInstance{
				ItemInstanceID: id,
				ItemDefinitionID: cmd.ItemDefinitionID,
				Quantity: int32(quantity),
				ContainerID: container.ContainerID,
				SlotIndex: slot,
				Revision: 1,
				InstanceState: "active",
				MaxStackSize: rule.MaxStackSize,
			})
			remaining -= quantity
		}
	}
	if remaining > 0 {
		return GrantPlan{}, inventoryFull()
	}
	return plan, nil
}

func deterministicGrantID(playerID, operationID string, ordinal int32) string {
	// 同OperationId+同玩家+固定序号形成稳定实例ID；不同玩家同操作号不碰撞。
	sum := sha256.Sum256([]byte(playerID + "\x00" + operationID + "\x00" + intToString(ordinal)))
	return "item-g-" + hex.EncodeToString(sum[:16])
}

func intToString(value int32) string {
	if value == 0 {
		return "0"
	}
	// GrantPlan中新实例序号最大不超过10000；无需浮点格式化。
	var digits [12]byte
	index := len(digits)
	for value > 0 {
		index--
		digits[index] = byte('0' + value%10)
		value /= 10
	}
	return string(digits[index:])
}

// Grant（内存仓储授予）与PostgreSQL共用纯计划，验证完成前没有物品写副作用。
func (r *MemoryRepository) Grant(ctx context.Context, playerID string, cmd GrantCommand, rule GrantRule) (MutationResult, error) {
	return r.mutate(ctx, playerID, cmd.OperationID,
		signature("grant", struct {
			Command GrantCommand
			Rule    GrantRule
		}{cmd, rule}), cmd.ExpectedRevision,
		func(state *memoryState) (int32, int32, error) {
			plan, err := PlanTrustedGrant(playerID, snapshotOf(state), cmd, rule)
			if err != nil {
				return 0, cmd.Quantity, err
			}
			for _, update := range plan.Updates {
				state.items[update.ItemInstanceID] = update
			}
			for _, created := range plan.Creates {
				state.items[created.ItemInstanceID] = created
			}
			return cmd.Quantity, 0, nil
		})
}
