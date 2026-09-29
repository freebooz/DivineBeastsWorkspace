# API（接口）

UE 主要 API：RecordEvent（记录事件）、RecordMetric（记录指标）、IncrementCounter（计数器）、RecordGauge（记录仪表值）、RecordHistogram（记录直方图）、RecordDuration（记录时长）、FlushBestEffort（尽力刷新）、BeginSession/EndSession（开始/结束会话）、UpdateWorldContext（更新世界上下文）。

目标客户端接入合同：`POST /telemetry/v1/batches`，正式实现后玩家身份必须由 Gateway PlayerAuthenticator（网关玩家认证器）推导并覆盖 UE 自报身份。当前 UE 客户端 Bootstrap 已按该路径构造请求，但 Backend 路由尚未实现。

目标服务器接入合同：`POST /internal/telemetry/v1/batches`，使用受保护 Bearer Token（持有者令牌）以及 X-Game-Server-Id/X-Server-Role 等服务端身份头。DBAServer 已按该合同装配发送端，但 Backend 路由和 Server Registry 核验尚未实现。

未来后端接入响应建议仅返回 BatchId、accepted_count、rejected_count、retryable、server_time_utc，不返回业务数据；该响应结构当前尚未形成 Shared 正式契约。