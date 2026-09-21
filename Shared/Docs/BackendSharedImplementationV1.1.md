# 《神兽联盟》Backend + Shared 工程实现说明 V1.1.0

## 1. 交付范围

本版本仅实现：

- `DivineBeastsWorkspace/Backend`：Go（Go语言）业务后端；
- `DivineBeastsWorkspace/Shared`：UE（虚幻引擎）与 Go 跨语言共享契约层。

不修改 `Game / Build / Deploy / Tests / Tools / Docs` 等其他工作空间目录。

正式 Dedicated Server（专用服务器）角色仍只有：

- `GameServer.Role.OpenWorld`：开放世界，承载 `OpenWorld.Hub` 与 `OpenWorld.Main`；
- `GameServer.Role.Village`：新手村，承载 Main / Tutorial / Training；
- `GameServer.Role.MainArena`：主竞技场，承载 1v1～5v5。

不存在独立 `Lobby Server（大厅服务器）`。

---

## 2. 本版本对应的 7 项实现

### 2.1 Shared Proto / OpenAPI 正式 Codegen（代码生成）

实现入口：

```text
Backend/internal/tools/contractcodegen/main.go
```

离线确定性流水线：

```bash
cd Backend
go run ./internal/tools/contractcodegen -workspace-root=..
go run ./internal/tools/contractcodegen -workspace-root=.. -check
```

会生成并校验：

```text
Backend/generated/gameplatform/
Backend/generated/divinebeasts/
Shared/Generated/Cpp/GamePlatform/
Shared/Generated/Cpp/Games/DivineBeasts/
Shared/Generated/Cpp/UEConsumerModules.generated.json
```

OpenAPI（开放接口规范）除 OperationId（操作编号）外，还会生成：

```text
HTTP Method（HTTP方法）
Path（HTTP路径）
OperationId（操作编号）
```

的 Go / C++ 路由目录，UE 消费模块无需再次手工维护 `/v1/...` 路径。

正式生成模式：

```bash
go run ./internal/tools/contractcodegen -workspace-root=.. -official
```

正式模式要求并校验：

```text
protoc                  30.2
protoc-gen-go           1.36.8
protoc-gen-go-grpc      1.5.1
oapi-codegen            2.4.1
```

工具版本来源：

```text
Shared/Docs/codegen-tools.lock.json
```

版本不一致直接失败，防止开发机之间产生不一致生成代码。

正式生成目标：

```text
Shared/Contracts/GamePlatform/Proto
  → Backend/generated/proto/shared/gameplatform/...
  → Shared/Generated/Cpp/GamePlatform/Proto/

Shared/Contracts/Games/DivineBeasts/Proto
  → Backend/generated/proto/shared/divinebeasts/...
  → Shared/Generated/Cpp/Games/DivineBeasts/Proto/

Backend/internal/contracts/proto
  → Backend/generated/proto/internal/...

Shared/*/OpenAPI
  → Backend/generated/openapi/<scope>/<spec>/
```

UE C++ 消费模块映射由：

```text
Shared/Generated/Cpp/UEConsumerModules.generated.json
```

声明，目标包括：

```text
GamePlatformOnline
GamePlatformSession
GamePlatformServer
GamePlatformArenaServer
DivineBeastsRuntime
DivineBeastsServer
DivineBeastsArena
```

由于本次交付范围只允许修改 `Backend + Shared`，因此不直接修改 `Game/Plugins/*/*.Build.cs`；Shared 已提供正式 C++ 生成目录、消费者映射和编译映射，完整 Workspace 集成时由 UE 模块把该目录作为只读 Generated Include/Source 输入。

---

### 2.2 五个 cmd 接入真实 HTTP / gRPC Transport（传输层）

五个入口：

```text
Backend/cmd/gatewayservice/main.go
Backend/cmd/identityservice/main.go
Backend/cmd/playerdataservice/main.go
Backend/cmd/matchservice/main.go
Backend/cmd/gameservercontrolservice/main.go
```

现在只负责：

```text
Signal（进程信号）
→ Config（配置）
→ Composition（装配）
→ HTTP / gRPC Transport
```

业务 Transport 位于：

```text
Backend/internal/transport/httpadapter/
Backend/internal/transport/grpcadapter/
Backend/internal/transport/grpcclient/
```

运行模式：

```text
默认构建
→ HTTP + 本地内存适配器

productiondeps
→ HTTP + PostgreSQL / Redis / NATS / Agones

productiondeps,grpcdeps
→ Gateway公网HTTP + 后端服务间gRPC
  + PostgreSQL / Redis / NATS / Agones
```

实际进程级冒烟测试已经跑通：

