# PersistenceAndTransactions（持久化与事务）

数据库迁移为 `0007_commerce（商城基础迁移）`和 `0008_commerce_optional_provider_ids（商城可选支付编号修正迁移）`。

`0007`建立 PurchaseIntent、Order、Payment、Fulfillment、Refund、Operation（购买意图、订单、支付、履约、退款、操作）表。创建订单事务使用 PostgreSQL advisory lock（咨询锁）按 game/player/offer（游戏/玩家/报价）串行化购买限制检查，并一次写入订单、RewardSnapshot（奖励快照）对应 Fulfillment Steps（履约步骤）与 CommerceOrderCreated Outbox（订单创建事务外发）。

PaymentConfirmed（支付确认）在一个事务内锁订单、写支付唯一事实、推进 Payment/Fulfillment（支付/履约）状态并写 CommercePaymentConfirmed Outbox。FinalizeFulfillment（完成履约）在确认全部步骤完成后推进 Fulfilled 并写 CommerceOrderFulfilled Outbox。

由于 `0007`文件被外部进程锁定，Optional ProviderEvent/Refund ID（可选支付事件/退款编号）的 NULL 唯一语义通过后续正式 `0008`迁移修正，而不是强改历史文件。