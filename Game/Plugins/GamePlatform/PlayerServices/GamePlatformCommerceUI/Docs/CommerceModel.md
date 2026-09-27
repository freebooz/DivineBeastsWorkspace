# CommerceModel（商城模型）

核心对象：Product（商品）、Offer（报价）、Price（价格）、PurchaseIntent（购买意图）、Order（订单）、Payment（支付事实）、FulfillmentStep（履约步骤）、Refund（退款边界）。

Product 不保存当前售价；同一 Product 可被多个 Offer 引用。Order 创建时持久保存 CatalogRevision、OfferRevision、PriceSnapshot、RewardSnapshot（目录修订号、报价修订号、价格快照、奖励快照），后续 Catalog 变化不能修改历史订单。

ProductType 第一版只允许 InventoryItem（背包物品）和 Entitlement（权益）。ProgressionXP（成长经验）商品默认不开放。