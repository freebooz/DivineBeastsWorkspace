# Backend 总体目录规划说明（中文）V1.1.0

> 适用范围：`DivineBeastsWorkspace/Backend`，Go 业务后端单 Go Module 及其五个可部署服务。
> 来源：`DivineBeastsWorkspace_Backend_Shared_DirectoryTree_CN_V1.1.0.md`。
> 本文是对应目录的总体目录规划说明，不替代源代码、协议文件或生成工具的实际行为。
> 维护要求：后续在本目录下新增、删除、重命名文件或目录时，必须在同一变更中同步更新本文的目录树、中文职责说明和版本记录；不得只改目录而不改本文档。

## 目录结构与职责

### Session部分内核增量（2026-09-21）

`internal/platform/database/postgresadmission/store.go`为数据库边界适配，`store_integration_test.go`为显式`sessionintegration`标签下的隔离真实PostgreSQL测试。唯一SQL真源为`migrations/000003_session_admission.sql`，包含授权快照、实例、预留、绑定及原子函数。没有新增第六服务或对外路由；尚未接入真实Online/UE服务器链，不构成生产准入完成。验证入口在工作空间`Tests/Integration/Session/TestSessionBackend.ps1`，只创建和清理自己的临时数据库容器。并行Online迁移由其原任务维护，不在本增量重写。

