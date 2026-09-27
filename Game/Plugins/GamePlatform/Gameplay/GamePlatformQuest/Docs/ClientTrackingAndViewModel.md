# ClientTrackingAndViewModel（客户端追踪与视图模型）

`UGamePlatformQuestClientSubsystem`按 QuestId 缓存当前玩家 Snapshot；只接受相同或更高 Revision，明确拒绝旧 Revision 覆盖新状态。

TrackQuest/UntrackQuest 是本地展示偏好，受 ClientMaxTrackedQuests 配置限制，不改变 Server Quest Progress、Revision、Completion 或 Reward。当前没有接入 GamePlatformSave，因此追踪偏好只保证当前会话内存状态。

GetSortedSnapshots 提供稳定状态/QuestId排序，可作为未来 UI ViewModel（视图模型）数据源；QuestClient 不依赖正式 GamePlatformUI，也不创建 Widget。

OwnerOnly QuestStateComponent 只向任务状态拥有者复制 Snapshot；服务器私有 Event去重、Persistence凭据、Outbox/Reward内部事务不会复制给 Client。
