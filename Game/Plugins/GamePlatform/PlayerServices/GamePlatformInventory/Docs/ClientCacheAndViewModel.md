# ClientCacheAndViewModel（客户端缓存与显示模型）

`UGamePlatformInventoryClientSubsystem（背包客户端子系统）`状态为 Uninitialized、Loading、Ready、Mutating、Reconciling、Error。第一版不做复杂 Optimistic UI（乐观界面）；写操作只标 Pending（处理中），服务器成功后才替换 Snapshot。

`FGamePlatformInventoryItemViewModel（背包物品显示模型）`提供 ItemInstanceId、ItemDefinitionId、Quantity、ContainerId、SlotIndex、Pending 和 DisplayDefinitionHandle。它不创建 Widget（界面组件），正式 UI 后续消费该中立模型。

排序只影响显示顺序，不修改权威 Slot。当前按 Container、Slot、Definition 排序；自动整理背包未实现。
