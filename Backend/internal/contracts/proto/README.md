# internal/contracts/proto（仅Go服务内部RPC协议）

本目录是 Backend 内部服务间RPC的唯一真源，例如 GatewayService → IdentityService、GatewayService → PlayerDataService、MatchService → GameServerControlService。

这些协议只服务Go后端进程之间，不跨UE边界，因此禁止移动到 `Shared/Contracts`。

凡是 UE Client 或 UE Dedicated Server 需要理解的协议，必须定义在 `Shared/Contracts/GamePlatform` 或 `Shared/Contracts/Games/DivineBeasts`。

`player-data-service.proto（玩家数据内部协议）`现已补充 Inventory（背包）查询、Operation 查询、Move/Split/Merge 与 Quickbar RPC 协议源；这些消息中的 Revision 使用 int64，因为只在 Go↔Go 内部 Protobuf 传输。UE Client 看到的公网契约仍以 `Shared/Contracts/GamePlatform/OpenAPI/inventory.openapi.yaml`为真源，并将 Revision 表示为十进制字符串。

当前 Runner 缺少 `protoc/protoc-gen-go/protoc-gen-go-grpc`，因此 Inventory RPC 的生成绑定及 grpcclient/grpcadapter 尚未生成。禁止手工编辑 `Backend/internal/generated` 冒充 Codegen 完成；恢复工具链后必须重新生成并执行生产 gRPC 构建验证。
