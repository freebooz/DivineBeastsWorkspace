# Architecture（架构）

GamePlatformTelemetry（游戏平台遥测插件）位于 GameFoundation/Diagnostics（游戏平台基础层/诊断分类），只保留一个 Runtime（双端运行时）模块，客户端和专用服务器复用同一 Event/Metric/Buffer/Sink（事件/指标/缓冲/输出器）模型。

公共插件不依赖 GamePlatformOnline/GamePlatformSession（在线/会话插件）。认证由项目组合层注入：DBAClient（神兽联盟客户端）已通过薄 Bootstrap 注入 Gateway URL，并使用动态 Header Provider 在每次发送前瞬时读取当前 Authorization，不在 Telemetry Transport 长期保存 AccessToken；DBAServer（神兽联盟服务端）从受控环境动态读取内部遥测 Token 和服务器身份。

后端仍不建议创建独立 TelemetryService（遥测服务）。规划目标是 Client Ingest 由 GatewayService、Server Ingest 由 GameServerControlService 承载；但当前仓库真实 Backend 只有 `Backend/internal/modules/telemetry/doc.go` 领域占位，具体 Handler、共享契约、NATS 发布尚未实现，不能把规划描述成已完成链路。