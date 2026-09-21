# Online玩家资料切片：接口、所有权与验证

日期：2026-09-21。范围仅玩家资料领域、新增同package PG适配及000006迁移；不修改Session、Identity、传输、网关、生成协议或既有`repositories_production.go`。

## 精确对接

保留`playerdata.NewService(repo Repository) *Service`及旧Repository的Get/Save签名。现有Service新增：

```go
UpdateDisplayNameIdempotent(ctx context.Context, playerID, displayName string, expectedRevision int64, key string) (Profile, error)
EnsureProfile(ctx context.Context, playerID, gameID string) error
Probe(ctx context.Context) error
```

生产装配使用：

```go
repo := postgres.NewOnlinePlayerRepository(pool) // 返回*postgres.OnlinePlayerRepository
service := playerdata.NewService(repo)
```

构造器签名为`NewOnlinePlayerRepository(pool *postgres.Pool) *postgres.OnlinePlayerRepository`（声明处同package写`*Pool`）。只绑定已有Pool，不建库、不执行迁移。适配器嵌入既有PlayerRepository，保留Save；Online覆盖Get和EnsureProfile取得分类错误及游戏归属校验，幂等写/Probe是既有PlayerRepository的新增方法。

旧Repository接口不扩张；三个增量端口分别为IdempotentDisplayNameRepository、ProfileInitializer、RepositoryProbe。缺少增量能力返回SERVICE_UNAVAILABLE，不回落到内存结果或Get+Save伪原子实现。MemoryRepository只保留既有开发/测试用途，不新增生产幂等替身。

GetProfile不自动创建。EnsureProfile必须由玩家资料服务的显式、可信初始化调用链调用；网关/身份服务通过服务端口接入，不直接写表。playerID由认证上下文取得，领域与仓储本身不实现Bearer认证。本切片没有修改上游处理器或完成端到端鉴权验收。

## 事务与错误

- 可编辑字段只有trim后的displayName，1..24个Unicode字符；无效UTF-8/NUL、空主体、负revision、空/超过128字符的幂等键拒绝。键与主体保留原值，不trim后合并不同身份。
- 幂等主键为player_id、固定操作UpdateCurrentPlayerProfile、idempotency_key；规范请求同时保存名称和expectedRevision。
- READ COMMITTED事务内先取得主体/操作/键的事务级advisory锁，再读持久结果。首次请求尚无记录时，同键竞争仍被串行化；哈希碰撞只增加等待，完整主键仍隔离数据。
- 同键同规范内容直接返回首次完整资料快照，优先于revision检查；同键异内容返回IDEMPOTENCY_CONFLICT；新键旧revision返回PLAYER_DATA_CONFLICT。其他更新之后重放旧键仍返回旧操作结果，不用当前资料伪造旧结果。
- UPDATE仅修改display_name、revision和updated_at；不写教学、世界、结构版本或角色权益。更新与幂等快照INSERT同一事务，任一步失败回滚。未找到资料返回PLAYER_PROFILE_NOT_FOUND。
- Ensure同玩家同游戏不重置资料；绑定不同游戏返回PLAYER_DATA_CONFLICT。
- Probe真实查询两张表结构/读权限和连接可用性，不是固定健康值；不是完整写权限、迁移内容或生产性能审计。
- 业务错误使用apperror.Error；未分类基础设施错误脱敏为SERVICE_UNAVAILABLE。提交响应失败表示结果未确认，不声称回滚；幂等更新须沿用原键与原参数恢复。提交前取消/超时保留context错误，不意味着客户端取消能撤销已经提交的远端事务。

唯一新增迁移：`Backend/migrations/000006_online_profile_idempotency.sql`。此前请求的000004未创建；000003/000005不属于本切片。幂等结果无自动TTL，不悄悄缩短重放保证。保留/归档策略须后续明确业务重试期限，不能直接删除仍可能重试的结果。

## 测试先行记录

以下为本次实际工具输出摘要，不是PG实测证明。早期red使用本机已有golang:1.23镜像；收到精确工具链约束后，后续验证全部使用同一已核实镜像ID：

`sha256:60deed95d3888cc5e4d9ff8a10c54e5edc008c6ae3fba6187be6fb592e19e8c0`

实际`go version`为`go1.23.12 linux/amd64`，不是仓库go指令1.23.0的精确补丁版。

