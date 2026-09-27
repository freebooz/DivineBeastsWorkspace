# TelemetryAndDiagnostics（遥测与诊断）

平台 UI 当前通过明确失败事件 `OnScreenOpenFailed`和静态门禁提供基础诊断，但没有新增业务 Telemetry（遥测）协议。

后续可把页面打开耗时、加载失败、路由阻止、焦点失败等中立指标发送到 `GamePlatformTelemetry（平台遥测插件）`，但不得携带密码、Token 或敏感后端数据。

Debug Layer（调试层）仅供开发目标，Shipping 必须禁用/剥离。

本轮没有修改后端日志或新增监控服务。
