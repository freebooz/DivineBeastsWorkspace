# IdempotencyAndOutbox（幂等与事务外发）

PurchaseIntent（购买意图）以 `(game_id, player_id, request_id)`唯一；Order（订单）以 PurchaseIntentId（购买意图编号）唯一；Provider Transaction、Receipt Fingerprint、Provider Event、Fulfillment Operation、Refund Operation（支付交易、凭据指纹、支付事件、履约操作、退款操作）均有数据库唯一约束。

FulfillmentOperationId（履约操作编号）由 OrderId + RewardEntryId（订单编号+奖励条目编号）稳定哈希生成，同一奖励步骤重试调用原领域幂等 Grant（发放），不会重复发放。

Outbox 当前按真实阶段发送：`CommerceOrderCreated（商城订单创建）`、`CommercePaymentConfirmed（商城支付确认）`、`CommerceOrderFulfilled（商城订单履约完成）`。

Outbox Payload（事务外发载荷）只含订单事实，不存 Receipt 原文、Provider Secret 或 ClientToken（支付凭据原文、提供器密钥、客户端令牌）。