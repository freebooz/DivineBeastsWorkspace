# internal/contracts/proto（仅Go服务内部RPC协议）

本目录是 Backend 内部服务间RPC的唯一真源，例如 GatewayService → IdentityService、GatewayService → PlayerDataService、MatchService → GameServerControlService。

这些协议只服务Go后端进程之间，不跨UE边界，因此禁止移动到 `Shared/Contracts`。

凡是 UE Client 或 UE Dedicated Server 需要理解的协议，必须定义在 `Shared/Contracts/GamePlatform` 或 `Shared/Contracts/Games/DivineBeasts`。
