# BackendIntegration（后端联调）

新增后端接口：无。新增 Go 微服务：无。原因：实时战斗由 UE Dedicated Server（专用服务器）权威执行。

Combat 不知道 PostgreSQL、Gateway、PlayerData、MatchService 或 GameServerControlService，不在每次命中时同步请求 Go。

完整联网测试未来应复用 Online 认证、Session 准入、玩家资格、服务器实例和角色配置；当前这些前置插件本身仍未形成可验证的真实网络链，因此本轮没有声称后端联调已执行。

后续对局最终可信结果持久化属于比赛/业务后端职责，不由 Combat 在运行期直接写库。