```text
Backend/                                                            # Go业务后端根目录；保持单Go Module并承载五个可部署服务
├── cmd/                                                            # 五个薄应用入口；仅负责配置、信号和Composition（装配）启动，不写领域规则
│   ├── gameservercontrolservice/                                   # GameServerControlService（游戏服务器控制服务）应用入口
│   │   └── main.go                                                 # GameServerControlService主入口；加载配置并启动对应Composition
│   ├── gatewayservice/                                             # GatewayService（统一接入服务）应用入口
│   │   └── main.go                                                 # GatewayService主入口；加载配置并启动HTTP/gRPC统一接入
│   ├── identityservice/                                            # IdentityService（身份认证服务）应用入口
│   │   └── main.go                                                 # IdentityService主入口；加载配置并启动身份认证服务
│   ├── matchservice/                                               # MatchService（组队、匹配与比赛编排服务）应用入口
│   │   └── main.go                                                 # MatchService主入口；加载配置并启动组队/匹配/比赛服务
│   └── playerdataservice/                                          # PlayerDataService（玩家长期数据服务）应用入口
│       └── main.go                                                 # PlayerDataService主入口；加载配置并启动玩家数据服务
├── configs/                                                        # 后端可提交的非敏感配置目录
│   ├── defaults.yaml                                               # 后端通用默认配置，包括端口、超时及基础运行参数
│   ├── divinebeasts.yaml                                           # 《神兽联盟》项目配置，包括ServerRole、Experience、ArenaMode及功能开关
│   └── production.env.example                                      # 生产环境变量模板；只保存键名/示例，不保存真实Secret
├── generated/                                                      # Go协议与目录生成代码；由Codegen生成，禁止手工修改
│   ├── divinebeasts/                                               # 《神兽联盟》项目专属Shared契约与目录生成代码
│   │   ├── catalog_generated.go                                    # Codegen生成的《神兽联盟》ServerRole/Experience/ArenaMode目录代码
│   │   └── contracts_generated.go                                  # Codegen生成的协议注册信息与Contract元数据
│   ├── gameplatform/                                               # GamePlatform（游戏平台）公共Shared契约生成代码
│   │   └── contracts_generated.go                                  # Codegen生成的协议注册信息与Contract元数据
│   ├── openapi/                                                    # 正式OpenAPI生成代码落点与生成说明
│   │   └── README.generated.md                                     # 生成目录说明；描述正式生成落点与禁止手改规则
│   └── proto/                                                      # 正式Proto/gRPC生成代码落点与生成说明
│       └── README.generated.md                                     # 生成目录说明；描述正式生成落点与禁止手改规则
├── internal/                                                       # Go后端私有实现根目录；受Go internal规则保护
│   ├── app/                                                        # Application（应用层）用例、跨领域编排和服务装配
│   │   ├── composition/                                            # Composition Root（装配根）；连接领域、Transport与生产基础设施
│   │   │   ├── composition_local.go                                # 本地/测试Composition装配；使用真实HTTP链路和本地适配器
│   │   │   ├── composition_production_grpc.go                      # 生产gRPC Composition装配；接入gRPC Transport及生产基础设施
│   │   │   └── composition_production_http.go                      # 生产HTTP Composition装配；接入HTTP Transport及生产基础设施
│   │   ├── gameservercontrol/                                      # 服务器注册、心跳、世界/竞技分配、Assignment、跨服和比赛结果应用服务
│   │   │   ├── service.go                                          # GameServerControl应用服务实现：注册、心跳、Ready/Drain、世界/竞技分配、Assignment、Transfer与MatchResult
│   │   │   └── service_test.go                                     # gameservercontrol模块核心服务单元测试与边界验证
│   │   ├── gateway/                                                # Gateway统一接入API应用层
│   │   │   ├── api.go                                              # Gateway公共HTTP API实现；负责路由、身份上下文和应用端口调用
│   │   │   └── api_test.go                                         # Gateway HTTP API自动化测试
│   │   ├── integrationadapter/                                     # 跨应用集成适配器；连接MatchService与GameServerControl等边界
│   │   │   ├── arena_control.go                                    # MatchService到GameServerControl的竞技服务器控制适配器
│   │   │   └── arena_control_test.go                               # 竞技服务器控制集成适配器测试
│   │   ├── matchapi/                                               # MatchService对外业务API用例实现
│   │   │   ├── service.go                                          # MatchService业务API实现：Party与Matchmaking请求用例
│   │   │   └── service_test.go                                     # matchapi模块核心服务单元测试与边界验证
│   │   ├── matchservice/                                           # 比赛创建、Roster组织和MainArena分配应用编排器
│   │   │   ├── orchestrator.go                                     # 比赛编排器；负责Roster、MainArena分配和TransferTicket生成
│   │   │   └── orchestrator_test.go                                # 比赛编排器测试；覆盖1v1～5v5、Party原子性和服务器分配
│   │   └── servicehost/                                            # 通用ServiceHost（服务宿主）基础能力与生命周期管理
│   │       ├── handler.go                                          # ServiceHost公共HTTP处理器及健康/版本端点基础实现
│   │       ├── handler_test.go                                     # ServiceHost公共处理器测试
│   │       └── server.go                                           # 通用HTTP服务启动、优雅关闭和生命周期管理
│   ├── contracts/                                                  # Go后端内部稳定DTO与服务间协议边界
│   │   ├── gameserver/                                             # 游戏服务器、世界分配和Assignment内部DTO
│   │   │   └── types.go                                            # GameServer/Assignment/Experience/ServerRole内部稳定DTO及转换边界
│   │   ├── matchresult/                                            # 权威MatchResult（比赛结果）内部DTO
│   │   │   └── types.go                                            # MatchResult内部稳定DTO；隔离Shared生成类型与领域模型
│   │   ├── proto/                                                  # 仅Go服务内部使用的Proto/gRPC协议真源；UE不得依赖
│   │   │   ├── game-server-control-internal.proto                  # MatchService等Go内部服务调用GameServerControlService的内部gRPC协议
│   │   │   ├── identity-service.proto                              # Gateway→IdentityService身份认证内部gRPC协议
│   │   │   ├── match-service.proto                                 # Gateway→MatchService组队/匹配内部gRPC协议
│   │   │   ├── player-data-service.proto                           # Gateway→PlayerDataService玩家数据内部gRPC协议
│   │   │   └── README.md                                           # 当前目录职责、使用方法和工程约束说明
│   │   ├── transfer/                                               # TransferTicket（跨服迁移票据）内部DTO和契约测试
│   │   │   ├── proto_contract_test.go                              # Transfer相关Proto字段和语义契约测试
│   │   │   └── types.go                                            # TransferTicket内部稳定DTO及签发/验证边界类型
│   │   └── contracts_test.go                                       # 内部DTO与Shared契约字段/常量一致性测试
│   ├── modules/                                                    # Domain（领域层）业务模块；禁止直接依赖具体数据库/缓存/消息实现
│   │   ├── arena/                                                  # Arena（竞技长期业务）预留领域模块
│   │   │   └── doc.go                                              # arena领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── commerce/                                               # Commerce（商品/订单/支付/退款）预留领域模块
│   │   │   └── doc.go                                              # commerce领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── economy/                                                # Economy（经济/货币/账本）预留领域模块
│   │   │   └── doc.go                                              # economy领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── entitlement/                                            # Entitlement（永久权益/解锁）预留领域模块
│   │   │   └── doc.go                                              # entitlement领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── gameserver/                                             # GameServer注册、状态、容量、分配和生命周期领域
│   │   │   ├── allocator.go                                        # 服务器分配端口/实现；按角色、体验、区域及容量选择目标实例
│   │   │   ├── gameserver.go                                       # GameServer聚合、状态机、心跳、Ready/Drain和Assignment领域逻辑
│   │   │   └── gameserver_test.go                                  # GameServer领域自动化测试
│   │   ├── guild/                                                  # Guild（公会/战队）预留领域模块
│   │   │   └── doc.go                                              # guild领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── identity/                                               # Identity（身份与会话）领域实现
│   │   │   ├── service.go                                          # Identity领域服务：登录、Token、Session及认证状态
│   │   │   └── service_test.go                                     # identity模块核心服务单元测试与边界验证
│   │   ├── inventory/                                              # Inventory（长期背包）预留领域模块
│   │   │   └── doc.go                                              # inventory领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── liveops/                                                # LiveOps（赛季/活动/运营）预留领域模块
│   │   │   └── doc.go                                              # liveops领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── mail/                                                   # Mail（游戏邮件）预留领域模块
│   │   │   └── doc.go                                              # mail领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── match/                                                  # Match（比赛）权威结果与领域事件处理
│   │   │   ├── result.go                                           # 权威MatchResult处理、幂等保存和Outbox事件创建
│   │   │   └── result_test.go                                      # 比赛结果与Outbox事务语义测试
│   │   ├── matchmaking/                                            # Matchmaking（匹配）票据、队列、规则及性能基准
│   │   │   ├── benchmark_test.go                                   # matchmaking领域关键路径Benchmark（性能基准）测试
│   │   │   ├── matchmaking.go                                      # MatchmakingTicket、队列状态和匹配约束领域实现
│   │   │   └── matchmaking_test.go                                 # 匹配领域自动化测试
│   │   ├── notification/                                           # Notification（通知）预留领域模块
│   │   │   └── doc.go                                              # notification领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── party/                                                  # Party（组队）聚合、成员、Ready和Roster Lock领域
│   │   │   ├── party.go                                            # Party聚合、Leader、Member、Ready与Roster Lock领域实现
│   │   │   └── party_test.go                                       # 组队领域自动化测试
│   │   ├── playerdata/                                             # PlayerData（玩家长期数据）领域实现
│   │   │   ├── service.go                                          # PlayerData领域服务：玩家长期Profile读取和更新
│   │   │   └── service_test.go                                     # playerdata模块核心服务单元测试与边界验证
│   │   ├── progression/                                            # Progression（长期成长）预留领域模块
│   │   │   └── doc.go                                              # progression领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── rating/                                                 # Rating（MMR/Rank竞技评分）预留领域模块
│   │   │   └── doc.go                                              # rating领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── servertransfer/                                         # ServerTransfer（跨服迁移）签名、验证、消费与防重放领域
│   │   │   ├── benchmark_test.go                                   # servertransfer领域关键路径Benchmark（性能基准）测试
│   │   │   ├── service.go                                          # ServerTransfer领域服务：票据签发、签名验证、Redis防重放消费
│   │   │   └── service_test.go                                     # servertransfer模块核心服务单元测试与边界验证
│   │   ├── social/                                                 # Social（好友/社交关系）预留领域模块
│   │   │   └── doc.go                                              # social领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   ├── telemetry/                                              # Telemetry（遥测）预留领域模块
│   │   │   └── doc.go                                              # telemetry领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   │   └── worldcontrol/                                           # WorldControl（世界/区域/分片控制）预留领域模块
│   │       └── doc.go                                              # worldcontrol领域包说明与预留边界；当前仅建立模块归属，不表示完整功能已实现
│   ├── platform/                                                   # 基础设施适配层；将领域Port连接到数据库、缓存、消息和Agones
│   │   ├── agones/                                                 # Agones（游戏服务器编排）MainArena生产分配适配器
│   │   │   ├── allocator.go                                        # 服务器分配端口/实现；按角色、体验、区域及容量选择目标实例
│   │   │   └── allocator_test.go                                   # 对应实现的Go自动化测试；用于功能、边界或回归验证
│   │   ├── apperror/                                               # 统一Application Error（应用错误）和错误码映射
│   │   │   ├── error.go                                            # 统一Application Error（应用错误）类型和错误码映射
│   │   │   └── error_test.go                                       # 应用错误映射测试
│   │   ├── cache/                                                  # 缓存适配根目录
│   │   │   └── redisstore/                                         # Redis生产适配；承载Session、幂等、状态缓存和TransferTicket防重放
│   │   │       ├── adapter_production.go                           # Redis生产连接适配器，实现连接、Ping、关闭及基础访问封装
│   │   │       └── repositories_production.go                      # Redis生产Repository实现：Session、幂等键、Party/匹配状态及Transfer防重放
│   │   ├── config/                                                 # 配置模型、YAML/环境变量加载和生产校验
│   │   │   ├── config.go                                           # 配置模型、YAML/环境变量加载和生产配置校验
│   │   │   └── config_test.go                                      # 配置加载与校验测试
│   │   ├── database/                                               # 数据库适配根目录
│   │   │   └── postgres/                                           # PostgreSQL生产适配；承载玩家档案、比赛结果和事务Outbox
│   │   │       ├── queries/                                        # PostgreSQL查询SQL唯一维护目录
│   │   │       │   ├── match_results.sql                           # MatchResult权威比赛结果持久化SQL
│   │   │       │   ├── outbox.sql                                  # Outbox入队、领取、租约、成功确认及失败重试SQL
│   │   │       │   └── player_profiles.sql                         # 玩家长期Profile查询、创建与更新SQL
│   │   │       ├── adapter_production.go                           # PostgreSQL生产连接池适配器，实现连接、Ping、事务和生命周期管理
│   │   │       └── repositories_production.go                      # PostgreSQL生产Repository实现：PlayerProfile、MatchResult及Outbox事务写入
│   │   ├── health/                                                 # 健康检查与依赖探测公共能力
│   │   │   ├── health.go                                           # 健康状态模型与依赖探测接口
│   │   │   └── health_test.go                                      # 健康检查逻辑测试
│   │   ├── ids/                                                    # 业务ID生成与格式校验公共能力
│   │   │   ├── id.go                                               # 统一业务ID生成与合法性验证
│   │   │   └── id_test.go                                          # ID工具测试
│   │   ├── messaging/                                              # 消息系统适配根目录
│   │   │   └── natsjs/                                             # NATS JetStream生产适配及Outbox事件发布器
│   │   │       ├── adapter_production.go                           # NATS JetStream生产连接适配器，实现消息流连接和生命周期管理
│   │   │       └── outbox_publisher_production.go                  # NATS JetStream Outbox事件发布器
│   │   └── outbox/                                                 # Transactional Outbox（事务发件箱）模型、Dispatcher、租约和重试
│   │       ├── outbox.go                                           # Transactional Outbox记录、租约锁、Dispatcher和发布状态流转实现
│   │       └── outbox_test.go                                      # Outbox状态机、并发租约和重试测试
│   ├── tools/                                                      # 后端内部开发与生成工具；仅开发/CI使用
│   │   └── contractcodegen/                                        # Shared Contract Codegen（共享契约代码生成）命令实现
│   │       └── main.go                                             # Contract Codegen命令入口；生成Go/C++绑定并执行新鲜度/工具版本检查
│   └── transport/                                                  # Transport（传输层）适配；负责HTTP/gRPC与应用层映射
│       ├── grpcadapter/                                            # gRPC服务端适配器；生产grpcdeps构建标签启用
│       │   ├── gameserver_internal_server_grpcdeps.go              # GameServerControl内部gRPC服务端适配器；grpcdeps标签启用
│       │   ├── gameserver_shared_server_grpcdeps.go                # UE Dedicated Server使用的Shared GameServerControl gRPC服务端适配器
│       │   ├── identity_server_grpcdeps.go                         # IdentityService内部gRPC服务端适配器
│       │   ├── match_server_grpcdeps.go                            # MatchService内部gRPC服务端适配器
│       │   ├── matchresult_server_grpcdeps.go                      # 权威MatchResult Shared gRPC服务端适配器
│       │   ├── playerdata_server_grpcdeps.go                       # PlayerDataService内部gRPC服务端适配器
│       │   └── transfer_server_grpcdeps.go                         # ServerTransfer Shared gRPC服务端适配器
│       ├── grpcclient/                                             # gRPC内部服务客户端；生产grpcdeps构建标签启用
│       │   ├── arena_control_grpcdeps.go                           # MatchService调用GameServerControlService的生产gRPC客户端
│       │   └── gateway_clients_grpcdeps.go                         # Gateway调用Identity/PlayerData/Match服务的生产gRPC客户端集合
│       └── httpadapter/                                            # HTTP/JSON服务端及内部客户端；支持本地真实进程联调
│           ├── common.go                                           # HTTP Transport公共JSON编解码、错误映射和请求辅助
│           ├── gameservercontrol_server.go                         # GameServerControlService HTTP服务端；提供注册、心跳、分配、Assignment、Transfer和MatchResult接口
│           ├── gateway_clients.go                                  # Gateway调用Identity/PlayerData/Match内部HTTP服务的真实客户端
│           ├── identity_server.go                                  # IdentityService HTTP服务端适配器
│           ├── match_server.go                                     # MatchService HTTP服务端适配器
│           ├── playerdata_server.go                                # PlayerDataService HTTP服务端适配器
│           └── transport_integration_test.go                       # HTTP Transport真实编解码和跨服务调用集成测试
├── migrations/                                                     # PostgreSQL数据库Migration（迁移）唯一来源
│   ├── 000001_core.sql                                             # 核心业务数据库初始Migration
│   ├── 000002_outbox.sql                                           # Transactional Outbox表、索引及租约字段Migration
│   └── README.md                                                   # 当前目录职责、使用方法和工程约束说明
├── pkg/                                                            # 真正跨模块复用且API稳定的公共Go包；当前保持极小
│   └── README.md                                                   # 当前目录职责、使用方法和工程约束说明
├── tests/                                                          # 后端架构、Codegen、生产装配和协议兼容测试
│   ├── architecture_test.go                                        # 目录边界和依赖方向架构门禁测试
│   ├── codegen_test.go                                             # Shared→Go/C++生成物新鲜度、确定性和工具锁测试
│   ├── contract_compatibility_test.go                              # Client/Server/Backend Contract Compatibility（协议兼容）测试
│   ├── production_composition_test.go                              # 生产Composition必须接入PostgreSQL、Redis、NATS、Agones等适配器的门禁测试
│   └── README.md                                                   # 当前目录职责、使用方法和工程约束说明
├── go.mod                                                          # Go Module定义、Go语言版本及生产依赖声明
└── go.sum                                                          # Go依赖内容完整性校验；生产依赖下载后由Go工具维护
```

## 当前接口文档实现补充（2026-09-21）

本次新增的 Gateway Swagger 入口、接口契约校验与本地部署绑定如下；本节以当前真实文件为准，用于补充上方目录树中较早的接口范围描述。

| 路径 | 中文职责 |
| --- | --- |
| `internal/app/gateway/swagger.go` | 仅在显式配置共享契约根目录时扫描 OpenAPI 真源、提供本地 Swagger UI 页面和原始规格文件。 |
| `internal/app/gateway/swagger_test.go` | 验证文档入口在本地配置下可访问，未配置时保持 `404`。 |
| `generated/gameplatform/contracts_generated.go` | 由 `contractcodegen` 根据全部 GamePlatform OpenAPI 路由重新生成；禁止人工修改。 |

## 维护规则

1. 新增或删除 Backend 文件、目录时，必须同步更新本文对应树节点及中文职责说明。
2. 重命名或移动 Backend 文件、目录时，必须在本文中同步调整路径层级和职责说明。
3. 对 `[预留]` 或生成目录的状态变化，必须同步更新本文中的边界标识；生成目录仍不得手工修改生成产物。
4. 本文版本发生结构性变化时，更新文件名版本或在变更记录中记录版本变更原因。