| 阶段 | 实际行为与退出码 |
| --- | --- |
| Service red | 4个新增顶层用例失败：现有Service缺少三个Online精确签名；2个旧用例通过。退出1，非语法/编译错误。 |
| Service green | 实现增量端口和验证后，上述4个新增用例及2个旧用例通过，退出0。 |
| PG能力 red | TestOnlinePlayerRepositoryRejectsBeforeDatabase失败：生产仓储缺少原子幂等更新能力，退出1。没有连接PG。 |
| PG离线 green | 新增仓储能力后，该输入/取消/缺失连接分类测试通过，退出0。不证明事务或持久化。 |
| 读取分类 red | TestOnlineReadClassifiesWithoutCreating失败：期望INVALID_REQUEST，实际普通“玩家资料不存在”，退出1。 |
| 读取分类 green | 现有GetProfile增量分类和取消处理后，5个新增领域顶层用例与2个旧用例通过。 |

测试替身仅用于外部持久端口记录和故障输入；真实PG测试调用生产Service和仓储，无内存幂等模拟。

## 已执行验证及未执行项

Docker上下文desktop-linux、`--pull never`、`--rm`。测试挂载当前工作空间到`/workspace:ro`，复制Backend到容器临时目录，下载现有go.mod依赖；`-mod=mod`的解析只写临时副本，不写共享工作树go.mod/go.sum。全量测试另外链接只读Shared目录以检查契约。格式化仅对本切片明确列出的Go文件使用gofmt。

执行的Go命令（工作目录均为容器临时Backend副本）：

```text
go test -mod=mod -tags=productiondeps -count=1 -v ./internal/platform/database/postgres ./internal/modules/playerdata
go test -mod=mod -race -tags=productiondeps -count=1 ./internal/modules/playerdata ./internal/platform/database/postgres
go vet -mod=mod -tags=productiondeps ./internal/modules/playerdata ./internal/platform/database/postgres
go test -mod=mod -count=1 ./...
go vet -mod=mod ./...
```

- 限定包离线测试、竞态检查、限定包vet：退出0。PG真实集成项缺配置而SKIP，不能计入PG成功。
- 全量默认测试：本次快照退出1；`Backend/tests/TestGeneratedContractsAreCurrent`指出`Backend/generated/gameplatform/contracts_generated.go`和`Shared/Generated/Cpp/GamePlatform/ContractRegistry.generated.hpp`过期。其他输出中的业务包通过。契约正在其他任务并发实施，本切片不手改或生成这些文件。
- 全量默认vet：退出0。
- 本切片git diff --check：退出0。
- 未执行数据库迁移、真实PG连接、PG并发/回滚/重开连接验证、真实后端进程重启、UE或端到端联调；没有启动数据库或其他后端容器。

## 真实PG测试的后续执行条件

须由拥有数据库权限的任务预先准备隔离schema及核心表和000006迁移，然后只向测试进程注入：

- ONLINE_PLAYER_TEST_DSN：含该测试schema的search_path，不出现在命令行或日志。
- ONLINE_PLAYER_TEST_ALLOW_WRITES=1：显式批准测试夹具写入。

schema必须以`online_player_test_`开头，且两张可见表实际属于该schema，禁止search_path回退public。测试不会建schema、执行迁移或清库；只创建随机测试主体，并精确清理该主体的资料/幂等行。

```text
go test -tags=productiondeps -count=1 -v -run '^TestOnlinePlayerPG' ./internal/platform/database/postgres
```

5个真实PG测试入口覆盖：持久快照重放与白名单字段、三种并发竞争、主体隔离、UPDATE后结果INSERT等待期间取消的事务回滚、同键锁等待超时后重试。重开独立连接池排除仓储内存缓存，但不是后端服务进程重启证明。若无DSN，所有真实入口明确SKIP。

## 交给集成任务同步的文件索引

本任务未修改独占范围之外的总体目录、Backend细化目录或全局Online文档；由父任务将本清单同步到这些正在并发维护的索引：

- 修改：`Backend/internal/modules/playerdata/service.go`。
- 新增：该目录的`online.go`、`online_test.go`、`OnlineVerification.md`。
- 新增：`Backend/internal/platform/database/postgres/online_player_repository.go`、`online_player_test.go`、`online_player_integration_test.go`。
- 新增：`Backend/migrations/000006_online_profile_idempotency.sql`。

未修改旧repositories_production.go；不提交、不回退其他任务修改、不接触任何历史空插件描述。
