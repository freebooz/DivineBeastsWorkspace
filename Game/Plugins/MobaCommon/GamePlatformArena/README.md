# GamePlatformArena（游戏平台竞技场插件）

正式路径为`Game/Plugins/MobaCommon/GamePlatformArena/`，不再位于平台层GameModes分类。它仍是40个既定GamePlatform身份之一；项目竞技扩展由DBAArena单向依赖本插件。

`GamePlatformArena（游戏平台竞技场插件）` 是 `MobaCommon（MOBA通用层）` 唯一通用竞技插件，内部固定为 `GamePlatformMobaCore（MOBA核心模块）`、`GamePlatformMobaData（MOBA数据模块）`、`GamePlatformArena（双端竞技运行模块）`、`GamePlatformArenaClient（竞技客户端模块）`、`GamePlatformArenaServer（竞技服务器模块）` 五个模块。

第一版统一支持 1v1、2v2、3v3、4v4、5v5，全部使用 `GameServer.Role.MainArena（主竞技场服务器角色）` 与同一个 `DivineBeastsArenaServer（神兽联盟服务器构建目标）`，运行时仅通过 `ArenaModeId（竞技模式编号）` 选择规则。

当前源码已实现模式定义、Assignment（比赛分配）、Roster（名单）、TransferTicket（转服票据）准入接口、选人/Ready、比赛阶段、服务器时间、比分/KDA、断线重连、弃权、结果构建、后端幂等提交、PostgreSQL（关系数据库）、Transactional Outbox（事务外发）与 `Match.Completed（比赛完成事件）` JetStream（流式消息）适配。

真实 UE5.8 编译、Dedicated Server（专用服务器）1v1～5v5联调、Client/Server Cook（客户端/服务器烘焙）与性能基线必须以实际环境执行结果为准；源码存在不等于这些项目已经通过。
