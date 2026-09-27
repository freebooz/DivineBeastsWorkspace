# GamePlatformCommerceUI（游戏平台商城界面插件）

正式路径：`Game/Plugins/GamePlatform/PlayerServices/GamePlatformCommerceUI`。

UE（虚幻引擎）只保留一个 `GamePlatformCommerceUIClient（商城客户端模块）`，类型为 `ClientOnly（仅客户端）`。它负责 Catalog（商城目录）缓存、Product/Offer/Price（商品/报价/价格）显示、PurchaseIntent（购买意图）、Order（订单）查询、Receipt（支付凭据）提交、Reconcile（对账）和通用 UI/ViewModel（界面/视图模型）模板；客户端不能定义权威价格、奖励或 Payment Success（支付成功）。

Go（Go语言）权威领域位于 `Backend/gameplatform/commerce`，由现有 PlayerDataService（玩家数据服务）与 GatewayService（网关服务）承载，不创建独立 Commerce 微服务。订单、支付、履约和退款边界持久化到 PostgreSQL（关系数据库）；商品目录第一版使用版本化 JSON 文件作为启动时真源。

当前仓库没有已批准的 Production Payment Provider（生产支付提供器）。只实现 `DevelopmentFakePaymentProvider（开发假支付提供器）`用于 Development/Test（开发/测试）状态机和履约验证，并在 production/shipping（生产/发布）环境拒绝初始化。Production Payment（生产支付）状态必须保持未执行。

第一版 Commerce（商城）支持 Inventory Item（背包物品）和 Entitlement（权益）商品履约；Progression XP（成长经验）商品明确 Unsupported（不支持）。Economy（经济系统）仅建立 SoftCurrency Wallet/Ledger Port（软货币钱包/账本端口），开发目录没有软货币 Offer，因此 SoftCurrency 购买未执行。

真实 `WBP_Commerce*（商城Widget Blueprint界面蓝图）`不得用文本伪造。本轮提供 C++ 可复用页面/ViewModel 基类和 Content/Client 说明，实际 .uasset（二进制UE资产）必须由 Unreal Editor（虚幻编辑器）创建。
