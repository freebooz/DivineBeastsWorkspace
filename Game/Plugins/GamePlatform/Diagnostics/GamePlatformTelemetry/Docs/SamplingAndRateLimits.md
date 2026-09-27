# SamplingAndRateLimits（采样与限流）

Sampling Policy（采样策略）支持 Always、DeterministicSessionSample、Probabilistic、Disabled（总是、确定性会话采样、概率采样、禁用）。

DeterministicSessionSample 使用 SamplingSeed + StableSessionSamplingKey + DefinitionName 的稳定哈希，使同一 Session 在同一规则下稳定入样/不入样。

UE Event Rate Limit 使用每 EventName Token Bucket（事件名令牌桶）并支持 sustained rate + burst（持续速率+突发）。

Gateway 和 Server Ingest 另有按认证主体的有界请求限流。限流只丢/拒绝遥测，不改变游戏状态。