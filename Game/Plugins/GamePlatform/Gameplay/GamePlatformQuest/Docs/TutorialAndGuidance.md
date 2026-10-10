# TutorialAndGuidance（教学与引导）

> 2026-09-30状态校正：本页保留旧方案/历史证据，旧DBAServer具体Quest HTTP/事件适配与后端交付宣称未在当前项目文件清单确认，不能作为现行验收。当前行为以[本轮整改说明](DesignRemediation-2026-09-30.md)、README与真实源码为准；本轮未执行数据库/Outbox/网络联调。

第一版共享模型支持 `Tutorial（教学）` ObjectiveType，可用服务器可信 Gameplay/Tutorial 事件推进。推荐 Development 教学链：进入训练区域 → 与训练祭坛交互 → 击败训练 Dummy。

该链分别来自 World/Region、Interaction Committed、Combat Death 的服务器事实，可验证事件适配、Objective 聚合、Completion、PlayerData 持久化和恢复。

当前没有真实 `DA_Quest_FoundationTutorial`资产，也没有正式剧情 UI、对话树或 Cutscene（过场）；因此完整教学体验运行状态为未执行。

QuestClient 只提供中立 Snapshot/Tracking 数据，未来 UI 可以消费，不在 Quest 插件内创建 Widget 或剧情表现。
