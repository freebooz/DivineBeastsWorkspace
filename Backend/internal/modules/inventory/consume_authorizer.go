package inventory

// 可信使用/消耗事件的授权层：背包只扣权威数量并持久化集成事件，
// 角色治疗、任务或Buff由真实游戏服逻辑处理；不能把UI按钮点击直接变成消耗。

import (
	"context"
	"crypto/sha256"
	"encoding/hex"
)

// ConsumeEvidence（已确认的服务端使用事件）由游戏服权威流程产生，
// 消耗命令与事件每个字段必须相同，确保幂等重试不会误扣不同物品。
type ConsumeEvidence struct {
	SourceEventID    string
	GameServerID     string
	ServerBootID     string
	PlayerID         string
	ReasonCode       string
	ItemDefinitionID string
	ItemInstanceID   string
	Quantity         int32
}

// ConsumeEvidenceSource（服务端使用事件端口）必须能读取已经权威确认的事件。
// 事件来源不得是公网友好客户端HTTP字段或无鉴权的消息队列。
type ConsumeEvidenceSource interface {
	FindConsumeEvidence(context.Context, string) (ConsumeEvidence, error)
}

// ConsumeOperationID（确定性消耗操作编号）由玩家和已确认事件唯一身份推导。
func ConsumeOperationID(playerID, sourceEventID string) string {
	digest := sha256.Sum256([]byte(playerID + "\x00" + sourceEventID))
	return "consume-" + hex.EncodeToString(digest[:16])
}

// VerifiedConsumeAuthorizer（真实使用授权组合器）同时验证玩家仍在服务器中、
// 事件已经确认及物品定义允许消耗。没有三项有效来源时默认失败关闭。
type VerifiedConsumeAuthorizer struct {
	Servers  ServerPlayerVerifier
	Evidence ConsumeEvidenceSource
	Catalog  ItemPolicyCatalog
}

func (a *VerifiedConsumeAuthorizer) AuthorizeInventoryConsume(
	ctx context.Context, playerID string, command ConsumeCommand,
) error {
	if err := ctx.Err(); err != nil {
		return err
	}
	if a == nil || a.Servers == nil || a.Evidence == nil || a.Catalog == nil ||
		!validID(command.SourceEventID) ||
		!validID(command.GameServerID) ||
		!validID(command.ServerBootID) ||
		!validID(command.ItemDefinitionID) ||
		command.ReasonCode != "item_use" ||
		command.OperationID != ConsumeOperationID(playerID, command.SourceEventID) {
		return consumeNotAllowed()
	}
	if err := a.Servers.VerifyServerPlayer(ctx, command.GameServerID, command.ServerBootID, playerID); err != nil {
		return consumeNotAllowed()
	}
	evidence, err := a.Evidence.FindConsumeEvidence(ctx, command.SourceEventID)
	if err != nil ||
		evidence.SourceEventID != command.SourceEventID ||
		evidence.GameServerID != command.GameServerID ||
		evidence.ServerBootID != command.ServerBootID ||
		evidence.PlayerID != playerID ||
		evidence.ItemDefinitionID != command.ItemDefinitionID ||
		evidence.ItemInstanceID != command.ItemInstanceID ||
		evidence.Quantity != command.Quantity ||
		evidence.ReasonCode != command.ReasonCode {
		return consumeNotAllowed()
	}
	policy, err := a.Catalog.FindItemPolicy(ctx, command.ItemDefinitionID)
	if err != nil || policy.ItemDefinitionID != command.ItemDefinitionID || !policy.CanConsume {
		return consumeNotAllowed()
	}
	return nil
}
