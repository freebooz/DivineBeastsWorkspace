# PrivacyAndSensitiveData（隐私与敏感数据）

Privacy Class（隐私等级）为 Operational、Pseudonymous、Sensitive、Forbidden（运营、伪匿名、敏感、禁止）。

第一版采用 Schema Allowlist（结构白名单）优先，并对 key 做大小写/下划线/短横线/点号归一化后的 Forbidden 检查。

禁止关键字至少覆盖 password、access_token、refresh_token、authorization、cookie、payment_receipt/raw_receipt、card_number、payment_token、provider_secret、private_key、billing_address。

Commerce Telemetry 可记录 ProductId、OfferId、Result、Duration、ProviderType、ErrorCode；禁止 raw receipt、支付 token、卡信息、Provider Secret 和账单地址。

后端不信任 UE 已脱敏，会再次过滤。