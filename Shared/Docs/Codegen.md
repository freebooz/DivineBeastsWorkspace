# Shared Contract Codegen（共享契约代码生成）

## 1. 唯一真源

`Shared/Contracts` 是 UE 与 Go 跨语言协议唯一真源。禁止从 `Backend/generated` 或 `Shared/Generated/Cpp` 反向修改协议。

生成物只允许进入：

- `Backend/generated/gameplatform`、`Backend/generated/divinebeasts`：离线Contract Registry/Catalog（契约注册表/目录）；
- `Backend/generated/proto`：正式 Protobuf/gRPC Go Binding（Go绑定）；
- `Backend/generated/openapi`：正式 OpenAPI Go types/client（类型/客户端）；
- `Shared/Generated/Cpp`：UE C++ 可消费的生成绑定与消费者映射。

## 2. 离线确定性生成

从 `Backend/` 执行：

```bash
go run ./internal/tools/contractcodegen -workspace-root=..
go run ./internal/tools/contractcodegen -workspace-root=.. -check
```

该步骤不依赖外部网络，生成并校验：

- ContractVersion（契约版本）；
- ServerRole / Experience / ArenaMode（服务器角色/体验/竞技模式）；
- OpenAPI OperationId（操作编号）及 HTTP Method/Path（方法/路径）路由表；
- Proto Service/RPC（服务/RPC）注册表；
- `Shared/Generated/Cpp/UEConsumerModules.generated.json` UE消费者模块映射。

## 3. 正式 Proto/OpenAPI 生成

安装 `codegen-tools.lock.json` 中锁定的工具版本后，从 `Backend/` 执行。生成器会先逐一校验 `protoc / protoc-gen-go / protoc-gen-go-grpc / oapi-codegen` 的实际版本，不匹配立即失败：

```bash
go run ./internal/tools/contractcodegen -workspace-root=.. -official
```

正式模式执行：

1. `Shared/Contracts/GamePlatform/Proto` → `Backend/generated/proto/shared/gameplatform/...`；
2. `Shared/Contracts/Games/DivineBeasts/Proto` → `Backend/generated/proto/shared/divinebeasts/...`；
3. 上述两类 Proto 同时 → `Shared/Generated/Cpp/.../Proto`；
4. `Backend/internal/contracts/proto` → `Backend/generated/proto/internal/...`；
5. 每个 OpenAPI 文件独立生成到 `Backend/generated/openapi/<scope>/<spec>/`，避免不同规范中的类型重名。

Go Proto 输出使用：

```text
--go_out=Backend --go_opt=module=divinebeasts/backend
--go-grpc_out=Backend --go-grpc_opt=module=divinebeasts/backend
```

由各 `.proto` 的 `go_package` 决定最终目录，因此生成路径与 gRPC Adapter（适配器）导入路径保持一致。

## 4. UE C++消费者边界

本交付只包含 `Shared + Backend`，因此不会修改 `Game/Plugins` 下的 `.Build.cs`。Shared 侧已经生成 `UEConsumerModules.generated.json`，正式 UE 工程应按该映射将 `Shared/Generated/Cpp` 作为只读生成代码接入以下模块：

- `GamePlatformOnline（游戏平台在线服务）`；
- `GamePlatformSession（游戏平台会话）`；
- `GamePlatformServer（游戏平台服务器）`；
- `GamePlatformArenaServer（游戏平台竞技服务器）`；
- `DivineBeastsRuntime（神兽联盟运行时）`；
- `DivineBeastsServer（神兽联盟服务器）`；
- `DivineBeastsArena（神兽联盟竞技）`。

生成 C++ 仅承担 DTO（数据传输对象）、Proto消息、枚举、序列化绑定以及 OpenAPI OperationId/HTTP Method/Path 路由常量，不允许承载 Gameplay（游戏玩法）、HTTP执行器或业务实现。

## 5. CI门禁

建议顺序：

```bash
go run ./internal/tools/contractcodegen -workspace-root=.. -check
go test ./tests -run 'Contract|Codegen'
go test ./...
go vet ./...
go test -race ./...
```

联网构建机额外执行：

```bash
go mod download
go run ./internal/tools/contractcodegen -workspace-root=.. -official
go test -tags=productiondeps,grpcdeps ./...
```

任何手工修改 `generated` 目录、生成物过期、Proto字段号破坏或Client/Server/Backend版本不兼容都应阻止合并。
