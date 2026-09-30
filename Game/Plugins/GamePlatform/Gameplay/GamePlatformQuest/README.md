# GamePlatformQuest（游戏平台任务插件）

跨游戏通用任务、教学进度和活动目标框架。包含GamePlatformQuest共享运行模块、GamePlatformQuestClient客户端模块与GamePlatformQuestServer服务器模块。

当前源码实现：定义/状态机、仅拥有者快照、客户端追踪与Revision/SnapshotSequence保护、服务器事件索引、QuestId+EventId去重、接取/放弃、目标评估、Completion/Reward边界、异步持久化端口、单玩家单飞、普通进度聚合、冲突对账及失败保留事件。本轮修复对账先撤销原记录造成的满队列/不兼容快照丢事件，并区分显示序列和落库Revision。

当前DBAServer文件清单未发现旧文档声称的QuestIntegration/UDBAQuestEventAdapterComponent或FDBAQuestPlayerDataPersistence。平台Persistence Port存在不代表具体HTTP适配、数据库迁移、Outbox、奖励链或UE→后端已联调；本轮没有实施或验收这些业务。旧后端方案材料仅供后续项目集成审查。

实际本轮Native C++策略回归已通过，UE专属回归源码放在模块Private/Tests，尚由统一主执行者执行引擎编译/Automation。网络、专服双客户端、重启恢复、数据库/Outbox联调及Cook未由本Task执行。详细行为与中文责任/失败/取消合同见[本轮专属说明](Docs/DesignRemediation-2026-09-30.md)。
