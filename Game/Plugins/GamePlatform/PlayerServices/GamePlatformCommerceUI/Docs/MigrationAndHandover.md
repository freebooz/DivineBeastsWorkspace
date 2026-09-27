# MigrationAndHandover（迁移与交接）

本轮新增 `0007_commerce（商城基础迁移）`与`0008_commerce_optional_provider_ids（商城可选支付编号修正迁移）`，不修改 Quest/Inventory/Entitlement/Equipment/Progression/LiveOps（任务/背包/权益/装备/成长/运营）历史 Migration。

后续接 Production Provider（生产支付提供器）时必须新增 Provider-specific Adapter（支付提供器专用适配器），完成 Receipt/Signature/Webhook（凭据/签名/回调）验证、沙箱交易和退款证据；不得把 DevelopmentFakePaymentProvider（开发假支付提供器）升级为生产实现。

若未来正式支持 SoftCurrency（软货币），必须先实现 Economy Wallet/Ledger（经济钱包/账本）与幂等 Debit/Credit（扣款/入账），再把 Commerce Economy Port（商城经济系统端口）接通；不能直接写余额。

本轮停止在 `GamePlatformCommerceUI（游戏平台商城界面插件）`，下一插件 `GamePlatformTelemetry（游戏平台遥测插件）`未进入。