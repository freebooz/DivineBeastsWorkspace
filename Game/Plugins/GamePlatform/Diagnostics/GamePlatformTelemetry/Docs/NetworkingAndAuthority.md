# NetworkingAndAuthority（网络与权威）

GamePlatformTelemetry（游戏平台遥测插件）不依赖 GamePlatformOnline/GamePlatformSession（在线/会话插件），通过 `IGamePlatformTelemetryTransport（遥测传输接口）`解耦认证与网络。

DBAClient（神兽联盟客户端组合层）已配置客户端 NetworkSink：Gateway URL 来自项目环境，Authorization 通过 Online 瞬时接口在每个 Batch 发送前动态获取，因此令牌刷新后无需重建 Transport。DBAServer 从环境读取 GameServerControl URL、Telemetry Server Token、ServerInstanceId、ServerRole、Region 等信息；缺失配置时遥测降级，不阻断服务器生命周期。

UE 发送端已经区分 Client/Server 身份注入，但后端 Client Ingest、Server Ingest、PlayerAuthenticator 覆盖和 Server Registry（服务器注册表）实例核验当前尚未实现。生产环境必须在后端再次覆盖客户端/服务器不可信身份，不能信任 UE Payload 自报的 Session/ServerInstance 字段。

没有可靠 UE API 时不会虚构 RTT、packet loss 或 replication budget（往返时延、丢包、复制预算）。

NetworkSink（网络输出器）断线处理只属于遥测传输可靠性，不替代 Gameplay/Session 重连：408/429/5xx/transport error 和 HTTP 请求瞬时启动失败采用有限次数、最大年龄、指数退避、确定性抖动及 Retry-After；不可重试 4xx 直接终止该 Batch。Shutdown 会撤销待触发重试，并给已发请求有限非阻塞预算。