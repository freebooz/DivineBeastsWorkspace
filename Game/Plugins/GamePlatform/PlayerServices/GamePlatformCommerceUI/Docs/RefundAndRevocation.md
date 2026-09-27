# RefundAndRevocation（退款与撤销）

Commerce Domain 已定义 Refund 模型与 RefundProvider（退款提供器）边界，但当前没有 Production Payment Provider，因此自动实付退款流程未实现，PlayerData RequestRefund 返回 RefundUnsupported。

未来退款确认后，Entitlement（权益）可以按 SourceType=CommerceOrder + SourceId=OrderId 撤销该商城来源 Grant，不影响 Quest/LiveOps 等其它来源。

Inventory（背包）消耗品若已被使用，不能简单删除任意玩家 ItemInstance（物品实例）；必须在业务规则明确后再实现人工或补偿策略。

Chargeback（拒付）本轮不自动封禁玩家，只保留未来审计边界。