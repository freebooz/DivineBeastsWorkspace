package inventory

// 本文件属于PlayerData长期背包领域的服务端授权合同。
// 一次奖励必须同时拥有已验证的玩家-服务器会话、不可由玩家自报的奖励事件记录，
// 以及后端权威物品规则目录；不能把请求字段文本或客户端DataAsset当授予资格。

import (
	"context"
	"errors"
	"fmt"
	"strings"
)

// GrantEvidence（后端可信奖励证据）由服务器侧的掉落/任务结算系统持久保存。
// SourceEventID是事件唯一号；事件重试永远复用相同值，跨账号由GrantOperationID隔离。
type GrantEvidence struct {
	SourceEventID    string
	GameServerID     string
	ServerBootID     string
	PlayerID         string
	ReasonCode       string
	ItemDefinitionID string
	Quantity         int32
}

// ServerPlayerVerifier（游戏服玩家归属验证端口）必须检查当前正式GameServer的Boot，
// 玩家经服务端准入后仍处于该世界/实例，且可信调用凭据有效。
// 生产接口未装配或网络故障必须返回错误，不能降级仅比较传入的GameServerID。
type ServerPlayerVerifier interface {
	VerifyServerPlayer(context.Context, string, string, string) error
}

// GrantEvidenceSource（可信事件资料端口）读取已确认的掉落、任务或剧情奖励。
// 来源必须是服务器内部已审核事件库，不能直接读取HTTP请求正文或普通客户端上传。
type GrantEvidenceSource interface {
	FindGrantEvidence(context.Context, string) (GrantEvidence, error)
}

// ItemPolicy（后端权威物品规则）只用于授予、消耗和堆叠判断；客户端展示内容不参与授权。
type ItemPolicy struct {
	ItemDefinitionID    string   `json:"itemDefinitionId"`
	MaxStackSize        int32    `json:"maxStackSize"`
	CanConsume          bool     `json:"canConsume"`
	AllowedGrantReasons []string `json:"allowedGrantReasons"`
}

// ItemPolicyCatalog（后端物品权威策略目录）不依赖任何项目美术、贴图或UE客户端类型。
type ItemPolicyCatalog interface {
	FindItemPolicy(context.Context, string) (ItemPolicy, error)
}

// VerifiedGrantAuthorizer（正式可信授予组合器）不持有存档/账本；三个可信来源必须全部认可。
type VerifiedGrantAuthorizer struct {
	Servers  ServerPlayerVerifier
	Evidence GrantEvidenceSource
	Catalog  ItemPolicyCatalog
}

// NewVerifiedGrantAuthorizer（构建带明确外部凭据依赖的授权器）。
// nil依赖会导致装配失败；生产不能回退使用“测试全放行”的内存授权器。
func NewVerifiedGrantAuthorizer(servers ServerPlayerVerifier, evidence GrantEvidenceSource, catalog ItemPolicyCatalog) (*VerifiedGrantAuthorizer, error) {
	if servers == nil || evidence == nil || catalog == nil {
		return nil, errors.New("可信授予需要服务器-玩家验证、奖励事件来源和后端物品规则目录")
	}
	return &VerifiedGrantAuthorizer{Servers: servers, Evidence: evidence, Catalog: catalog}, nil
}

