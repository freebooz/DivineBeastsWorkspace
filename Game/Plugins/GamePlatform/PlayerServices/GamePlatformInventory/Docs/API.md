# API（接口）

客户端公开 `UGamePlatformInventoryClientSubsystem（背包客户端子系统）`。只读接口包括 State、LastError、Snapshot、InventoryRevision、Pending Operation、排序 Items、ViewModel 与按 ItemInstanceId 查询；写请求包括 Refresh、RetryPendingOperation、Move、Split、Merge、Set/Clear Quickbar。

`IGamePlatformInventoryClientTransport（背包客户端传输接口）`是唯一公开传输边界，供测试或受控宿主替换。默认 `FGamePlatformInventoryGatewayHttpTransport（背包网关在线适配器）`已收回模块 `Private/Transport（私有传输实现）`，不再作为公共 API；它不直接持有 HTTP/AccessToken，而是委托 `UGamePlatformOnlineClientSubsystem::SendAuthenticatedRequest`。写操作把同一个 OperationId 同时作为请求 Body 业务键与 Online Idempotency-Key（幂等请求头），网络结果未知时保留原 OperationId。

公网共享契约真源是 `Shared/Contracts/GamePlatform/OpenAPI/inventory.openapi.yaml`，路径包括：
- `GET /v1/inventory`
- `GET /v1/inventory/operations/{operationId}`
- `POST /v1/inventory/move`
- `POST /v1/inventory/split`
- `POST /v1/inventory/merge`
- `POST /v1/inventory/quickbar/set`
- `POST /v1/inventory/quickbar/clear`

公网 `inventoryRevision/revision/expectedRevision` 均使用十进制字符串；UE 内部和 Go 领域内部仍使用 int64。错误响应必须通过稳定 `errorCode` 映射，客户端逻辑禁止解析 message 文本。

Go 服务内部 HTTP DTO 直接使用 int64 Revision；生产 gRPC 协议源为 `Backend/internal/contracts/proto/player-data-service.proto`。当前 Runner 缺少 Codegen 工具，因此新增 Inventory OpenAPI/Proto 的 Generated 绑定尚未更新，不能宣称生成代码已通过。普通客户端没有 Grant/Consume API。
