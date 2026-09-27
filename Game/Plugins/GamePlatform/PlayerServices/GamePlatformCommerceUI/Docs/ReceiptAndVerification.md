# ReceiptAndVerification（支付凭据与验证）

Receipt/Transaction Token（支付凭据/交易令牌）从客户端提交后，只作为后端 Provider Verify（支付提供器验证）的输入。

后端验证结果必须至少绑定 ProviderName、ProviderTransactionId、ReceiptFingerprint、Order、AmountMinor、CurrencyCode、VerifiedAt。金额和币种必须与 Order PriceSnapshot 一致。

PostgreSQL 对 ProviderTransactionId、ReceiptFingerprint、ProviderEventId（支付交易编号、凭据指纹、支付事件编号）建立唯一约束，防止重放。可选 ProviderEventId 的 NULL 约束通过 `0008_commerce_optional_provider_ids（商城可选支付编号修正迁移）`修正为普通 UNIQUE，允许多个 NULL。

真实生产 Provider 签名、Webhook（回调）与沙箱交易因为 Provider 未指定，状态为未执行。