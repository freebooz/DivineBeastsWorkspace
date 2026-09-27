# UnrealInsightsAndTracing（虚幻性能分析与追踪）

GamePlatformTelemetry 定义可选 `GamePlatformTelemetryChannel（平台遥测追踪通道）`，使用 UE_TRACE_EVENT_BEGIN/END 和 UE_TRACE_LOG（追踪事件定义/写入）。

Trace Bridge（追踪桥）只写短 EventName、Sequence、CorrelationId 和 Bookmark，不写 AccessToken、Receipt 或 Secret。

Subsystem 只有在 TraceBridgeEnabled（追踪桥启用）时调用自定义 Trace；真正事件还要求运行时启用对应 Trace Channel。Shipping（发布版）默认不应开启高频 Trace。

Unreal Insights Runtime（虚幻性能分析运行验证）当前未执行。