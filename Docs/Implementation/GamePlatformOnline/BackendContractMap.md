# GamePlatformOnline 后端契约映射

日期：2026-09-21。本文件先记录现场与本轮增量协议，后续按实际源码更新；协议约定不是实现或联调通过证据。

## 已核对的基线与兼容策略

已读取 `Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml`、身份/资料 OpenAPI、内部两份 Proto、Gateway 路由、HTTP/gRPC 适配、Identity/PlayerData 领域、生产装配及 PostgreSQL 迁移。现有登录路径 `/v1/auth/login` 仅支持 guest，凭据不参与真实校验；Refresh/Logout 没有网关路由；资料只有 GET，领域 UpdateDisplayName 尚无持久幂等。`/health/ready` 只表示宿主就绪，不能冒充业务依赖就绪。

保留现有路径、operationId 和响应字段。新增密码提供方 `password`，使用新增 `accountName` 和原有 `credential`（密码，禁止日志）；旧 guest 仅保留原本非生产开发装配与兼容测试，真实 Online 联调必须使用 PostgreSQL 密码认证装配，绝不从密码失败回退游客。内部沿用已有 gRPC，并同步既有 HTTP 适配，不创建另一套协议栈。生成物由锁定工具生成。

## 六项操作约定（由公共 gateway.openapi.yaml 作为协议真源）

| 操作 / operationId | HTTP | 必填输入与输出、单位 | 权限/成功/错误 | 幂等与归属 |
| --- | --- | --- | --- | --- |
| ProbeService / probeGatewayOnline | GET `/v1/online/probe` | 输出 `ready` boolean、`contractVersion` string、`service`=`gatewayservice` | 无认证；实际身份和资料依赖可用才200，否则503；兼容版本1.0.0 | 安全读取；Gateway 调下游 Probe，领域仓储真实探测，不写表 |
| Login / gatewayLogin（原身份保留） | POST `/v1/auth/login` | 原 `gameId,provider,credential,clientVersion`；password另需 `accountName`，deviceId可选。原响应 `playerId,sessionId,accessToken,refreshToken,expiresAt`，追加 `refreshExpiresAt`，时间UTC RFC3339 | 密码提供方成熟慢哈希校验；400/401/429/503；200成功 | 不自动重试；Identity 账号/会话所有者，凭据不记录 |
| RefreshAuthentication / refreshGatewayAuthentication | POST `/v1/auth/refresh` | `refreshToken`；输出与密码登录一致 | 原子轮换；401无效/重放/禁用，429限流，503不可用 | 不盲重试；同会话串行，消费摘要保留用于重放检测，Identity所有 |
| Logout / logoutGatewayAuthentication | POST `/v1/auth/logout` | `refreshToken`（当前或本会话已消费凭据，保密） | 有效会话凭据授权撤销；204无正文；401未知凭据，503不可达 | 同会话幂等撤销；旧访问/刷新均拒绝，Identity所有 |
| GetCurrentPlayerProfile / getGatewayPlayerProfile（原身份保留） | GET `/v1/player/profile` | 无客户端playerId；原资料字段、`revision`非负int64、`dataVersion`正数 | Bearer由Identity解析；200/401/403/404/503 | 安全读取；PlayerData独占player_profiles |
| UpdateCurrentPlayerProfile / updateGatewayPlayerProfile | PATCH `/v1/player/profile` | body仅 `displayName`（trim后1..24 Unicode字符）、`expectedRevision`（非负int64）；`Idempotency-Key`非空至128字符 | Bearer主体；200完整资料，400非法/越权字段，401/403/409修订或幂等冲突/503 | 主体+操作+规范内容绑定；同键同体先重放结果再查旧修订；资料+结果同一PG事务；PlayerData所有 |

## 处理链与持久化边界

- Gateway：`internal/app/gateway` 保留 NewAPI/IdentityPort/PlayerDataPort，增量独立能力接口，避免破坏既有测试替身；路由仅解析、鉴权、白名单和安全错误映射，不写数据库。
- Identity：`internal/modules/identity` 复用 Service，增量密码/原子仓储接口及 Probe、Refresh、Logout 用例。PostgreSQL 新迁移只含账号、哈希令牌会话和消费摘要；禁用账户在认证/刷新时复核。原 NewService 和 LoginGuest 留给旧开发/测试，生产采用明确的持久装配。
- PlayerData：`internal/modules/playerdata` 复用 GetProfile/UpdateDisplayName，新增幂等更新与幂等初始化用例。仅仓储事务可修改 player_profiles 和本领域幂等表；初始化由所属服务用例执行。
- 内部协议：`Backend/internal/contracts/proto/identity-service.proto` 增量 account_name 字段及 Refresh/Logout/Probe；`player-data-service.proto` 增量 UpdateProfile/EnsureProfile/Probe。不复用已发布字段号。
- PostgreSQL：沿用现有 Pool/pgx；迁移仅隔离开发库显式应用并登记版本，不修改旧迁移、不每次启动重放全库。
- 登录/刷新公网限流和请求体上限；内部服务默认回环或受控私网，不能直接向公网暴露。

## 验证状态

当前是实施锁定约定，尚无本轮通过证据。UE构建仍受用户明确保留的三份空历史插件描述阻断；不能据此修改它们或另造宿主。最终方法签名、实际处理器与证据由 InterfaceContract / Verification 记录。
