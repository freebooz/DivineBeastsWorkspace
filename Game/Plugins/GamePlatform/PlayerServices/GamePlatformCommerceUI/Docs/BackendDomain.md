# BackendDomain（后端领域）

Commerce 公共领域位于 `Backend/gameplatform/commerce`，包含 Product、Offer、Price、PurchaseIntent、Order、Payment、Fulfillment、Refund、Provider Port、Repository Port、Errors、Catalog 与 tests（商品、报价、价格、购买意图、订单、支付、履约、退款、提供器端口、仓储端口、错误、目录与测试）。

领域层不依赖具体 PostgreSQL、HTTP、支付 SDK、Inventory Repository 或 Entitlement Repository（数据库、网络、支付SDK、背包仓储、权益仓储）。

具体 PostgreSQL 实现在 `Backend/gameplatform/internal/persistence/commercerepository（商城数据库仓储）`；Development Fake Provider（开发假支付提供器）位于 `Backend/gameplatform/commerceprovider`；跨领域编排位于 `Backend/gameplatform/playerdata/commerce_application.go（商城应用编排）`。

Economy（经济系统）仅在 `Backend/gameplatform/economy/port.go`定义最小端口，没有声称 Wallet/Ledger（钱包/账本）已完成。