# GamePlatformGameplay 实施计划

> **执行方式：** 本轮按用户附件直接在唯一正式工作空间内联实施；每项行为先写失败测试，再做最小实现。禁止提交、推送、迁移工作空间或绕过既有空插件描述阻断。

**目标：** 实现第十个基础层插件 `GamePlatformGameplay` 的体验定义、可信加入、权威出生、复制快照、晚加入恢复和两阶段开放，并为真实 UE／Go 联调保留可验证而非伪造的组合根接口。

**架构：** `GameState` 上的体验组件持有体验 Data 租约、复制体验快照和世界内服务入口；`GameMode` 独占可信准入、等待队列、出生预留与控制决策；`PlayerState` 复制公共生命周期；`PlayerController` 通过拥有者令牌提交本地准备。插件只编译依赖 Core、Data、World 和必要引擎模块；Session／Loading／Input 由主工程组合，不形成反向依赖。

**技术栈：** UE5.8 C++、UBT/UHT、Unreal Automation、CMake/CTest 原生策略测试、PowerShell 7、Unreal Python 资产生成。

**规格：** `C:/Users/Freebooz/.codex/attachments/b72694fd-579d-4a4b-9d99-b06f7976e1a8/已粘贴的文本.txt`。

## 全局约束

- 唯一新增插件是 `Game/Plugins/GameFoundation/Gameplay/GamePlatformGameplay`，默认仅一个双端运行模块。
- 运行模块不得依赖 Online、Session、Loading、InputClient、PCG、MOBA、项目层、HTTP、UI、Character、GAS、Combat 或 Niagara。
- 所有客户端输入只能是非权威准备事实；体验、Pawn、出生点和控制权只能由服务器批准。
- 不伪造 `.uasset`／`.umap`；反射代码未编译时只交付幂等引擎脚本并标记资产未生成。
- 三份历史空 `.uplugin` 保留原位，正式 UE 构建失败必须记录，不能通过禁用、移动或替代宿主绕过。
- 所有公开类型、字段、参数、返回、权限、线程、失败、取消和清理均提供中文说明。

## 审查重点

- 任何默认 `HandleStartingNewPlayer`／`RestartPlayer*`／`SpawnDefaultPawn*` 入口都不能绕过准入与体验门禁。
- 迟到的体验加载、准备报告、出生回调或旧 SessionEpoch 不能改变新世界／新玩家／新 Pawn 代次。
- 玩家离开任何阶段都必须释放队列、出生预留、Pawn、令牌和 Data 租约，且不能影响其他玩家。
- 复制 `Active` 不能等价于客户端本地资源已准备；晚加入必须从当前快照恢复而非依赖历史多播。
- 明确离线开发通路不得成为生产缺少准入时的静默回退。

## 任务

### 任务 1：纯策略与边界测试

**文件：**

- 创建 `Game/Plugins/GameFoundation/Gameplay/GamePlatformGameplay/Tests/CMakeLists.txt`
- 创建 `Game/Plugins/GameFoundation/Gameplay/GamePlatformGameplay/Source/GamePlatformGameplay/Private/Tests/GameplayPolicyTests.cpp`
- 创建 `Game/Plugins/GameFoundation/Gameplay/GamePlatformGameplay/Source/GamePlatformGameplay/Private/Policies/GameplayPolicy.h`

**产出接口：** 体验阶段转换、玩家阶段转换、准入代次匹配、出生幂等键、准备令牌匹配、队列容量／截止、确定出生候选排序的无 UE 纯算法。

- [ ] 先写正常、拒绝、取消、超时、旧代次、重复生成、乱序准备和队列边界测试。
- [ ] 运行 CMake/CTest，确认因生产头缺失或行为未实现而失败。
- [ ] 实现最小策略并运行 Debug／Release，要求全部断言通过。

### 任务 2：插件骨架、定义与公开契约

**文件：** 插件描述、构建规则、模块入口、`Public/Definitions`、`Public/Types`、`Public/Interfaces`、`Public/Services`、`Public/Settings`。

**产出接口：** `UGamePlatformExperienceDefinition`、`UGamePlatformPawnDefinition`、体验／玩家／准备／出生值类型、`IGamePlatformGameplayService`、`IGamePlatformGameplayAdmissionSink`、`IGamePlatformSpawnPolicy`、`GamePlatformGameplayServices::Get`。

- [ ] 先写 UE 自动化契约测试，锁定安全默认值、定义校验和公开签名。
- [ ] 实现仅 Core/Data/World 的公开边界及中文注释。
- [ ] 静态检查描述、模块名、导出宏、依赖和禁止头文件。

