# ContextAndCorrelation（上下文与关联）

Context 分为稳定 Build/Process、Session、World、Request/Transaction（构建/进程、会话、世界、请求/事务）信息。

字段包含 BuildVersion、ContentRevision、Platform、Environment、SourceRole、ServerRole、Region、MapId、WorldId、ExperienceId、ServerInstanceId、MatchId、ArenaModeId、SessionId、PseudonymousPlayerId、CorrelationId、TransactionId。

客户端 PlayerId 不直接进入遥测。Gateway 使用 HMAC-SHA256 和后端密钥生成 PseudonymousPlayerId（伪匿名玩家编号）。服务端接入时 ServerInstanceId/ServerRole/Region/BuildVersion 由受保护请求身份覆盖 UE 上传值。

Metric（指标）禁止使用 PlayerId/SessionId/CharacterId/MatchId/ServerInstanceId/OrderId/TransactionId/RequestId/CorrelationId 作为标签。