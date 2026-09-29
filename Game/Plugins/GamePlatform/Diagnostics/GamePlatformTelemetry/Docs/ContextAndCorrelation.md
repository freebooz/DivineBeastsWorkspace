# ContextAndCorrelation（上下文与关联）

Context 分为稳定 Build/Process、Session、World、Request/Transaction（构建/进程、会话、世界、请求/事务）信息。

字段包含 BuildVersion、ContentRevision、Platform、Environment、SourceRole、ServerRole、Region、MapId、WorldId、ExperienceId、ServerInstanceId、MatchId、ArenaModeId、SessionId、PseudonymousPlayerId、CorrelationId、TransactionId。

运行时 Buffer 不再为每条 Event/Metric 保存一份完整 Context：连续相同 Context 共享不可变快照，Batch 只序列化一次 `SourceContext（来源上下文）`；Context 变化会强制自然切批。该优化只改变 UE 内存/传输表示，不改变上下文字段的语义边界。

客户端 PlayerId 不直接进入遥测。Gateway 使用 HMAC-SHA256 和后端密钥生成 PseudonymousPlayerId（伪匿名玩家编号）。服务端接入时 ServerInstanceId/ServerRole/Region/BuildVersion 由受保护请求身份覆盖 UE 上传值。

Metric（指标）禁止使用 PlayerId/SessionId/CharacterId/MatchId/ServerInstanceId/OrderId/TransactionId/RequestId/CorrelationId 作为标签。