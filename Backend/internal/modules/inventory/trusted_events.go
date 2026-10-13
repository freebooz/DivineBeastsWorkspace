package inventory

// Backend可信服务器事件摄入编排：只接收内部已认证GameServer确认的拾取、
// 任务奖励和物品使用事实。事件先持久化再进入背包幂等事务。
// 消耗后玩法效果仍需游戏服的独立幂等消费者执行，不在此直接改角色属性。

import (
	"context"
	"errors"
	"divinebeasts/backend/internal/platform/apperror"
)

const (
	TrustedEventGrant = "grant" // 授予：采集、任务及剧情
	TrustedEventConsume = "consume" // 消耗：受控使用
)

// ConfirmedInventoryEvent（已确认权威事件）把玩家、世界实例和会话世代与数量锁定。
// SourceEventID须由Dedicated Server权威玩法事件稳定生成，重试禁止重新生成。
type ConfirmedInventoryEvent struct {
	EventKind string `json:"eventKind"`
	GameID string `json:"gameId"`
	GameServerID string `json:"gameServerId"`
	ServerBootID string `json:"serverBootId"`
	PlayerID string `json:"playerId"`
	SessionEpoch int64 `json:"sessionEpoch"`
	ConnectionID string `json:"connectionId"`
	SourceEventID string `json:"sourceEventId"`
	ExpectedRevision int64 `json:"expectedRevision"`
	ItemDefinitionID string `json:"itemDefinitionId"`
	ItemInstanceID string `json:"itemInstanceId,omitempty"`
	Quantity int32 `json:"quantity"`
	ReasonCode string `json:"reasonCode"`
}

// Lease（玩家当前权威租约）不是客户端自行声明的登录凭据。
func (e ConfirmedInventoryEvent) Lease() TrustedPlayerLease {
	return TrustedPlayerLease{
		GameID:e.GameID, GameServerID:e.GameServerID, ServerBootID:e.ServerBootID,
		PlayerID:e.PlayerID, SessionEpoch:e.SessionEpoch, ConnectionID:e.ConnectionID,
	}
}

// Validate（结构验证）不是来源验证；来源必须由实际玩家租约与可信日志证明。
func (e ConfirmedInventoryEvent) Validate() error {
	if !validID(e.GameID) || !validID(e.GameServerID) || !validID(e.ServerBootID) ||
		!validID(e.PlayerID) || !validID(e.ConnectionID) || !validID(e.SourceEventID) ||
		!validID(e.ItemDefinitionID) || e.SessionEpoch < 1 || e.ExpectedRevision < 0 ||
		e.Quantity < 1 || e.Quantity > MaxGrantQuantity {
		return invalidRequest()
	}
	if e.EventKind == TrustedEventGrant && e.ItemInstanceID == "" {
		switch e.ReasonCode {
		case "pickup","quest_reward","scripted_reward":
			return nil
		}
	}
	if e.EventKind == TrustedEventConsume && e.ReasonCode == "item_use" &&
		validID(e.ItemInstanceID) {
		return nil
	}
	return invalidRequest()
}

// GrantCommand（授权授予命令）使用权威事件的全部不可变字段及固定操作号。
func (e ConfirmedInventoryEvent) GrantCommand() GrantCommand {
	return GrantCommand{
		OperationID: GrantOperationID(e.PlayerID,e.SourceEventID),
		ExpectedRevision:e.ExpectedRevision, GameID:e.GameID,
		GameServerID:e.GameServerID, ServerBootID:e.ServerBootID,
		SessionEpoch:e.SessionEpoch, ConnectionID:e.ConnectionID,
		SourceEventID:e.SourceEventID, ReasonCode:e.ReasonCode,
		ItemDefinitionID:e.ItemDefinitionID, Quantity:e.Quantity,
	}
}

// ConsumeCommand（授权消耗命令）必须同时绑定物品定义及真实物品实例。
func (e ConfirmedInventoryEvent) ConsumeCommand() ConsumeCommand {
	return ConsumeCommand{
		OperationID: ConsumeOperationID(e.PlayerID,e.SourceEventID),
		ExpectedRevision:e.ExpectedRevision, GameID:e.GameID,
		GameServerID:e.GameServerID, ServerBootID:e.ServerBootID,
		SessionEpoch:e.SessionEpoch, ConnectionID:e.ConnectionID,
		SourceEventID:e.SourceEventID, ReasonCode:e.ReasonCode,
		ItemDefinitionID:e.ItemDefinitionID, ItemInstanceID:e.ItemInstanceID,
		Quantity:e.Quantity,
	}
}

