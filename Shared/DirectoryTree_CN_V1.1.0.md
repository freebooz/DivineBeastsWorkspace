# Shared 总体目录规划说明（中文）V1.1.0

> 适用范围：`DivineBeastsWorkspace/Shared`，UE 与 Go 之间的跨语言共享契约层。
> 来源：`DivineBeastsWorkspace_Backend_Shared_DirectoryTree_CN_V1.1.0.md`。
> 本文是对应目录的总体目录规划说明，不替代源代码、协议文件或生成工具的实际行为。
> 维护要求：后续在本目录下新增、删除、重命名文件或目录时，必须在同一变更中同步更新本文的目录树、中文职责说明和版本记录；不得只改目录而不改本文档。

## 目录结构与职责

```text
Shared/                                                             # UE与Go跨语言共享契约层；禁止保存UE Gameplay或Go业务实现
├── Contracts/                                                      # 跨语言Contract（契约）唯一真源
│   ├── GamePlatform/                                               # GamePlatform（游戏平台）多游戏公共协议
│   │   ├── Events/                                                 # 真正跨UE与Backend边界的公共集成事件
│   │   │   ├── Common/                                             # 公共消息Envelope（信封）和事件元数据
│   │   │   │   └── message-envelope.schema.json                    # 跨语言集成事件统一Envelope（信封）结构
│   │   │   ├── GameServer/                                         # GameServer生命周期/异常公共事件
│   │   │   │   └── game-server-unhealthy.schema.json               # GameServer异常/不健康跨边界事件结构
│   │   │   └── Match/                                              # 比赛生命周期公共事件
│   │   │       └── match-completed.schema.json                     # 通用Match.Completed比赛完成事件结构
│   │   ├── OpenAPI/                                                # 客户端与Gateway之间的公共HTTP API契约
│   │   │   ├── gateway.openapi.yaml                                # Gateway公共HTTP聚合契约及健康/版本接口
│   │   │   ├── identity.openapi.yaml                               # 登录、刷新、退出和Session查询HTTP契约
│   │   │   ├── matchmaking.openapi.yaml                            # 创建、查询、取消MatchmakingTicket的HTTP契约
│   │   │   ├── party.openapi.yaml                                  # Party创建、邀请、加入、离开、Ready和Leader变更HTTP契约
│   │   │   └── player-data.openapi.yaml                            # 玩家Profile、Characters、Progress等长期数据HTTP契约
│   │   ├── Proto/                                                  # UE Dedicated Server与Go Backend之间的公共Proto/gRPC协议
│   │   │   ├── common.proto                                        # UE/Backend公共基础消息、ID、时间、Endpoint和错误结构
│   │   │   ├── game-server-control.proto                           # Dedicated Server注册、心跳、Ready、Drain、Assignment等公共RPC协议
│   │   │   ├── match-result.proto                                  # MainArena Dedicated Server提交权威MatchResult的公共RPC协议
│   │   │   └── server-transfer.proto                               # 跨服TransferTicket签发、验证与消费公共RPC协议
│   │   └── Schemas/                                                # 跨语言公共JSON Schema数据结构约束
│   │       ├── Common/                                             # 跨协议公共基础结构
│   │       │   ├── api-error.schema.json                           # 统一API错误返回结构
│   │       │   └── endpoint.schema.json                            # 网络Endpoint地址/端口公共结构
│   │       ├── Match/                                              # 比赛结果公共结构
│   │       │   └── match-result.schema.json                        # 权威比赛结果跨语言结构
│   │       ├── Matchmaking/                                        # 匹配票据公共结构
│   │       │   └── matchmaking-ticket.schema.json                  # MatchmakingTicket匹配票据跨语言结构
│   │       ├── PlayerData/                                         # 玩家长期档案公共结构
│   │       │   └── player-profile.schema.json                      # 玩家长期Profile跨语言结构
│   │       ├── ServerTransfer/                                     # 跨服迁移票据公共结构
│   │       │   └── transfer-ticket.schema.json                     # TransferTicket跨服迁移票据跨语言结构
│   │       └── game-server-assignment.schema.json                  # 统一世界/竞技GameServer Assignment任务分配结构
│   └── Games/                                                      # 具体游戏跨语言协议扩展根目录
│       └── DivineBeasts/                                           # 《神兽联盟》项目专属跨语言契约
│           ├── Events/                                             # 《神兽联盟》项目专属跨边界事件
│           │   └── arena-match-completed.schema.json               # 《神兽联盟》Arena比赛完成项目事件扩展
│           ├── OpenAPI/                                            # 《神兽联盟》项目专属HTTP接口契约
│           │   └── catalog.openapi.yaml                            # 《神兽联盟》ServerRole、Experience、ArenaMode和地图目录只读HTTP契约
│           ├── Proto/                                              # 《神兽联盟》项目专属Proto消息扩展
│           │   └── divine-beasts-context.proto                     # 《神兽联盟》ServerRole、Experience、ArenaMode等项目上下文扩展消息
│           └── Schemas/                                            # 《神兽联盟》ServerRole、Experience、ArenaMode等结构约束
│               ├── arena-mode.schema.json                          # 《神兽联盟》1v1～5v5 ArenaMode定义约束
│               ├── experience.schema.json                          # 《神兽联盟》OpenWorld.Hub/OpenWorld.Main/Village.*等Experience定义约束
│               ├── server-catalog.schema.json                      # 《神兽联盟》服务器角色与体验目录聚合结构
│               └── server-role.schema.json                         # 《神兽联盟》OpenWorld/Village/MainArena三类ServerRole定义约束
├── Docs/                                                           # 契约版本、权限、Codegen、兼容矩阵和仓库边界说明
│   ├── BackendSharedImplementationV1.1.md                          # Backend + Shared V1.1功能实现和边界说明
│   ├── BuildMapping.md                                             # Shared Contract到Backend Go生成包和UE C++消费模块的编译映射
│   ├── codegen-targets.json                                        # Codegen目标映射；定义协议到Go/C++的输出位置
│   ├── codegen-tools.lock.json                                     # 正式Codegen工具版本锁；保证不同开发机生成一致
│   ├── Codegen.md                                                  # 离线/正式Codegen使用方法、工具要求和生成规则
│   ├── compatibility-matrix.json                                   # Client/Server/Backend/Contract版本兼容矩阵
│   ├── contract-version.json                                       # Shared Contract当前语义版本和兼容级别
│   ├── Overview.md                                                 # Shared契约层总体说明、职责和协议分类
│   ├── Permissions.md                                              # Client、Dedicated Server、Backend调用权限与权威边界
│   ├── RepositoryBoundary.md                                       # Shared与Backend目录职责、禁止依赖和生成物边界
│   └── Versioning.md                                               # Proto/OpenAPI/Schema/Event版本演进和兼容规则
└── Generated/                                                      # Shared内唯一允许的生成产物根目录
    └── Cpp/                                                        # UE C++跨语言绑定生成代码；禁止手工修改
        ├── GamePlatform/                                           # GamePlatform公共契约C++生成绑定
        │   ├── ContractRegistry.generated.hpp                      # Codegen生成的C++契约注册表，供UE消费者定位协议和版本
        │   └── ContractVersion.generated.hpp                       # Codegen生成的C++ Contract版本常量
        ├── Games/                                                  # 具体游戏C++生成绑定根目录
        │   └── DivineBeasts/                                       # 《神兽联盟》项目专属C++生成绑定
        │       ├── ContractRegistry.generated.hpp                  # Codegen生成的C++契约注册表，供UE消费者定位协议和版本
        │       └── DivineBeastsCatalog.generated.hpp               # Codegen生成的《神兽联盟》ServerRole/Experience/ArenaMode C++目录
        └── UEConsumerModules.generated.json                        # Codegen生成的UE消费模块映射清单，用于构建和兼容检查
```

## 维护规则

1. 新增或删除 Shared 文件、目录时，必须同步更新本文对应树节点及中文职责说明。
2. 重命名或移动 Shared 文件、目录时，必须在本文中同步调整路径层级和职责说明。
3. Shared 只承载跨语言契约、文档和生成物，不得把 UE Gameplay 或 Go 业务实现写入本目录；职责边界变化时必须同步更新本文。
4. `Generated/` 下的生成产物不得手工修改；生成规则、工具或输出映射变化时，必须同步维护本文及相关契约文档。
5. 本文版本发生结构性变化时，更新文件名版本或在变更记录中记录版本变更原因。

