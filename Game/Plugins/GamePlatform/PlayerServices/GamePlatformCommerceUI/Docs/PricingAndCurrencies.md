# PricingAndCurrencies（定价与币种）

权威金额全部使用整数最小货币单位 `int64 UnitAmountMinor（64位最小单位金额）`，禁止 float/double 参与扣款计算。例如 USD 1.99 存储为 199。

PriceSnapshot（价格快照）保存 PriceId、PriceType、CurrencyCode/CurrencyId、UnitAmountMinor、TotalAmountMinor、PriceVersion。Quantity（数量）乘法执行 int64 溢出保护。

第一版 Development Catalog 只有 RealMoney（真实货币）USD 测试价。SoftCurrency（软货币）只保留 Economy Port，未实现真实购买；Free（免费）类型保留在模型，但因无批准业务规则，Catalog Validation（目录校验）第一版拒绝发布。

客户端 DisplayText（显示文本）不是扣款权威；最终金额以后端 PriceSnapshot 与 Provider Verification（支付提供器验证）为准。