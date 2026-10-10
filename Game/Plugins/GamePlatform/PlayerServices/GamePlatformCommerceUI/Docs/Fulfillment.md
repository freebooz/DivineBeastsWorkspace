# Fulfillment（订单履约）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

PaymentConfirmed（支付确认）后进入 FulfillmentPending（履约等待）。每个 RewardEntry（奖励条目）建立持久 FulfillmentStep，OperationId=SHA-256(OrderId|RewardEntryId)。

第一版支持：
- Inventory Grant（背包发放），SourceType=CommerceOrder，SourceId=OrderId。
- Entitlement Grant（权益发放），SourceType=CommerceOrder，SourceId=OrderId。

Progression XP（成长经验）商品明确 Unsupported。步骤成功后持久化 completed；失败时订单进入 FulfillmentFailed，重试先 ResumeFulfillment，再只执行未完成步骤。全部步骤完成后才 Finalize 为 Fulfilled 并写 Outbox。

该设计避免“奖励已发但订单仍重复发放”的问题。
