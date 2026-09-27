# API（公开接口）

共享核心：`UGamePlatformQuestDefinition（平台任务定义）`、`FGamePlatformQuestObjectiveDefinition（任务目标定义）`、`FGamePlatformQuestSnapshot（任务快照）`、`FGamePlatformQuestEvent（任务事件）`、`FGamePlatformQuestStateMachine（任务状态机）`和 `UGamePlatformQuestStateComponent（任务状态组件）`。

客户端核心：`UGamePlatformQuestClientSubsystem（任务客户端子系统）`，提供 ApplyAuthoritativeSnapshots（应用权威快照）、FindQuest（查询任务）、TrackQuest/UntrackQuest（追踪/取消追踪）、GetSortedSnapshots（排序快照）。它没有 Progress/Complete/Reward 的服务器写入口。

服务器核心：`UGamePlatformQuestServerSubsystem（任务服务器子系统）`，提供 Definition 注册/依赖图验证、RegisterPlayer（注册玩家）、AcceptQuest（接取）、AbandonQuest（放弃）、HandleQuestEvent（处理可信事件）、FlushPlayerProgressNow（迁服/登出前强制刷新）。

后端调用通过 `IGamePlatformQuestPersistencePort（任务持久化端口）`隔离。当前 UE 端没有具体 HTTP 绑定实现，因此端口到 PlayerDataService 的真实网络联调仍为未执行，不能把接口存在写成联调通过。