// AuthorizeInventoryGrant（核验可信拾取/奖励）：先核对当前服务器Boot和玩家绑定，
// 再按已确认事件逐字段比对请求，最后从后端策略目录确定冻结到物品实例的堆叠上限。
func (a *VerifiedGrantAuthorizer) AuthorizeInventoryGrant(
	ctx context.Context, playerID string, cmd GrantCommand,
) (GrantRule, error) {
	if err := ctx.Err(); err != nil {
		return GrantRule{}, err
	}
	if a == nil || a.Servers == nil || a.Evidence == nil || a.Catalog == nil {
		return GrantRule{}, grantNotAllowed()
	}
	if err := a.Servers.VerifyServerPlayer(ctx, cmd.GameServerID, cmd.ServerBootID, playerID); err != nil {
		return GrantRule{}, grantNotAllowed()
	}
	evidence, err := a.Evidence.FindGrantEvidence(ctx, cmd.SourceEventID)
	if err != nil || evidence.SourceEventID != cmd.SourceEventID ||
		evidence.PlayerID != playerID ||
		evidence.GameServerID != cmd.GameServerID ||
		evidence.ServerBootID != cmd.ServerBootID ||
		evidence.ReasonCode != cmd.ReasonCode ||
		evidence.ItemDefinitionID != cmd.ItemDefinitionID ||
		evidence.Quantity != cmd.Quantity {
		return GrantRule{}, grantNotAllowed()
	}
	policy, err := a.Catalog.FindItemPolicy(ctx, cmd.ItemDefinitionID)
	if err != nil || policy.ItemDefinitionID != cmd.ItemDefinitionID ||
		policy.MaxStackSize < 1 || policy.MaxStackSize > MaxGrantStackSize {
		return GrantRule{}, grantNotAllowed()
	}
	allowReason := false
	for _, reason := range policy.AllowedGrantReasons {
		if reason == cmd.ReasonCode {
			allowReason = true
			break
		}
	}
	if !allowReason {
		return GrantRule{}, grantNotAllowed()
	}
	return GrantRule{MaxStackSize: policy.MaxStackSize}, nil
}

// StaticItemPolicyCatalog（不可变物品目录）适用于版本锁定后端部署。
// 正式发布时由经过审核的项目配置导入，不能从客户端上传实时覆盖。
type StaticItemPolicyCatalog struct {
	items map[string]ItemPolicy
}

// NewStaticItemPolicyCatalog（初始化不可变规则快照）；不接收重复定义或不合法的来源类别。
func NewStaticItemPolicyCatalog(policies []ItemPolicy) (*StaticItemPolicyCatalog, error) {
	result := &StaticItemPolicyCatalog{items: make(map[string]ItemPolicy, len(policies))}
	for _, entry := range policies {
		if !validID(entry.ItemDefinitionID) || strings.TrimSpace(entry.ItemDefinitionID) != entry.ItemDefinitionID ||
			entry.MaxStackSize < 1 || entry.MaxStackSize > MaxGrantStackSize ||
			len(entry.AllowedGrantReasons) == 0 {
			return nil, fmt.Errorf("物品规则目录存在非法定义")
		}
		seen := make(map[string]bool)
		for _, reason := range entry.AllowedGrantReasons {
			switch reason {
			case "pickup", "quest_reward", "scripted_reward":
			default:
				return nil, fmt.Errorf("物品规则目录存在未审批的授予事由")
			}
			if seen[reason] {
				return nil, errors.New("物品规则目录授予事由重复")
			}
			seen[reason] = true
		}
		if _, exists := result.items[entry.ItemDefinitionID]; exists {
			return nil, errors.New("物品规则目录ItemDefinitionId重复")
		}
		entry.AllowedGrantReasons = append([]string(nil), entry.AllowedGrantReasons...)
		result.items[entry.ItemDefinitionID] = entry
	}
	if len(result.items) == 0 {
		return nil, errors.New("物品规则目录不能为空")
	}
	return result, nil
}

func (c *StaticItemPolicyCatalog) FindItemPolicy(ctx context.Context, definitionID string) (ItemPolicy, error) {
	if err := ctx.Err(); err != nil {
		return ItemPolicy{}, err
	}
	if c == nil {
		return ItemPolicy{}, grantNotAllowed()
	}
	policy, ok := c.items[definitionID]
	if !ok {
		return ItemPolicy{}, grantNotAllowed()
	}
	policy.AllowedGrantReasons = append([]string(nil), policy.AllowedGrantReasons...)
	return policy, nil
}
