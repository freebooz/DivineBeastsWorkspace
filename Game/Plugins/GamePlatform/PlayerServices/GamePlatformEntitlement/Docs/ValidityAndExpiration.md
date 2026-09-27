# ValidityAndExpiration（有效期与过期）

Permanent Grant（永久授予）没有 ExpiresAt。Temporary Grant（临时授予）使用可信 UTC StartsAt/ExpiresAt。

第一版采用 Query-time effectiveness（查询时计算有效性），不为每个权益创建 Go Timer（定时器）。过期 Grant 记录继续留在数据库用于审计，但不会出现在有效 Snapshot 中，Dedicated Server 授权检查也会拒绝。

客户端可用 ExpiresAt 做显示倒计时，但本地时间只用于表现，不能成为权限判断依据。