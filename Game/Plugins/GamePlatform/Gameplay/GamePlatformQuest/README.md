# GamePlatformQuest（游戏平台任务插件）

跨游戏通用任务、教学进度和活动目标框架。包含 `GamePlatformQuest（共享Runtime模块）`、`GamePlatformQuestClient（客户端ClientOnly模块）`和 `GamePlatformQuestServer（服务器ServerOnly模块）`。

已实现源码：Quest/Objective Definition、状态机、OwnerOnly Snapshot、客户端 Revision/Tracking、服务器 EventType→Objective 索引、QuestId+EventId 去重、Accept/Abandon、Objective评估、CompletionId、RewardClaim边界、普通进度短周期批量持久化、跨服前Flush/Reconcile，以及 DBAServer Combat/Interaction/Region 中立事件适配。

后端已扩展现有 PlayerDataService：Go Quest领域、pgx PostgreSQL Repository、正式 Migration、Revision乐观并发、processed_event幂等、completion ledger、PlayerQuestCompleted Transactional Outbox 和通用Outbox Dispatcher。没有新增QuestService，也没有直接修改长期货币。

DBAServer 已实现异步 `FHttpModule（HTTP模块）` → PlayerDataService（玩家数据服务）持久化适配，QuestServer 的 Persistence Port 已改为异步 Completion（完成回调）模型，并以单玩家单飞持久化、Deferred Event（延迟事件）、Revision Reconcile（修订号对账）避免游戏线程阻塞和旧版本覆盖。当前 Runner 无 Go/psql/PostgreSQL/UE5.8 工具链，因此 Go 编译、Migration 实际执行、UE→PlayerData 真实运行联调、Outbox 消息发布、专服/双客户端/重启恢复和 Cook 均未执行。

