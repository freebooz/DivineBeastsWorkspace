# BackendIntegration（后端集成）

继续复用现有 `MatchService（匹配服务）` 与 `GameServerControlService（游戏服务器控制服务）`，没有新增Arena微服务。MatchService生成Party原子的Roster、MainArena Assignment和TransferTicket；GameServerControl持久化Assignment并提供服务器读取、Ticket验证消费与结果提交。

当前源码没有完整GameServer Allocation（游戏服务器分配器）实现，因此从匹配成功到真实MainArena实例分配仍是后续联调断点，不能标记为端到端通过。

`ResultPending（结果待提交）` 已由 `GamePlatformArenaServer（竞技服务器模块）` 自动触发有界幂等提交；提交成功后进入Completed（完成）。当前 `GamePlatformServer（游戏平台服务器插件）` 尚无真实BeginDrain/Release（排空/释放）接口，因此服务器进程生命周期释放仍保留为明确续接点，不在Arena内部复制第二套Server Registry（服务器注册系统）。