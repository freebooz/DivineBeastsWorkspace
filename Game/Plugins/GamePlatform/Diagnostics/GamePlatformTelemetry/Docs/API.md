# API（接口）

UE 主要 API：RecordEvent（记录事件）、RecordMetric（记录指标）、IncrementCounter（计数器）、RecordGauge（记录仪表值）、RecordHistogram（记录直方图）、RecordDuration（记录时长）、FlushBestEffort（尽力刷新）、BeginSession/EndSession（开始/结束会话）、UpdateWorldContext（更新世界上下文）。

外部接入：`POST /telemetry/v1/batches`，玩家身份由 Gateway PlayerAuthenticator（网关玩家认证器）推导。

服务器接入：`POST /internal/telemetry/v1/batches`，使用受保护 Bearer Token（持有者令牌）以及 X-Game-Server-Id/X-Server-Role 等服务端身份头。

接入响应仅返回 BatchId、accepted_count、rejected_count、retryable、server_time_utc，不返回业务数据。