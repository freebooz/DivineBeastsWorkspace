# ClientCacheAndViewModel（客户端缓存与显示模型）

`UGamePlatformInventoryClientSubsystem（背包客户端子系统）`状态为 Uninitialized、Loading、Ready、Mutating、Reconciling、Error。它只保存当前 LocalPlayer 的最新权威 Snapshot 与一个 Pending Operation；不做复杂 Optimistic UI（乐观界面），服务端明确成功后才替换 Snapshot。

Snapshot 现在包含 `Containers（容器容量快照） + Items（物品实例） + Quickbar（快捷栏引用） + InventoryRevision`。应用新快照前会验证重复实例、重复容器槽位、容量越界、重复快捷栏槽位和悬空引用；派生排序缓存和 ItemInstanceId 索引只由 SnapshotGeneration 驱动，不成为第二份业务真源。

`FGamePlatformInventoryItemViewModel（背包物品显示模型）`提供 ItemInstanceId、ItemDefinitionId、Quantity、MaxStackSize（服务端权威堆叠上限）、ContainerId、SlotIndex、Pending 和 DisplayDefinitionHandle。MaxStackSize 只用于 UI 展示与可合并数量预判，不能替代服务端写入校验。ViewModel 不创建 Widget，不持有项目图标/网格等硬引用；项目 UI/内容层根据稳定 DefinitionId 解析项目显示资产。

`LastError（最后错误）`由子系统公开给 UI。确定性业务错误可以在状态恢复 Ready 后继续保留 LastError，便于页面显示失败原因；下一次成功状态迁移会清除错误。UI 通过 OnChanged 事件刷新，不使用 Tick 轮询。

排序只影响显示顺序，不修改权威 Slot。当前按 Container、Slot、Definition 排序；自动整理背包、分页/Delta 同步仍未实现，只有真实规模数据证明全量快照成为瓶颈后再引入。
