# SSLM 设计审查与整改执行计划

> 按任务实施，逐项提交回归证据。执行时使用 superpowers:executing-plans 或 superpowers:subagent-driven-development。审查日期为 2026-11-15。

**基线**　`main/a48afb4f`，本地远程跟踪引用相同。审查开始已有四处服务器配置和四份 UE FileOpenOrder 日志未提交。本计划以实际工作树为准。

**目标**　修复竞技场入退场和验证入口的问题，建立可重跑的性能基线，逐个收口确有多套入口的资源与目录。

**架构**　现有三层及唯一执行链继续作为修复边界。Go Match 与控制面持有可信比赛与准入状态，GamePlatformArena 负责竞技生命周期，项目层负责装配和交互。发布约束通过现有构建、Cook、包审计和本地验证入口执行。

**技术栈**　UE 5.8、Win64 Development、C++、Go、PowerShell、Monolith MCP、Unreal Insights。

**规范依据**　[工作空间规则](../../../AGENTS.md)、[总体规划](../../SSLMEngineeringStandard.md#architecture)、[插件开发规范](../../SSLMEngineeringStandard.md#plugins)、[整合与交付说明](../../Implementation/GamePlatformAuditRemediation/整合与交付说明_2026-10-09.md)、[名称标准](../../SSLMEngineeringStandard.md#naming)。


## 审查意见

静态架构基线符合规范。当前 70 个插件描述中有 48 个代码或机制插件、20 个登记内容包、2 个获批编辑器工具。70 个插件共 91 个模块，主工程另有一个模块。公共与竞技的 Client、Server、Editor 六种声明装配通过，公开类型继承扫描覆盖 609 个头文件、1,346 个类型和 228 条继承边。六组 Pester 共 52 项通过。服务器表现排除配置、Lighting Lab 离线合同、调试启动参数拒绝正数限帧也通过各自的静态检查。

| 优先级 | 确认的问题或证据缺口 | 依据与影响 |
| --- | --- | --- |
| P1 | 同场玩家同时请求首次入场票时争用比赛租约 | `Backend/internal/app/matchservice/runtime.go:336、361–369` 先抢占共享操作租约，再读取既有玩家票据。`Backend/internal/platform/database/postgres/match_operation_production.go:112` 会拒绝租约冲突。项目竞技客户端在 `DivineBeastsArenaClientSubsystem.cpp:313、326–332` 遇到失败后保留错误并挡住后续自动转场，正常玩家可能因此错过首次入场期限。 |
| P1 | 倒计时内开战失败缺正式退场路径 | `GamePlatformArenaGameMode.cpp:401–430` 出生装配失败后进入 Failed。服务器适配层 `GamePlatformArenaServerSubsystem.cpp:89–101、240–252` 主要衔接正常结果和结算后排空。后端 `runtime.go:216–235` 依赖结果、取消、心跳失效或期限变化。专服仍健康时，玩家可能等到后端的比赛故障超时。 |
| P1 | 已结算竞技实例掉线后，尚未返程的玩家可能无法恢复 | `runtime.go:216–247` 在实例不健康时完成并释放比赛。`matchmaking_runtime.go:67` 仅在 Returning 签返程票，且 `DivineBeastsArenaOnlineMatchmakingAdapter.cpp:298–306` 只分派 Returning、TransferReady、InMatch。旧竞技 Assignment 的重连仍会请求竞技恢复，掉线点落在结算与返程之间时存在状态缺口。 |
| P1 | 标准本地快速登录入口缺强制双端版本门禁 | `Build/Game/StartLocalTestClients.ps1:9、52` 核对 Compose、网关和账号配置后启动，将构建与版本门禁留给调用方。现有配对验证证据和辅助逻辑在 Saved 下，正常入口可直接跳过。 |
| P2 | 源码头文件门禁误报 | `Tests/Architecture/ProjectHeaderAudit.psm1:70–85` 的代码扫描和头文件索引漏掉 `.inl`。实际工程脚本扫描 2,130 处引用，报缺失 6 处，逐个核对六个目标都存在。 |
| P2 | DBAClient 收敛回归仍使用旧模块数量 | `Tests/Architecture/DBAPluginConsolidation.Tests.ps1:66` 仍断言五模块。`DBAClient.uplugin:345` 及交付台账登记了第六个模块 `DivineBeastsDemoClient`。修正断言时验证该模块的真实目标及开发装配隔离。 |
| P2 | 水体两张同名开发地图职责难以辨认，部分内容位置保留版本命名 | `AuditCurrentPluginQuality.py:46` 使用 `/Game/Development/Water/Maps/L_GP_WaterReview`，`VillageWaterSystemsMonolith.py:12` 和水面作者使用 `/Game/Development/Water/L_GP_WaterReview`。`ExistingMaps.json` 将前者登记为无正式案例的诊断地图，后者关联三个开发案例。先原生核对，再按各自职责定名。同样，英雄图标和公共表现包按路径筛得 252 个版本或审核名称候选，名字本身无法证明生效、重复或进入正式包。 |
| P2 | 托管目录有误入的本地产物，部分规划仍列历史数量 | 12,033 个 Git 跟踪路径通过目录和非文档文件的 ASCII 检查。Git 仍跟踪 `Game/Game/Saved/Validation/TerrainGaea/RockRealism01/RockRealism01_MonolithScenePreview_Sphere.png` 和根级 `Monolith/monolith_proxy_tools_localhost_9316.json`。四个已跟踪的 FileOpenOrder 文件实际会被 Cook 刷新，已有证据代码准确识别这四个生成物。总体规划 IN17 仍以 19 个内容包和 67 份描述标为存量快照，应将读者引向现行登记。 |
| P2 | 运行、资产和表现验收缺口 | 静态质量登记中 54 个插件要求人工表现验收，其中 7 个缺少可操作场景登记。11 月 9 日变更已经做过部分原生和包级验证，仍有真实竞技、实机视觉和压力证据未闭合。当前 Lighting Lab 通过离线合同，需要在原生地图及实机画面上验证光照和山水构图。 |


## 性能审查

| 项目 | 已有证据 | 本轮要验证的成本与调整条件 |
| --- | --- | --- |
| 竞技 VFX | 11 月 9 日计划登记已接入代表性 Effect Type，并有 5 个待补 Niagara System。项目配置的 MaxActiveInstances 和 HardMaxTrackedInstances 管理平台请求。 | 采集真实 System／Emitter、GPU、半透明过绘、折射及光源。先覆盖十人同施放，再优化经测量确认的装饰成本。保留 Low 档危险区、弹道和命中信息，NDC 启用须验证去重、取消和原生回退。 |
| Village 怪群与画面 | 200 只狼是已交付的历史布置。既有村庄画面数据来自历史 PIE 镜头。 | `DivineBeastsVillageWolfSpawnPolicy.h:9` 的上限及 `GamePlatformAIController.cpp:764、794、821` 的定期候选筛选值得重点采样。分别检查空闲、靠近聚怪和战斗，记录服务端帧时间、感知、寻路、移动、复制，以及客户端地形、草木、水体和光照。只有串行尖峰或流送瓶颈可重现时，才分别考虑错峰感知与地图分区。 |
| 世界名称板 | 已有单实体快速路径；相机变化仍进入全量候选重投影。 | 对照静止与移动相机、可见实体数及 UI 开关，核对 `GamePlatformWorldUIService.cpp:247–262、279–299`。若为热点，合并同帧变更并复用视图投影输入，保持事件驱动、静止相机的单实体更新和世界代次回收。 |
| 权威弹体 | `GamePlatformCombatProjectile.cpp:25–27、233–270` 请求 60 Hz 更新频率，并按时间、路程和转向细分，部分子步执行两路扫描。 | 记录每帧子步、扫描 CPU 和复制字节。仅在瓶颈证实后调整扫描或表现复制，保留追踪原目标、目标中心、遮挡、碰撞和已确认的小角度取消附加弧线语义。 |
| 匹配后端 | 历史 10／100／1,000 票据快照读取与复核往返从 220／2,200／22,000 降至 6／10／88。 | 复用已有 benchmark，补足真实 Redis／Postgres 竞赛负载，测候选调度、锁等待、订阅容量、吞吐和 P95／P99。快照往返指标仅覆盖该调用路径。 |

性能采样工具以 UE5.8 Unreal Insights、Networking Insights 及已有项目计数为主。证据绑定包哈希、地图、画质、分辨率、RHI、机器及场景脚本。 [Unreal Insights](https://dev.epicgames.com/documentation/unreal-engine/unreal-insights-in-unreal-engine?lang=en-US) · [Networking Insights](https://dev.epicgames.com/documentation/unreal-engine/networking-insights-in-unreal-engine)


## 整改顺序与验收

任务 1 的逻辑回归与任务 2 的启动门禁可并行。任务 1 的所有实机步骤需在任务 2 通过后进行。任务 3 中两个静态门禁修复可同步开始，资产迁移安排独立变更。

### 1．修复竞技入场、比赛终止和返程

- [ ] 在 `Backend/internal/app/matchservice/runtime_test.go` 扩展既有五模式回归。覆盖同一候选的两人及十人同时取首次票、倒计时断线或出生失败但实例仍健康、结果已提交而竞技实例在返程前失联。使用真实存储集成夹具验证并发租约拒绝路径。
- [ ] 修改 `matchservice/runtime.go`，按已有角色、会话、比赛阶段和 Server Boot 合同读取仍有效的首次入场票。需要签发新票或改变状态的动作继续串行且幂等。项目竞技客户端对明确可重试的暂态冲突使用有截止时间的原请求重试，账号换代、用户取消及转场结束时撤销在途。
- [ ] 由 `GamePlatformArenaGameMode.cpp` 和 `GamePlatformArenaServerSubsystem.cpp` 沿既有比赛生命周期上报受信的终止原因。结束通知按 MatchId 和 Boot 去重，撤回半完成出生，按权威准入流返回且禁止伪造胜负。
- [ ] 让结算、实例释放和个人返程资格独立收敛。扩展 `matchapi/matchmaking_runtime.go` 及当前竞技恢复适配路径，在允许的恢复窗口内按同一锁定角色与原安全世界重新签票。存在已验证的排队位置时复用，失效时沿用现有安全出生回退。若确需协议变更，从 Shared 真源更新并重新生成现有绑定。
- [ ] 运行 `go test ./internal/app/matchservice ./internal/app/matchapi ./internal/app/gameservercontrol ./internal/platform/database/postgres`（在 Backend 下）并执行对应 UE Automation。实际双端通过 1v1 报名→进场→比赛／退场→返程，且三个故障用例的玩家可恢复。随后验证其余四种模式、并发、换代和重连。检查结果只提交一次，返程票仅对当前会话与锁定角色有效。

### 2．让快速登录入口强制验证双端版本

- [ ] 提取 `Saved/Validation/ArenaPerformance_20261109` 中现有配对验证的有效规则，整理到 `Build/Game`。复用 Foundation 证明及 Village/MainArena 启动记录，核对实际 exe、PID／启动态、角色、注册就绪、完整构建与 Cook/Stage、两端共享战斗／复制合同。
- [ ] 在 `StartLocalTestClients.ps1` 读取账号种子及启动客户端之前调用统一门禁。旧包、证明缺失、服务器换进程或动态 MainArena 池混有旧实例时，拒绝启动并指出具体差异。修正后按用户既定同步流程重测。新增脚本夹具以假数据验证匹配、错版本、已退出、PID 复用或错误角色，失败路径应产生零个客户端进程。
- [ ] 复用 `Deploy/Local/StartVillageDevelopment.ps1`、`RunMainArenaPool.ps1` 及标准快速登录入口。测试账号凭据仅通过进程环境传递。人工核对真实登录页有十个可用按钮，且 `t.MaxFPS=0`、`r.VSync=0` 实际生效。保留进程就绪、登录、Village 准入、MainArena 准入的分别记录。

### 3．恢复静态门禁的可信度，收口目录与命名

- [ ] 先扩展 `ProjectHeaderAudit.Tests.ps1`，验证存在的项目 `.inl`、确实缺失的 `.inl` 和内联文件自身包含缺失头的情况。修改 `ProjectHeaderAudit.psm1` 让源码扫描、索引和路径定位按相同的受管扩展规则执行。重跑 `Tests/Architecture/ValidateProjectHeaders.ps1` 和 `ValidateDesignBaseline.ps1`。
- [ ] 修改 `DBAPluginConsolidation.Tests.ps1` 的 DBAClient 期望集合，增加第六个已登记开发模块的精确条件检查。分别验证真实 Client、Editor、正式 Server 和发行配置。
- [ ] 在现有 `NamingStandard.md` 与 `RenameCandidates.csv` 的例外及候选基础上，用 UE AssetRegistry／Monolith 核对 Water 两张地图的出入边、演示入口、专用夹具职责和实际 Cook，然后冻结经核实的映射。若是同一功能，收敛作者、测试和发布审计到唯一当前路径。若两张具有不同真实用途，为诊断夹具和审核区采用明确职责名并更新消费者。
- [ ] 对 `DBAPresentationPack_Core`、生肖 UI 等候选按消费者逐项确认当前、历史、引擎兼容或供应商身份。只迁移确认仍活动且违反命名或重复约束的资产。备份，通过 Monolith 原生重命名、保存和冷重载，修复引用与必要重定向。发布包门禁拒绝已正式退休的资产。 [UE 资产重定向器](https://dev.epicgames.com/documentation/unreal-engine/asset-redirectors-in-unreal-engine?lang=en-US)
- [ ] 核对嵌套 Game/Saved 截图和根级 Monolith 快照是否有消费者，将需要版本化的证据归对应 Docs，将本机生成快照归工作区 Saved 或正式工具的可再生输出。单独核查 FileOpenOrder 是否是有意跟踪的打包输入，若需保留批准的顺序基线，隔离 Cook 运行产生的日志。总体规划保留历史快照并指向当前登记。命名门禁区分版本管理范围和本机产物，尊重 UE 外置 Actor／Object 与已登记例外。

### 4．建立性能基线并做定向优化

- [ ] 任务 2 通过后可与任务 3 并行准备采样。冻结 1v1、5v5 和 Village 三组对照，其中 Village 包含 200 只狼场景的空闲、聚怪及战斗。标准客户端启动器单次最多四个，5v5 使用跨主机真实准入或经验证的等效负载工具。客户端记录整帧 P50／P95／P99、Game／Render／GPU、首次施法与转场尖峰，专服记录帧时间、AI、扫掠、移动和复制，后端记录延迟、错误、租约冲突及 RTT。
- [ ] MainArena 按项目规范测试四档、十人同施法、每秒至少 150 次短命中、AOE 叠加和 10 分钟稳定场景，并观察断线、取消、重连及切 World 的回收。报告候选预算对比。High 候选为整帧 P95 ≤16.67ms、P99 ≤20ms、VFX GPU ≤1.5ms、CPU ≤0.6ms。
- [ ] 仅改已测热点，一次验证一类变化。VFX 从主体／装饰分类和其余 System 的实际 Effect Type 入手。服务器先量化怪群感知和弹体的占比。UI 仅在重投影确有成本时调整事件合并。保存同机同场景的 A/B 记录与回退点。

### 5．验证真实包与人工体验

- [ ] 修复前后分别记录受影响目标的真实完整增量构建。使用 `Build/Game/BuildFoundation.ps1`，锁定 UE5.8 Win64 Development，参数 `-NoSharedPCH -NoUBA -MaxParallelActions 8`。Editor 另加 `-UsePrecompiled -AdditionalPlugins DBAServer`。通过既有 Cook 入口以对应的真实地图和配置进行 Stage，生成物留在当前工作区 Saved。核对普通装配、无竞技装配及 DedicatedServer 容器实际依赖和剥离结果。
- [ ] 按 `Docs/Design/PluginDevelopment20261114/ImplementationPlan.md` 补缺少可操作场景的七个插件。先补齐竞技与项目世界验收，其余复用既有宿主与案例路由。正式 UI 资产走 Monolith，分别验证编译、保存、重载、输入和人工画面。
- [ ] 沿独立 Lighting Lab 核对固定曝光、相机、时段和天气下的艺术方向及 GPU 成本，再确定哪些方案进入正式地图。村庄、竞技场和常驻世界的三角色准入、五模式及手工玩法结论随最终包登记。

## 执行记录

- 竞技修复补齐并行取首次入场票、开战失败的终止上报、竞技实例失联后的返程票恢复。正式应用流程资产增加两条 `Recover` 路由。Go 定向测试、真实 Postgres 2／10 请求并发和 Redis 持久化回归通过。UE 原生的 `StartAbort`、`FiveModeSingleRoundElimination`、`CommandEnvelope` 三项通过。本机 Compose 的五项后端服务已核对就绪。
- 正式本地启动器在读取快速登录账号前检查 Client／Server 构建、双端 Cook/Stage、战斗指纹和实际 Village／MainArena 实例。构建证明合同的 40 个隔离用例通过。两端完整增量构建共用 `fadc90ef-be2d-462e-9696-f7997eaaf84c`，战斗指纹一致。
- `.inl` 及 DBAClient 模块门禁回归通过，实时头文件扫描覆盖 2,130 处引用、零缺失。两份误入仓库的本地产物已移回 `Saved`。两份 Water 同名地图经原生核对，分别是开发演示与诊断地图，需在迁移时同步更新各自的正式消费者。
- 正式 `L_Village_Start` 复用现有 Landscape 实例。按当地湖面与山顶高程归一化，岩石约在 254–280 米完成过渡，积雪约在 365–391 米完成高程过渡，坡度控制陡壁积雪，手绘道路和耕地得到保护。Monolith 编译、保存、重载并回读既有地图绑定和材质链接。基色截图及原生证据位于 `Saved/Validation/SSLMRemediation20261115/NativeEditor`。
- 专服 Cook/Stage、服务器表现剥离及资源归属门禁通过。以本次制品启动的 Village 和 MainArena 已由实际控制面确认 `Ready`。两个无人在线的专服各采集 14 个约 2 秒区间，Village 平均占 9.45% 的单核 CPU、工作集 323.0 MiB，MainArena 分别为 2.04% 和 295.8 MiB。Village 日志记录该次布置受安全区、间距、NavMesh 及地形条件限制，实际生成 147 只狼。原始样本见 `Saved/Validation/SSLMRemediation20261115/ServerIdleProcesses.csv`。
- 客户端 Cook/Stage 首次缺少 5 个系统的同次编译记录，并将 ShaderCompileWorker 启动耗时误判为编译问题。修正后全部 15 个 Niagara 系统取得同次编译记录，真实 IoStore 门禁通过，并读到两份新手村材质的非空导出。Client Cook `b9a47eb4-424a-4d22-9a00-cbc87b5356ac` 与 Server Cook `ea086d8f-21a9-48d7-a1fe-1609597ddbe5` 战斗指纹一致。
- 本地启动 `bd311146-48bf-4595-8004-36514bf419bd` 的运行中双端门禁通过。两个实际登录页都显示十个测试账号快捷按钮，引擎日志回读 `t.MaxFPS=0`、`r.VSync=0`。一个客户端通过专服连接启动 `L_Village_Start`，客户端地图日志与专服 `Join succeeded` 对上。两个客户端随后由视口正常退出，竞技入场／退场／返程、正式游戏镜头的视觉验收及负载性能采样待完成。
