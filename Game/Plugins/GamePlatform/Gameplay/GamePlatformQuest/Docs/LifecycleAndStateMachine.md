# LifecycleAndStateMachine（生命周期与状态机）

状态包括 Locked、Available、Accepted、Active、ObjectivesCompleted、CompletionPending、Completed、Abandoned、Failed、Expired。`FGamePlatformQuestStateMachine`集中定义合法转换，Completed 等终态不能回退为 Active。

Accept 时服务器先验证 Definition、OneShot历史完成、当前是否已激活、Prerequisite 和最大Active数量，再由 Available → Active 状态机创建新 QuestInstanceId；关键接取状态必须由 Persistence Port 成功持久化后才写入运行时。

全部必需 Objective 完成后，Server 先经 Active → ObjectivesCompleted → CompletionPending；生成 CompletionId，若有 RewardSet 才生成 RewardClaimId。只有后端完成事务返回的 Snapshot 才进入当前运行快照的 Completed。

Abandon 只在 Definition 明确允许时执行并经过状态机；玩家 Unregister（注销）前尝试 Flush，Flush 失败则保留 Runtime 而不是静默删除尚未持久化进度。
