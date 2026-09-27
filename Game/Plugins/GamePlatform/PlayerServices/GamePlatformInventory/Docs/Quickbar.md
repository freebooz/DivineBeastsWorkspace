# Quickbar（快捷栏）

Quickbar（快捷栏）只引用 ItemInstanceId（物品实例编号），不复制 Item 数据。`player_quickbar_slots（玩家快捷栏槽位表）`保存 slot_index、item_instance_id、revision。

Move 保持 ItemInstanceId 不变，因此 Quickbar 引用继续有效。Split 创建新 ItemInstanceId，不自动改旧快捷栏。Merge/Consume 导致源 Item 删除或数量归零时，Repository 在同一事务清理指向该实例的 Quickbar。

客户端 Set/Clear Quickbar 都携带 OperationId 与 ExpectedRevision，服务端成功后返回新 Snapshot。
