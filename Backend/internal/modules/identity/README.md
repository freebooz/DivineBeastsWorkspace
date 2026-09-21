# Identity 真实密码认证切片

本领域独占账户、密码验证和认证会话。玩家显示名/资料由 PlayerData 用例初始化；Identity 的仓储与迁移没有 `player_profiles` 写入。

## 接入与稳定 API

旧 `NewService(SessionRepository, Clock, TokenGenerator, accessTTL, refreshTTL)`、`LoginGuest` 保留给旧开发装配和测试，不能用于真实密码认证；旧内存/Redis刷新路径不具备本节的持久原子保证。

生产装配使用 `postgres.NewOnlineIdentityRepository(pool *postgres.Pool)`（`productiondeps` 标签）与：

```go
identity.NewPersistentService(repo identity.PersistentRepository, clock identity.Clock,
    accessTTL, refreshTTL time.Duration) (*identity.Service, error)

(*identity.Service).EnsureAccount(ctx context.Context, gameID, accountName, password string) (identity.Account, error)
(*identity.Service).Probe(ctx context.Context) error
(*identity.Service).LoginPassword(ctx context.Context, gameID, accountName, password, deviceID string) (identity.Session, error)
(*identity.Service).Authenticate(ctx context.Context, accessToken string) (identity.Session, error)
(*identity.Service).Refresh(ctx context.Context, refreshToken string) (identity.Session, error)
(*identity.Service).Logout(ctx context.Context, refreshToken string) error
```

构造要求 `0 < accessTTL <= refreshTTL`；不自动建库、探测或迁移。Probe 真实读取三张所需表，缺迁移、缺权限或不可达必须失败，空表不是失败。持久服务 `LoginGuest` 明确拒绝。

`EnsureAccount` 仅供受控工具，不是公开注册/重置口令接口；同游戏、账户、密码返回稳定 PlayerID，不同口令或禁用账户冲突，不改旧密码。Account 返回时清空 PasswordHash。账户名区分大小写，不归一化，游戏/账户键各1..128字节且无首尾空白；初始化密码8..72字节，不截断、不trim；可选deviceID最多256字节。

账户使用 bcrypt cost=12，依赖显式锁定 `golang.org/x/crypto v0.39.0`，与既有 pgx v5.7.6 最小版本选择一致，兼容Go1.23，不升级其他既有直接依赖。未知账户执行等成本占位哈希比较，返回与错误密码/禁用账户相同错误。公网限流与请求体大小由网关装配负责；本领域不冒充该防护。

## 令牌与事务边界

- Access/Refresh 各自使用 crypto/rand 的32字节随机数，base64url无填充编码；数据库只收到SHA256摘要。不存在令牌签名密钥、固定成功令牌或密码登录回落guest。
- 原始令牌只在Login/Refresh成功后返回现有 `Session`。`AccessExpiresAt` 和已有 `RefreshExpiresAt` 为UTC。Authenticate返回可信主体快照并清空原始令牌字段。
- 新迁移唯一位置：`Backend/migrations/000005_online_identity.sql`。三表为 `online_identity_accounts`（账户）、`online_identity_sessions`（会话）、`online_identity_refresh_credentials`（当前及消费历史摘要）。本切片不应用迁移。
- 刷新事务显式 `READ COMMITTED`，通过不可变历史摘要找会话，再对同一session行 `FOR UPDATE`、账户行 `FOR SHARE`。获得行锁后再次读取消费状态；消费旧摘要、更新会话、插入新摘要在一次事务提交。旧access立即被替换；刷新绝对截止不延长，新access期限不越过它。
- 对已消费refresh的重放先提交整个session撤销，再返回无效凭据。并发同refresh最多一次成功，失败方重放检测会撤销成功方的新凭据；客户端必须single-flight，刷新响应不确定时不能盲重试。
- Logout接受当前/历史refresh，保留摘要历史，重复同凭据退出幂等；已到期/禁用会话仍允许凭据撤销。未知凭据拒绝。不删除历史，不撤销其他session。
- Authenticate使用一条SQL同时读取当前access、账户禁用、会话撤销和期限，无会话缓存。**撤销提交后开始的新认证失败；提交前已经取到授权快照的业务请求不能被事后追回。** 不声称Identity撤销与PlayerData写入构成跨服务事务。
- Authenticate/Refresh将数据库连接池及行锁等待的单调耗时追加到领域时钟，防止以请求开始时间继续放行排队期间已过期的凭据。取消/提交失败不会被报告成成功；提交响应丢失仍可能结果不确定。
- 登录创建会话时在事务内重新检查账户并取共享锁。禁用更新与创建/轮换互斥；后续认证实时复核禁用状态。

