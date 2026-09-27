# Fulfillment（订单履约）

PaymentConfirmed（支付确认）后进入 FulfillmentPending（履约等待）。每个 RewardEntry（奖励条目）建立持久 FulfillmentStep，OperationId=SHA-256(OrderId|RewardEntryId)。

第一版支持：
- Inventory Grant（背包发放），SourceType=CommerceOrder，SourceId=OrderId。
- Entitlement Grant（权益发放），SourceType=CommerceOrder，SourceId=OrderId。

Progression XP（成长经验）商品明确 Unsupported。步骤成功后持久化 completed；失败时订单进入 FulfillmentFailed，重试先 ResumeFulfillment，再只执行未完成步骤。全部步骤完成后才 Finalize 为 Fulfilled 并写 Outbox。

该设计避免“奖励已发但订单仍重复发放”的问题。