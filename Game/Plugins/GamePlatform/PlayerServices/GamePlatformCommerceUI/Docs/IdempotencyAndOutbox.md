# IdempotencyAndOutbox（幂等与事务外发）

> 2026-09-30审查更正：以下为截至2026-09-29的历史设计/计划材料，其中实现路径、完成宣称与工具环境描述已被当前README和TestingAndEvidence替代。旧Backend/gameplatform路径、迁移、Outbox、DBAServer持久化/奖励及假支付描述不构成当前交付事实；设计约束可供后续立项，必须重核实际源码。

## 历史设计材料（被现行能力矩阵替代）

PurchaseIntent（购买意图）以 `(game_id, player_id, request_id)`唯一；Order（订单）以 PurchaseIntentId（购买意图编号）唯一；Provider Transaction、Receipt Fingerprint、Provider Event、Fulfillment Operation、Refund Operation（支付交易、凭据指纹、支付事件、履约操作、退款操作）均有数据库唯一约束。

FulfillmentOperationId（履约操作编号）由 OrderId + RewardEntryId（订单编号+奖励条目编号）稳定哈希生成，同一奖励步骤重试调用原领域幂等 Grant（发放），不会重复发放。

Outbox 当前按真实阶段发送：`CommerceOrderCreated（商城订单创建）`、`CommercePaymentConfirmed（商城支付确认）`、`CommerceOrderFulfilled（商城订单履约完成）`。

Outbox Payload（事务外发载荷）只含订单事实，不存 Receipt 原文、Provider Secret 或 ClientToken（支付凭据原文、提供器密钥、客户端令牌）。