// CanonicalGrantCommand（可信授予请求身份）排除可变化的乐观修订号。
// 同一来源事实由玩家+事件号、连接世代、物品和数量决定，Revision只用于首次写入的并发检查。
func CanonicalGrantCommand(cmd GrantCommand) GrantCommand {
	cmd.ExpectedRevision = 0
	return cmd
}

// CanonicalConsumeCommand（可信消耗请求身份）同样排除乐观修订号，
// 但不排除服务器Boot、玩家连接世代、实际物品实例或数量。
func CanonicalConsumeCommand(cmd ConsumeCommand) ConsumeCommand {
	cmd.ExpectedRevision = 0
	return cmd
}

// ConfirmedEventJournal（已确认事件日志）在写库存之前保存事件，
// 同EventID+同内容幂等，任何字段变更必须拒绝。
type ConfirmedEventJournal interface {
	RecordConfirmedEvent(context.Context,ConfirmedInventoryEvent) error
}

// TrustedMutationService（可信背包修改端口）与普通Gateway隔离。
type TrustedMutationService interface {
	GetSnapshot(context.Context,string)(Snapshot,error)
	GrantTrusted(context.Context,string,GrantCommand)(MutationResult,error)
	ConsumeTrusted(context.Context,string,ConsumeCommand)(MutationResult,error)
}

// TrustedEventProcessor（可信事件处理器）由仅内部认证的服务组合根装配。
type TrustedEventProcessor struct {
	GameID string
	Verifier ServerPlayerVerifier
	Journal ConfirmedEventJournal
	Mutation TrustedMutationService
}

// NewTrustedEventProcessor（创建可信编排）无玩家验证、日志或事务则拒绝装配。
func NewTrustedEventProcessor(
	gameID string, verifier ServerPlayerVerifier,
	journal ConfirmedEventJournal, mutation TrustedMutationService,
)(*TrustedEventProcessor,error){
	if !validID(gameID) || verifier == nil || journal == nil || mutation == nil {
		return nil,errors.New("可信背包事件缺少身份验证、事件日志或事务服务")
	}
	return &TrustedEventProcessor{GameID:gameID,Verifier:verifier,Journal:journal,Mutation:mutation},nil
}

// Process（处理可信事件）先检查真实玩家世代，再记录事件，最后提交背包事务。
// 异常超时应复用相同SourceEventID和权威事件字段；Revision仅是服务端并发前提，
// 由PlayerData重新读取。仓储幂等指纹不含Revision，其他玩家/物品/数量字段一律不可变。
func (p *TrustedEventProcessor) Process(
	ctx context.Context,event ConfirmedInventoryEvent,
)(MutationResult,error){
	if err:=ctx.Err();err!=nil{return MutationResult{},err}
	if p==nil || p.Verifier==nil || p.Journal==nil || p.Mutation==nil ||
		event.GameID!=p.GameID {return MutationResult{},grantNotAllowed()}
	if err:=event.Validate();err!=nil{return MutationResult{},err}
	if err:=p.Verifier.VerifyServerPlayer(ctx,event.Lease());err!=nil{
		return MutationResult{},grantNotAllowed()
	}
	if err:=p.Journal.RecordConfirmedEvent(ctx,event);err!=nil{return MutationResult{},err}
	// 事件本身不可变，但乐观修订号只是本次尝试的并发前提。
	// 从PlayerData真源重新读取修订号，避免多个真实世界事件在同一玩家背包上永久冲突。
	// 由于仓储指纹排除了Revision，同一事件已提交时仍返回原幂等快照。
	for attempt:=0;attempt<3;attempt++ {
		snapshot,err:=p.Mutation.GetSnapshot(ctx,event.PlayerID)
		if err!=nil{return MutationResult{},err}
		var result MutationResult
		switch event.EventKind {
		case TrustedEventGrant:
			cmd:=event.GrantCommand();cmd.ExpectedRevision=snapshot.InventoryRevision
			result,err=p.Mutation.GrantTrusted(ctx,event.PlayerID,cmd)
		case TrustedEventConsume:
			cmd:=event.ConsumeCommand();cmd.ExpectedRevision=snapshot.InventoryRevision
			result,err=p.Mutation.ConsumeTrusted(ctx,event.PlayerID,cmd)
		default:
			return MutationResult{},invalidRequest()
		}
		if err==nil{return result,nil}
		var appErr *apperror.Error
		if !errors.As(err,&appErr) || appErr.Code!="INVENTORY_REVISION_CONFLICT" || attempt==2 {
			return MutationResult{},err
		}
	}
	return MutationResult{},revisionConflict()
}
