# Architecture（架构）

GamePlatformTelemetry（游戏平台遥测插件）位于 GameFoundation/Diagnostics（游戏平台基础层/诊断分类），只保留一个 Runtime（双端运行时）模块，客户端和专用服务器复用同一 Event/Metric/Buffer/Sink（事件/指标/缓冲/输出器）模型。

公共插件不依赖 GamePlatformOnline/GamePlatformSession（在线/会话插件）。认证由项目组合层注入：DBAClient（神兽联盟客户端）注入 Gateway URL + AccessToken；DBAServer（神兽联盟服务端）从服务端环境注入 GameServerControl URL + Server Identity。

后端不创建 TelemetryService（遥测独立服务）。Client Ingest 由 GatewayService 承载，Server Ingest 由现有 GameServerControlService 承载，二者共享 Backend/gameplatform/telemetry（公共遥测领域）。