```text
Gateway
→ Identity HTTP
→ PlayerData HTTP
→ Match HTTP
```

并完成：

```text
游客登录
→ AccessToken
→ 查询玩家资料
→ 创建Party
→ 创建1v1 MatchmakingTicket
```

---

### 2.3 PostgreSQL / Redis / NATS / Agones 接入 Composition（装配）

生产装配：

```text
Backend/internal/app/composition/composition_production_http.go
Backend/internal/app/composition/composition_production_grpc.go
```

PostgreSQL（关系数据库）：

```text
Backend/internal/platform/database/postgres/
```

当前承载：

- PlayerProfile（玩家长期资料）；
- MatchResult（权威比赛结果）；
- Transactional Outbox（事务发件箱）。

Redis（高速状态存储）：

```text
Backend/internal/platform/cache/redisstore/
```

当前承载：

- Identity Session（身份会话）；
- Party（组队状态）；
- Matchmaking ClientRequestId（匹配幂等状态）；
- TransferTicket Replay Protection（跨服票据防重放）。

NATS JetStream（消息系统）：

```text
Backend/internal/platform/messaging/natsjs/
```

作为 Outbox Dispatcher 的生产 Publisher（发布器），使用 MessageID 作为 JetStream 去重键。

Agones（游戏服务器编排）：

```text
Backend/internal/platform/agones/
```

MainArena 使用 Agones `GameServerAllocation` 原子选择 Ready 实例；OpenWorld / Village 使用已注册常驻服务器池进行容量选择，避免每个玩家进入常驻世界都触发一次 Agones Allocation。

---

### 2.4 OpenWorld.Hub / OpenWorld.Main / Village 世界分配和 Assignment

统一应用服务：

```text
Backend/internal/app/gameservercontrol/service.go
```

当前映射：

```text
Experience.OpenWorld.Hub
Experience.OpenWorld.Main
    → GameServer.Role.OpenWorld

Experience.Village.Main
Experience.Village.Tutorial
Experience.Village.Training
    → GameServer.Role.Village

Experience.MainArena.Main
    → GameServer.Role.MainArena
```

世界迁移新增组合用例：

```text
AllocateWorldTransfer
```

闭环：

```text
请求目标 Experience / World / Region
→ Best-Fit选择Ready/Active常驻服务器
→ 预留玩家容量
→ 生成World Assignment
→ 绑定Assignment签发TransferTicket
→ 客户端连接目标Dedicated Server
→ 目标服务器Validate/Consume Ticket
→ 提交容量预留
```

实际 HTTP 冒烟测试已经验证：

```text
OpenWorld.Hub注册
→ Ready
→ allocate-world-transfer
→ 返回Assignment + Ticket
→ 第一次validate成功
→ 第二次validate返回401重放拒绝
```

MainArena仍使用：

```text
MatchService
→ GameServerControlService
→ Agones Allocation
→ MainArena Assignment
→ 每名玩家TransferTicket
```

---

### 2.5 TransferTicket 防重放迁移 Redis

领域服务不再直接维护内部 `map`，改为：

```text
servertransfer.ReplayStore
```

抽象。

本地：

```text
MemoryReplayStore
```

生产：

```text
redisstore.TransferReplayStore
```

Redis 使用：

```text
SET key value NX EX/PX TTL
```

对应 Go 实现使用 `SetNX`，保证多个 GameServerControlService 进程并发校验同一 Ticket 时只有一个消费者成功。

Redis Key：

```text
transfer:consumed:<TicketId>
```

TTL 与 Ticket 剩余有效期一致，过期后自动释放状态，不形成永久垃圾键。

TransferTicket 签名上下文包含：

```text
TicketId
AssignmentId
GameId
PlayerId
SessionId
SourceGameServerId
DestinationGameServerId
DestinationWorldId
DestinationExperienceId
MatchId
IssuedAt
ExpiresAt
Nonce
Signature
```

---

### 2.6 Transactional Outbox Dispatcher（事务发件箱分发器）

数据库迁移：

```text
Backend/migrations/000002_outbox.sql
```

比赛结果提交采用：

```text
PostgreSQL Transaction
├── INSERT match_results
└── INSERT outbox_messages (Match.Completed)
        ↓
COMMIT
```

保证 MatchResult 和事件不会出现“一边成功、一边失败”。

Dispatcher：

```text
Backend/internal/platform/outbox/outbox.go
```

生产仓储：

```text
Backend/internal/platform/database/postgres/repositories_production.go
```

并发领取语义：

```sql
FOR UPDATE SKIP LOCKED
```

并使用：

```text
state
lock_owner
locked_until
attempts
published_at
```

