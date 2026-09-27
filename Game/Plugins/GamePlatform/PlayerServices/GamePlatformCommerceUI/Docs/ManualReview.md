# ManualReview（人工审查）

状态：未执行。AI 未代签。

人工审查至少覆盖：ClientOnly（仅客户端）模块与 Server（服务器）隔离、四个商城页面模板真实 `.uasset（UE二进制资产）`、Catalog/Product/Offer/Price（目录/商品/报价/价格）、整数金额、Intent TTL（购买意图有效期）、订单状态机、Request/Order 幂等（请求/订单幂等）、Expired/Future/LiveOps Offer（过期/未来/运营报价）、Purchase Limit（购买限制）、Development Fake success/failure/timeout（开发假支付成功/失败/超时）、Shipping 禁用 Fake（发布版禁用假支付）、Receipt Replay、Provider Callback Replay（支付凭据/回调重放）、Inventory/Entitlement 履约（背包/权益履约）、部分失败恢复、服务重启、跨账号订单隔离、退款边界、Secret 泄漏检查、Client Build/Cook（客户端构建/烘焙）、Server Build 影响（服务器构建影响）。

Production Payment Provider（生产支付提供器）、真实 Provider Receipt/Webhook（真实支付凭据/回调）、沙箱交易、自动退款、SoftCurrency Wallet/Ledger（软货币钱包/账本）、Multi-PIE（多编辑器玩家）和真实 UI 资产验收当前均未执行。