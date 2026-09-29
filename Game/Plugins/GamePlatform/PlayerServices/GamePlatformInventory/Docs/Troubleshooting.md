# Troubleshooting（故障排查）

客户端长期 `Uninitialized（未初始化）`：先检查 `GamePlatformOnlineClient（平台在线客户端）`是否真正进入 `Authenticated（已认证）`。Inventory 现在自动订阅 Online 认证事件，不需要业务层手工创建 Transport，也不保存 AccessToken。若 Online 未认证，先排查登录/刷新链，不要给 Inventory 增加旁路 Token。

`InventoryNotLoaded（背包未加载）`：确认 LocalPlayer 所属 GameInstance 已创建 Online 子系统，并确认 `GamePlatformInventory.uplugin` 已启用 `GamePlatformOnline`。认证成功后 Inventory 应自动进入 Loading → Ready；如果进入 Error，读取 `GetLastError()`和 `LogGamePlatformInventory`，不要轮询 UI Tick。

`RevisionConflict（修订冲突）`：客户端会进入 Reconciling（对账中）并发起一次专用全量 Snapshot。普通 Refresh 在存在未决写操作时会拒绝，避免误清 Pending。不要生成新的 OperationId 盲重试。

网络断开、超时、响应解析失败且 Pending 保留：结果可能已经到达服务端。调用 `RetryPendingOperation（重试待处理操作）`会先查询原 `GET /v1/inventory/operations/{operationId}`；已提交则直接应用持久结果，仅明确返回 `INVENTORY_OPERATION_NOT_FOUND` 时才复用原 OperationId 重发。

`INVENTORY_SLOT_OCCUPIED / INVENTORY_INVALID_QUANTITY / INVENTORY_STACK_LIMIT_EXCEEDED` 等确定性业务错误：Pending 会清理并恢复 Ready，`GetLastError()`仍保存稳定错误码供 UI 提示。客户端逻辑不能解析服务端 message 文本。

`SERVICE_UNAVAILABLE（服务不可用）`：HTTP 模式检查 Gateway→PlayerData 内部网络、PostgreSQL 和 `000008_player_inventory.sql` 是否已应用。当前生产 gRPC Inventory 生成绑定尚未完成；若部署使用 `productiondeps,grpcdeps`，Inventory 公网接口会保持不可用，直到恢复 Codegen 并实现对应 grpcclient/grpcadapter。

当前尚没有正式 Grant/Consume、Quest Reward 或 External Pickup 链路；遇到这类需求不要参考旧文档中的 `INVENTORY_ITEM_POLICY_FILE` 或 Pickup Reservation 说明，它们并非当前已实现能力。