实现多副本 Worker 租约。

流程：

```text
pending
→ publishing
→ NATS JetStream Publish
→ published
```

发布失败：

```text
Attempts + 1
→ 释放租约
→ pending
→ 下一轮重试
```

NATS 发布成功才允许标记 Published。

---

### 2.7 Client / Server / Backend Contract Compatibility（协议兼容测试）

测试入口：

```text
Backend/tests/contract_compatibility_test.go
Backend/tests/codegen_test.go
```

检查：

1. `Shared/Docs/contract-version.json`；
2. Go Generated ContractVersion；
3. UE C++ Generated ContractVersion；
4. Compatibility Matrix（兼容矩阵）；
5. Client Network Protocol；
6. Server Network Protocol；
7. 当前 ContractVersion 必须位于 supported 兼容范围；
8. Proto关键字段号禁止破坏；
9. OpenAPI `operationId` 全局唯一；
10. 禁止重新出现 `GameServer.Role.Lobby`；
11. Codegen生成物必须与 Shared 当前契约一致。

当前：

```text
ContractVersion = 1.3.0
BackendVersion  = 1.1.0
ClientNetworkProtocol = 1
ServerNetworkProtocol = 1
```

1.3.0 对 1.2.x 采用新增字段方式演进，不复用旧 Proto 字段号。

---

## 3. 当前关键目录

```text
DivineBeastsWorkspace/
├── Backend/
│   ├── cmd/
│   ├── internal/
│   │   ├── app/
│   │   │   ├── composition/
│   │   │   └── gameservercontrol/
│   │   ├── modules/
│   │   │   ├── gameserver/
│   │   │   ├── servertransfer/
│   │   │   └── match/
│   │   ├── platform/
│   │   │   ├── agones/
│   │   │   ├── cache/redisstore/
│   │   │   ├── database/postgres/
│   │   │   ├── messaging/natsjs/
│   │   │   └── outbox/
│   │   ├── transport/
│   │   │   ├── httpadapter/
│   │   │   ├── grpcadapter/
│   │   │   └── grpcclient/
│   │   └── tools/contractcodegen/
│   ├── generated/
│   ├── migrations/
│   ├── configs/
│   └── tests/
│
└── Shared/
    ├── Contracts/
    │   ├── GamePlatform/
    │   └── Games/DivineBeasts/
    ├── Generated/Cpp/
    └── Docs/
```

---

## 4. 验证方式

离线开发环境：

```bash
cd Backend

gofmt -w $(find . -name '*.go' -type f)
go run ./internal/tools/contractcodegen -workspace-root=..
go run ./internal/tools/contractcodegen -workspace-root=.. -check
go test ./...
go vet ./...
go test -race ./...

go build ./cmd/gatewayservice
go build ./cmd/identityservice
go build ./cmd/playerdataservice
go build ./cmd/matchservice
go build ./cmd/gameservercontrolservice
```

联网生产构建机：

```bash
cd Backend

go mod download
go run ./internal/tools/contractcodegen -workspace-root=.. -official
go test -tags=productiondeps,grpcdeps ./...
go vet -tags=productiondeps,grpcdeps ./...
```

---

## 5. 当前验证状态与限制

本交付环境已通过：

- `go test ./...`；
- `go vet ./...`；
- `go test -race ./...`；
- 五个 cmd 独立 `go build`；
- 离线 Codegen + `-check`；
- Shared JSON / OpenAPI YAML 解析；
- Shared + Backend Proto 语法解析；
- Shared C++ 生成头文件 C++17 编译检查；
- 五服务真实进程 HTTP 冒烟测试；
- OpenWorld.Hub 世界分配 / Assignment / TransferTicket / 重放拒绝 HTTP 冒烟测试。

当前执行环境没有联网能力，且仅存在旧 `protoc 3.13.0`，缺少锁定版本的 `protoc-gen-go / protoc-gen-go-grpc / oapi-codegen`。因此：

- `-official` 已验证会因工具版本不匹配而主动失败；
- 没有伪造正式 Proto/OpenAPI 生成结果；
- `go mod download` 无法下载生产依赖，因此 `go.sum` 仍需在联网 CI/开发机按 `go.mod` 生成；
- `productiondeps,grpcdeps` 全量编译应在联网构建机完成。

另外，当前 GameServer Registry / Assignment Runtime State（服务器注册表/任务运行态）仍由单个 GameServerControlService 进程持有；Redis 已用于 TransferTicket 防重放、Session、Party 和匹配幂等。若 GameServerControlService 下一阶段需要多副本主动-主动部署，应继续把 Registry / Assignment 运行态迁移到 Redis 或专门的共享控制面存储。

