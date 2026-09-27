# ClientUIAndViewModel（客户端界面与视图模型）

商城客户端复用现有 `GamePlatformUIClient（平台UI客户端模块）`：页面基类继承 `UGamePlatformUIScreen（平台页面）`，视图模型继承 `UGamePlatformViewModelBase（平台视图模型）`。

C++ 提供四个 Blueprintable（可蓝图继承）模板基类：`UGamePlatformCommerceCatalogScreen（商城目录页）`、`UGamePlatformCommerceProductDetailScreen（商品详情页）`、`UGamePlatformCommercePurchaseConfirmScreen（购买确认页）`、`UGamePlatformCommerceOrderStatusScreen（订单状态页）`。

它们对应计划中的 `WBP_CommerceCatalog / WBP_CommerceProductDetail / WBP_CommercePurchaseConfirm / WBP_CommerceOrderStatus（商城Widget Blueprint界面蓝图）`。当前真实 `.uasset（UE二进制资产）`未创建，因为工程规则禁止文本伪造。

状态机包括 Idle、LoadingCatalog、Browsing、CreatingIntent、Confirming、AwaitingProvider、AwaitingVerification、AwaitingFulfillment、Succeeded、Failed、Reconciling。Widget（界面组件）不解析 HTTP JSON。