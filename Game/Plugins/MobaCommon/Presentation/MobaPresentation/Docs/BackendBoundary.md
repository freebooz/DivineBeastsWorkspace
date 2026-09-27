# BackendBoundary（后端边界）

新增Go业务后端接口：无。

MobaPresentation不调用GatewayService（统一接入服务）、MatchService（匹配服务）、PlayerData（玩家数据服务）或GameServerControlService（游戏服务器控制服务）。

竞技/战斗事实已经通过UE权威网络状态到达客户端；后端不向MobaPresentation返回具体VFX/SFX/UI资源路径。

Telemetry（遥测）可记录请求数、ProviderMissing、Drop和Resolve Latency，但遥测不能驱动Gameplay或补发权威事实。