### 任务 3：体验组件和 Data 租约生命周期

**文件：** `Public/Components/GamePlatformExperienceComponent.h` 及 `Private/Components`、`Private/Experience`。

**产出接口：** 服务器 `BeginExperience`／`BeginDrain`，客户端 OnRep 后本地资源准备，查询、订阅、装配注册与逆序撤销。

- [ ] 先写缺定义、类型错误、版本错误、旧回调、部分装配失败、重复排空和多实例隔离测试。
- [ ] 实现根定义、共享定义、服务器定义和 Pawn 定义租约持有；成功后才发布 `Active`。
- [ ] 失败先失效代次，再逆序撤销装配与租约；客户端本地准备状态不修改服务器阶段。

### 任务 4：权威 GameMode、玩家生命周期和出生控制

**文件：** `Public/Framework`、`Private/Framework`、`Private/Players`、`Private/Spawning`、`Private/Networking`。

**产出接口：** `AGamePlatformGameModeBase`、`AGamePlatformGameStateBase`、`AGamePlatformPlayerStateBase`、`AGamePlatformPlayerControllerBase`，可信准入提交／撤销、等待队列、出生候选、占位、生成、Possess、准备确认、授权重生成和退出清理。

- [ ] 先写默认出生入口绕过、无准入、旧 SessionEpoch、重复 Restart、占位冲突、生成失败、准备令牌错代次和退出清理测试。
- [ ] 覆盖所有 UE 默认生成入口；只有 GameMode 内部有效生成作用域可调用 `SpawnDefaultPawn*`。
- [ ] 复制体验和玩家当前快照；拥有者令牌使用 `COND_OwnerOnly`，公共状态不含凭据或完整账号资料。

### 任务 5：薄主工程开发装配

**文件：** `Game/Source/DivineBeastsArena/Private/Bootstrap/Gameplay`、主模块构建规则、项目描述和配置。

**产出接口：** 明确 `FoundationGameplayOffline` 开关的开发 GameMode／测试 Pawn；生产样式缺少可信准入时保持拒绝。Input／Loading／Session 缺少可链接公开实现时只使用 Gameplay 的中立报告接口并记录阻断。

- [ ] 先写开发入口策略原生测试，证明 Shipping、Commandlet、网络客户端和未显式开关均拒绝离线准入。
- [ ] 实现项目派生类和最小网络测试 Pawn；Active 前禁用权威移动，Active 后由服务器开放。
- [ ] 启用 Gameplay 插件，不启用未完成 Online；不覆盖正式默认地图。

### 任务 6：引擎资产、集成和验证脚本

**文件：** `Tools/AssetTools/CreateGameplayFoundationAssets.py`、Gameplay 集成／网络测试脚本、`Build/Validation/VerifyGameplay.ps1`。

**产出：** `L_GameplayFoundation`、两份 Experience、必要 Pawn Definition 的幂等 UE 生成路径；原生、三目标、资产、Cook、Stage、专服双客户端和人工项分开报告。

- [ ] 先写离线脚本测试，证明不使用 UE 时不会创建二进制占位、重复运行不会覆盖人工资产、外部失败码向上传播。
- [ ] 实现脚本；在当前构建阻断下运行离线与原生部分，正式 UE 项目按真实退出码记录。
- [ ] 不把 NullRHI、单 PIE、端口可达或旧日志当成真实双客户端证据。

### 任务 7：16类正文、40组追踪和目录同步

**文件：** 插件根 `README.md`＋`Docs` 15份专题；仓库 `ExecutionProgress.md`、`InterfaceContract.md`、`Docs/Production/GamePlatformGameplayVerification.md`、总体目录规划。

- [ ] 从最终源码提取准确签名、状态、字段、依赖和命令。
- [ ] 将 GAMEPLAY-01 至 GAMEPLAY-40 分别标为通过／失败／未执行／不适用，附本轮命令、退出码和证据；实现状态与人工状态分栏。
- [ ] 人工审查字段保持“待人工审查”，不由 AI 代签。

### 任务 8：最终验证与独立审查

- [ ] 新鲜运行 Debug／Release 原生测试、脚本测试、依赖扫描和可执行的 Go 回归。
- [ ] 真实尝试 Editor／Client／Server 构建一次，保留三份空描述导致的规则扫描失败证据，不循环重试。
- [ ] 检查差异、未跟踪缓存、秘密样式文件和文档签名；对严重发现执行一次测试先行修复。
- [ ] 最终只报告有直接证据的状态，并给出从哪个阻断点继续。

