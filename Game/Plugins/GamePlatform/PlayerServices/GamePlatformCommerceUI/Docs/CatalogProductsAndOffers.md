# CatalogProductsAndOffers（目录商品与报价）

Commerce Catalog（商城目录）包含 CatalogRevision、GeneratedAt、Products、Offers、Prices、RewardSets。

Development Catalog（开发目录）覆盖：正常背包物品 Offer、权益 Offer、Expired Offer（已过期报价）、Future Offer（未来报价）、LiveOps-linked Offer（运营活动关联报价）和 Lifetime Purchase Limit（终身购买限制）。

Offer 包含 OfferRevision、PriceId、StartsAtUtc、EndsAtUtc、AudienceRuleSetId、LiveOpsEventId、PurchaseLimitPolicy。后端 CreatePurchaseIntent 时重新判断时间窗口、LiveOps Event/Eligibility（运营活动/资格）、价格和奖励，客户端过滤只用于显示。

Catalog 第一版真源是版本化 JSON 文件，PlayerData 启动时校验加载。