驱动错误在仓储边界变为安全哨兵，保留取消/截止错误但不拼接SQL/参数。本实现没有日志调用，也没有启用pgx查询追踪；部署者仍须检查数据库服务端参数日志、代理与调用方日志配置，不得记录密码、原始令牌、密码哈希或完整认证报文。

## 测试与证据

纯领域测试仅把外部仓储换成 `_test.go` 中的替身，实际执行bcrypt、随机签发、摘要与领域判断；仓储事务边界测试故障注入真实仓储代码，但不证明数据库行锁。

在Backend目录运行：

```text
go test -mod=mod ./internal/modules/identity
go test -mod=mod -tags productiondeps -run TestOnlineIdentity -v ./internal/platform/database/postgres
go test -mod=mod -race -tags productiondeps ./internal/modules/identity ./internal/platform/database/postgres
go vet -mod=mod -tags productiondeps ./internal/modules/identity ./internal/platform/database/postgres
```

本机通过已存在Docker镜像 `sha256:60deed95d3888cc5e4d9ff8a10c54e5edc008c6ae3fba6187be6fb592e19e8c0` 运行Go1.23.12 linux/amd64；`--pull never --rm`。源码只读挂载，测试在容器临时副本解析依赖；全后端测试需同时复制Shared保持相对路径。证据位于工作空间 `Saved/Validation/GamePlatformOnline/Identity`，不放源码目录。

真实PG测试要求操作者先批准隔离库，并只在 `online_identity_test_` 前缀schema应用000005一次。DSN通过环境 `ONLINE_IDENTITY_TEST_DSN` 注入，连接的search_path必须指向该schema；另设 `ONLINE_IDENTITY_TEST_ISOLATED=1`。测试核对三张可见表都确实属于该schema，禁止回退public。测试不启动DB/服务、不应用SQL迁移；每个用例随机gameID且只清理自己的身份记录。

```text
go test -mod=mod -tags productiondeps -run '^TestOnlineIdentityPG' -count=1 -timeout=120s -v ./internal/platform/database/postgres
```

真实用例覆盖同refresh争抢/重放撤销、历史logout、session隔离、禁用/到期/游戏范围、logout-vs-refresh竞态。缺授权环境时显示SKIP；新Service重读只证明持久读取，**不等价于真实服务或数据库重启**，后者由主集成任务单独执行。当前没有真实PG或UE验收通过声明。

### 2026-09-21 已执行记录

- `01-red.log`：新增领域测试先因缺Account/StoredSession/生产构造API而编译失败（不是行为断言失败，未捏造测试数量）。`02-domain-green.log` / `04-repository-green.log`：领域6例通过，其中3例为原游客兼容回归。
- `03-repository-red.log`：事务测试先因缺真实仓储实现而编译失败。`04-repository-green.log`：首批4个身份事务边界测试通过。
- `07-review-red.log`：只读审查后先添加回归；显式隔离级别，以及Authenticate/Refresh存储等待跨期共3条断言真实失败。修复后 `08-review-green-race.log`：6个身份仓储顶层单测通过，其中等待跨期含2子例；4个真实PG顶层用例SKIP。Identity/PG两包vet和race退出0。
- `05-full-validation.log`：首次容器副本遗漏Shared，引发契约相对路径缺失；保留失败证据，随后补齐副本布局。未改变源码测试路径规避问题。
- `06-full-validation.log`：完整布局后，默认全仓回归仍遇并行生成物过期及旧生产装配词串断言。`09-production-build-vet.log` 的实际命令为 `go test -mod=mod -tags productiondeps,grpcdeps ./...`（文件名不代表该日志运行过vet），最终仅 `TestProductionCompositionWiresInfrastructure` 仍因旧 `postgres.NewPlayerRepository` 断言失败，已交给装配所有者。各生产模块实际编译成功，不将全仓失败隐去。
- `10-vet.log`：`go vet -mod=mod -tags productiondeps,grpcdeps ./...` 退出0。

以上都是相应执行时的共享工作树快照，其他任务可能继续修改。真实数据库、服务重启、UE联调、三目标构建和烘焙不在这些通过结果之内。

## 兼容与后续边界

不新增第六个常驻服务，不修改网关、传输、装配或玩家资料。账号初始化调用所属领域方法，测试工具不得直接伪造可认证记录。原Session字段和方法签名保留；本切片没有Flow修改。

M0保留刷新消费历史，不做未经批准的清理；长期清理/账户禁用管理/密码重置策略需后续独立契约，不能删除历史后仍声称保留无限期Logout幂等。
