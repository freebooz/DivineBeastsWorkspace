# ConfigurationAndRun（配置与运行）

PlayerData Commerce（玩家数据商城）复用现有 DATABASE_URL、PLAYERDATA_INTERNAL_TOKEN、GATEWAY_CALLER_ID（数据库地址、玩家数据内部令牌、网关调用方编号）等配置，并新增：`COMMERCE_CATALOG_FILE（商城目录文件）`、`COMMERCE_INTENT_TTL_SECONDS（购买意图有效秒数，60～3600，默认600）`、`COMMERCE_PAYMENT_PROVIDER（支付提供器）`、`COMMERCE_FAKE_PAYMENT_OUTCOME（开发假支付结果）`、`APP_ENVIRONMENT（应用环境）`。

Development（开发）使用 `divinebeasts.commerce.development.json（神兽联盟开发商城目录）`与 `development_fake（开发假支付提供器）`。Production（生产）样例明确 Provider=none（无支付提供器），因此真实货币购买会安全返回 ProviderUnavailable（支付提供器不可用），而不是伪造支付成功。

当前 Commerce Catalog（商城目录）为版本化文件真源；订单/支付/履约状态为 PostgreSQL（关系数据库）真源。