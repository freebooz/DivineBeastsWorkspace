# Architecture（架构）

Commerce（商城）采用四层边界：`GamePlatformCommerceUIClient（商城客户端模块）`负责显示和请求；GatewayService（网关服务）负责认证代理；PlayerData Commerce Application（玩家数据商城应用层）负责 LiveOps（运营）校验、Payment Provider（支付提供器）调用和 Inventory/Entitlement（背包/权益）履约编排；`Backend/gameplatform/commerce（商城公共领域）`负责 Product/Offer/Price/PurchaseIntent/Order/Payment/Fulfillment/Refund（商品/报价/价格/购买意图/订单/支付/履约/退款）规则。

PostgreSQL Adapter（数据库适配器）位于 `Backend/gameplatform/internal/persistence/commercerepository`。Economy（经济系统）当前只有 `Backend/gameplatform/economy`端口，没有正式 Wallet/Ledger（钱包/账本）实现。

不创建 `cmd/commerceservice（商城独立服务）`。Server Target（服务器构建目标）不得链接 `GamePlatformCommerceUIClient`。