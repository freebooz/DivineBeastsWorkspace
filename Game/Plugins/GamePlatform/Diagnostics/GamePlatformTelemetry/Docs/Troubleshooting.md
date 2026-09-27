# Troubleshooting（故障排查）

RecordEvent 返回 InvalidEventName：确认 EventName 已注册且符合 Domain.Action.Result。ForbiddenAttribute：检查是否出现 token、receipt、card、secret、cookie 等禁止字段。

MetricLabelNotAllowed：检查是否把 PlayerId/SessionId/MatchId/OrderId 等高基数标识放入 Metric Label。

BufferFull/RateLimited：降低事件频率、调整 Sampling 或合理提高有界容量；不要改成无界队列。

Gateway Telemetry 503：检查 NATS_URL/JetStream Stream、网络和凭据；普通 Gateway API 不应受影响。

Server Telemetry 404：可能未配置 TELEMETRY_SERVER_INTERNAL_TOKEN，因此 GameServerControl 未注册遥测路由。401 则检查 Token 与 Server Identity Header。

Trace 不显示：确认 TraceBridgeEnabled 且运行参数启用了 GamePlatformTelemetry Trace Channel。