# NetworkingAndReplication（网络与复制）

`UGamePlatformQuestStateComponent（任务状态组件）`将 QuestSnapshots 以 `COND_OwnerOnly（仅拥有者）`复制给当前玩家。客户端不接收服务器 Event 去重缓存、Persistence Port、数据库凭据、Completion事务或 Outbox 内部状态。

`UGamePlatformQuestClientSubsystem（任务客户端子系统）`只应用权威 Snapshot；Revision 更旧的快照不会覆盖更新状态。Track/Untrack 是本地展示偏好，不修改服务器 Revision 或 Objective 进度。

QuestServer 没有公开 `ProgressDelta`、CompleteQuest、GrantReward 等客户端 Server RPC。游戏行为先在服务器形成 Combat/Interaction/Region/Gameplay 事实，再由 DBAServer 项目组合适配为 QuestEvent。

Dedicated Server + 1/2 Client、OwnerOnly 隔离、Late Join（晚加入）恢复、断线重连、A 玩家事件不推进 B 玩家等真实网络测试因 UE_ROOT/测试 Definition/真实 Transport 缺失而未执行。
