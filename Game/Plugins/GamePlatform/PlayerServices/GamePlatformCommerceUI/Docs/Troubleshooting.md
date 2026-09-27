# Troubleshooting（故障排查）

CommerceCatalogUnavailable（商城目录不可用）：检查 `COMMERCE_CATALOG_FILE（商城目录文件）`路径与 JSON 校验。

OfferNotActive/OfferNotEligible（报价未生效/无资格）：检查 UTC 时间窗口、LiveOps Event 与 Audience Rule（运营活动/人群规则）；客户端本地显示不是最终资格。

PurchaseIntentExpired（购买意图过期）：重新创建 Intent，不复用旧价格。

ProviderUnavailable（支付提供器不可用）：生产样例当前是预期行为，因为尚无已批准 Production Provider（生产支付提供器）。Development（开发）必须确认 `APP_ENVIRONMENT=development`且`COMMERCE_PAYMENT_PROVIDER=development_fake`。

ReceiptReplay（支付凭据重放）：不要生成新订单绕过；查询原订单并 Reconcile（对账）。

FulfillmentFailed（履约失败）：使用同一 Order 对账，已完成 RewardStep（奖励步骤）不会重复发。SoftCurrencyUnavailable（软货币不可用）表示 Economy Wallet/Ledger（经济钱包/账本）尚未正式实现。