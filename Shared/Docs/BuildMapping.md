# BuildMapping（协议生成与编译映射）

本文件定义 `Shared（UE与Go共享契约层）` 的唯一生成方向与消费者映射。本交付只修改 `Shared + Backend`；UE `.Build.cs（模块构建规则）` 位于 `Game/Plugins`，不包含在本代码包中。

## 1. 生成方向

| 协议真源 | C++生成物 | Go生成物 | 说明 |
|---|---|---|---|
| `Shared/Contracts/GamePlatform/Proto` | `Shared/Generated/Cpp/GamePlatform/Proto` | `Backend/generated/proto/shared/gameplatform/...` | UE Dedicated Server ↔ Backend 公共RPC |
| `Shared/Contracts/Games/DivineBeasts/Proto` | `Shared/Generated/Cpp/Games/DivineBeasts/Proto` | `Backend/generated/proto/shared/divinebeasts/...` | 神兽联盟项目RPC扩展 |
| `Shared/Contracts/GamePlatform/OpenAPI` | 不生成C++业务客户端 | `Backend/generated/openapi/gameplatform/<spec>` | 公共HTTP DTO/Client |
| `Shared/Contracts/Games/DivineBeasts/OpenAPI` | 不生成C++业务客户端 | `Backend/generated/openapi/divinebeasts/<spec>` | 项目HTTP DTO/Client |
| `Backend/internal/contracts/proto` | 禁止进入Shared | `Backend/generated/proto/internal/...` | 仅Go服务内部gRPC |

离线 Catalog/Registry 生成物仍分别位于：

- `Backend/generated/gameplatform`；
- `Backend/generated/divinebeasts`；
- `Shared/Generated/Cpp/GamePlatform`；
- `Shared/Generated/Cpp/Games/DivineBeasts`。

## 2. 强制边界

1. `Shared/Generated/` 下只允许 `Cpp/`。
2. `Shared/Generated/Cpp/` 和 `Backend/generated/` 均禁止手工修改。
3. `Shared/Contracts` 是 UE/Go 跨语言协议唯一真源。
4. `Backend/internal/contracts/proto` 仅用于 Go 服务间 RPC，不能被 UE 直接消费。
5. 具体 ServerRole/Experience/ArenaMode 属于 `Shared/Contracts/Games/DivineBeasts`，不能写死进 `GamePlatform` 公共协议。

## 3. GamePlatform公共协议 → UE消费者模块

- Identity / PlayerData / Party / Matchmaking：`GamePlatformOnline（游戏平台在线服务）`。
- Session / ServerTransfer：`GamePlatformSession（游戏平台会话）`。
- GameServer Control：`GamePlatformServer（游戏平台服务器）`。
- Match Result：`GamePlatformArenaServer（游戏平台竞技服务器）`。
- 公共ID、错误、版本：`GamePlatformCore（游戏平台核心）` / `GamePlatformData（游戏平台数据）`。

## 4. DivineBeasts协议 → UE消费者模块

- ServerRole / Experience / World：`DivineBeastsServer（神兽联盟服务器）`。
- ArenaMode / Match扩展：`DivineBeastsArena（神兽联盟竞技）`。
- 项目上下文：`DivineBeastsRuntime（神兽联盟运行时）`。

实际映射同时写入机器可读文件：

```text
Shared/Generated/Cpp/UEConsumerModules.generated.json
```

UE模块不得复制 Shared 协议结构；应把 `Shared/Generated/Cpp` 作为只读 Include/Source 输入，并用薄 Adapter（适配器）转换为 UE 内部类型。

## 5. Backend消费者

- gRPC Adapter 直接导入 `Backend/generated/proto/...`；
- OpenAPI transport/gateway 可逐步切换到 `Backend/generated/openapi/...`；
- Domain（领域）和 Application（应用）代码继续使用稳定内部 DTO，不让生成类型侵入核心领域。

这样即使 Shared Contract 升级，生成代码变化也被 Transport/Adapter 边界吸收。
