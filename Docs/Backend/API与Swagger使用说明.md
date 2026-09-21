# 业务后端 API 与 Swagger 使用说明

适用范围：`DivineBeastsWorkspace` 当前五个 Go 业务服务的 HTTP 接口与共享 OpenAPI 契约。本文说明可运行接口、Swagger 访问方式、调用边界和契约维护流程；它不把未注册路由或未来能力描述为已经部署。

## 1. 访问地址

执行本地一键启动后，统一入口为：

```text
http://127.0.0.1:28080/swagger/
```

该页面由本地 `GatewayService` 提供，并直接读取 Docker 只读挂载的 `Shared/Contracts`。浏览器能够访问 `unpkg.com` 时会加载 Swagger UI；无法加载公共静态资源时，页面仍列出所有 YAML 原始契约链接，可下载后导入本地 Swagger Editor、Postman 或其他 OpenAPI 3.1 兼容工具。

原始规格地址的公共前缀为：

```text
http://127.0.0.1:28080/swagger/specs/
```

例如公网 Gateway 规格：

```text
http://127.0.0.1:28080/swagger/specs/GamePlatform/OpenAPI/gateway.openapi.yaml
```

`/swagger/` 只在本地 Compose 显式设置 `DIVINEBEASTS_SWAGGER_CONTRACTS_ROOT=/contracts` 时启用。生产装配未设置该变量时，访问该路径应返回 `404`，避免将内部控制面说明暴露到公网。

## 2. Swagger 规格总表

| 服务或契约 | OpenAPI 真源 | 本地服务地址 | 接口边界 |
| --- | --- | --- | --- |
| GatewayService | `Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml` | `http://127.0.0.1:28080` | 公网接口；游戏客户端只能通过此入口访问已实现业务。 |
| IdentityService | `Shared/Contracts/GamePlatform/OpenAPI/identity.openapi.yaml` | `http://127.0.0.1:8081` | 内部接口；登录会话创建与 Access Token 验证。 |
| PlayerDataService | `Shared/Contracts/GamePlatform/OpenAPI/player-data.openapi.yaml` | `http://127.0.0.1:8082` | 内部接口；跨局长期玩家资料读取。 |
| MatchService（组队） | `Shared/Contracts/GamePlatform/OpenAPI/party.openapi.yaml` | `http://127.0.0.1:8083` | 内部接口；当前只实现创建赛前队伍。 |
| MatchService（匹配） | `Shared/Contracts/GamePlatform/OpenAPI/matchmaking.openapi.yaml` | `http://127.0.0.1:8083` | 内部接口；当前只实现创建匹配票据。 |
| GameServerControlService | `Shared/Contracts/GamePlatform/OpenAPI/game-server-control.openapi.yaml` | `http://127.0.0.1:8084` | 内部接口；Dedicated Server 生命周期、分配、迁移和权威比赛结果。 |
| DivineBeasts 目录契约 | `Shared/Contracts/Games/DivineBeasts/OpenAPI/catalog.openapi.yaml` | 无当前独立 HTTP 服务 | 项目只读目录契约，当前不是本地 Compose 已运行的第六服务。 |

每份运行服务规格均列出 `/health/live`、`/health/ready` 和 `/version`。这些运维端点用于探针和诊断，不是替代业务准入、数据库连通性、消息投递或专用服务器联调的验收证据。

## 3. 公网接口与认证

当前由 GatewayService 实际注册的公网业务路由只有以下四项：

| 方法 | 路径 | 认证 | 真实调用链 |
| --- | --- | --- | --- |
| `POST` | `/v1/auth/login` | 无 | GatewayService → IdentityService。 |
| `GET` | `/v1/player/profile` | `Bearer AccessToken` | GatewayService → IdentityService → PlayerDataService。 |
| `POST` | `/v1/party` | `Bearer AccessToken` | GatewayService → IdentityService → MatchService。 |
| `POST` | `/v1/matchmaking/tickets` | `Bearer AccessToken` | GatewayService → IdentityService → MatchService。 |

Gateway 在响应头写入 `X-Request-Id`、`X-Trace-Id`，并在配置了共享契约版本时写入 `X-Contract-Version`。调用方应记录这些值用于诊断，不应把它们当作授权凭据。

当前登录实现只接受 `provider=guest`。本地内存身份实现用于开发联调，不能作为生产账号、令牌轮换、撤销、审计或凭据安全方案。

## 4. 内部接口安全边界

`/internal/v1/**` 均为内部接口。以下调用要求不能被 Swagger 的“Try it out”功能绕过：

1. 客户端、浏览器和公网 Ingress 不得直连 Identity、PlayerData、Match 或 GameServerControl 服务的主机端口。
2. `playerId`、`gameServerId`、`matchId`、`roster`、迁移票据和比赛结果只能由已认证的后端或 Dedicated Server 产生；客户端提交的同名字段不能成为权威依据。
3. `GameServerControlService` 的注册、分配、迁移和 `match-result` 路由会改变控制面或权威状态。只能在隔离本地环境或已审批的目标环境使用，禁止对未知环境直接点击执行。
4. 当前本地 Compose 未部署 NATS、PostgreSQL、Redis、Agones 或真实 Dedicated Server。Swagger 显示的接口契约不表示生产依赖已满足。

## 5. 字段与兼容性说明

Gateway、Identity、PlayerData 与 Match 的 HTTP DTO 使用 lowerCamelCase JSON 字段。`GameServerControlService` 请求同样以 lowerCamelCase 文档化；其现有 Go 传输适配器按大小写不敏感规则解码。

服务器分配、迁移验证和比赛结果的部分响应直接编码 Go 内部 DTO，因此当前响应字段是 `AssignmentID`、`GameServerID`、`ResultID` 等 PascalCase。`game-server-control.openapi.yaml` 已按真实线协议标注。若要统一成 lowerCamelCase，必须先完成调用方盘点、兼容期和版本迁移，不能只改 Swagger 文档或单边改服务。

## 6. 契约维护与校验

1. 先修改 `Shared/Contracts` 下的 OpenAPI 真源，再同步适配器、调用方和中文说明；禁止手改 `Backend/generated` 或 `Shared/Generated` 生成物。
2. 从 `Backend/` 运行 `go run ./internal/tools/contractcodegen -workspace-root=..` 生成轻量契约目录，再运行相同命令的 `-check` 模式检查生成物是否过期。
3. 执行 `Tests/Contracts/ValidateBackendApiDocumentation.ps1`，它会从实际 `ServeMux` 注册路由验证每个运行接口均被对应 OpenAPI 覆盖。
4. Swagger 可读不等于服务可运行。变更后仍需执行 Go 测试、部署结构校验与本地一键启动验证；生产环境还需独立完成依赖、权限、迁移、观测和回退审批。

## 7. 当前未实现项

旧的契约草案中出现过刷新/退出/会话查询、组队邀请/加入/准备/队长转让、匹配票据查询/取消、玩家设置更新等路径。当前 Go HTTP 服务没有注册这些路由，因此它们不在本版运行 Swagger 规格中。新增任何此类接口前，必须先实现授权、校验、幂等、失败语义和回归测试，再扩展 OpenAPI。
