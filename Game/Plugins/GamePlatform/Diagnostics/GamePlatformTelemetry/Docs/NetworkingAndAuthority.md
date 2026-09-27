# NetworkingAndAuthority（网络与权威）

GamePlatformTelemetry（游戏平台遥测插件）不依赖 GamePlatformOnline/GamePlatformSession（在线/会话插件），通过 `IGamePlatformTelemetryTransport（遥测传输接口）`解耦认证与网络。

DBAClient（神兽联盟客户端组合层）接受正式登录链传入的 Gateway URL + AccessToken 并配置客户端 NetworkSink；DBAServer（神兽联盟服务端组合层）从环境读取 GameServerControl URL、Telemetry Server Token、ServerInstanceId、ServerRole、Region、BuildVersion。

后端 Client Ingest 用 PlayerAuthenticator 验证玩家，Server Ingest 使用受保护 Server Token。当前 GameServerControl 没有正式 Server Registry（服务器注册表）身份核验，因此“ServerInstanceId 确实为已分配实例”的控制面验证仍未执行。

没有可靠 UE API 时不会虚构 RTT、packet loss 或 replication budget（往返时延、丢包、复制预算）。