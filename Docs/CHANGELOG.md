# 变更记录

保留已有工程变更记录；不根据历史聊天补造不存在的提交或验收记录。

## 2026-09-29｜GamePlatformSettings 四模块专项审查与实装

- 专项审查确认原 `GamePlatformSettings` 只有 ClientOnly 模块入口、没有公开契约、设置模型、校验、生命周期、持久化、测试或消费者；设置域本身具有跨项目价值，因此保留插件身份并形成 Runtime／Client／Server／Editor 四模块，而不是继续保留空壳。
- Runtime 新增类型安全 Descriptor／Provider／Registry／分层解析／Snapshot／ChangeSet／Migration／异步保存编排；Client 直接适配 UE5.8 `UGameUserSettings` 提供设备设置 Stage→Preview→Confirm/Cancel，并增加本地 User Profile；Server 提供 INI／Environment／CommandLine 只读覆盖；Editor 提供 Provider/Descriptor 校验。四模块保持 Client／Server／Editor → Runtime → GamePlatformCore 单向依赖，`CanContainContent=false`。
- 重新锁定职责边界：Input重绑／灵敏度、UI可访问性、Camera行为、SFX播放／混音和Save通用业务存档仍由各自插件拥有；当前没有非测试 `IGamePlatformSettingsProvider`，因此 Runtime/Server 只能标记为框架已实现，未经逐领域“旧真源迁移→消费者切换→旧持久化删除”不得成为第二套设置真源。
- 稳定性与安全整改包括：0 Tick/Ticker、显式Apply/Save、设备预览不写盘、异步保存弱引用回主线程；Provider拓扑变化在Save进行中延迟到同一MutationGeneration保存成功后处理；敏感Descriptor只允许Session临时作用域且Server只在检测到真实INI/环境变量/命令行覆盖尝试时拒绝；User持久化必须Client、Server持久化与ServerDefault必须Server；环境变量规范化键冲突Fail Closed；字符串设置统一限制4096字符，凭据/令牌/密钥继续使用部署秘密机制。
- 已核对锁定UE5.8 `UGameUserSettings`真实API；四模块专项架构门禁通过并输出“无生产Provider”成熟度告警，项目头文件审计410处/0缺失。UE定向编译尝试因另一个仍持有全局UBT互斥锁的构建进程返回 `ConflictingInstance`，未进入Settings编译；UE Automation、Client/Server Cook/Stage、Standalone/Packaged、多显示器/高DPI和人工长稳验收仍待执行，不把静态验证冒充运行通过。

## 2026-09-29｜GamePlatformOpenWorld 空壳专项退休

- 专项审查确认 `GamePlatformOpenWorld` 只有 Runtime／ServerOnly 模块注册入口，没有公开契约、测试、资产或运行时消费者；全工作空间未发现项目 Dynamic World Event／Zone Activity／Population Scheduler 等真实跨项目需求。
- 采用退休而非补造“万能 OpenWorld Manager”：OpenWorld 服务器角色继续保留，世界生命周期／流送由 GamePlatformWorld 承担，PCG／Navigation／Interaction／AI 各自保持独立边界，项目大厅／主城／野外规则继续归 DivineBeasts 层。
- 正式基线调整为39个GamePlatform稳定身份（平台层38＋MOBA层GamePlatformArena 1）、1个MobaPresentation、5个DBA代码插件，共45个代码／机制插件＋登记内容N；DesignBaselineAudit同步拒绝重新引入已退休空壳。
- 本变更不修改Shared协议、服务器角色、地图／资产身份或现有运行时代码；UE完整构建、Cook／Stage、联机与人工验收仍按独立证据记录。
- 专项 DesignBaselineAudit 回归13/13通过；实际工作区基线正确识别 `GamePlatform 39/39`、代码／机制45、World分类4，Game／Shared运行引用为0且 `git diff --check` 通过。全量基线仍有22项既有失败（12个英雄空内容包＋10条DBAWorlds跨插件声明缺失），不归因于本次退休；本轮未停止或借用并行中的UE构建，因此不宣称完整UE构建、Cook／Stage或联机通过。

## 2026-09-28｜Monolith登录界面与事件驱动边界

- 将“神兽联盟项目自有用户界面视觉资产必须通过 Monolith MCP 创建、修改、编译、保存和回读”写入全局工程规则、总体规划和插件规范；明确 GamePlatformUI、DBAClient 与第三层内容包的职责边界。
- 新增并登记纯内容插件 `DBAUIPack_Core`，通过 Monolith 0.20.3 生成真实 `WBP_DBA_UI_RootLayout` 与 `WBP_DBA_UI_Login` 资产；登录页采用纯黑页面和黑色用户名／密码输入框，不使用卡片或面板，保留蓝色登录按钮及事件驱动的忙碌、维护和错误反馈。
- DBAClient 新增登录页C++父类与事件绑定；页面只消费 ViewModel 状态并提交命令，不使用业务 Tick、不直接访问HTTP，也不保存密码。密码在提交和页面失活时清空。
- Monolith回读显示根布局10个节点、登录页13个节点，两个蓝图编译均为0错误／0警告，登录页可访问性审计0问题。CommonUI静态审计保留1条工具通用焦点属性警告，项目实际通过平台原生焦点契约和页面目录的`AccountInput`提供焦点，仍待PIE验证。
- `DivineBeastsUIClient` Editor定向构建成功；原生自动化先后发现初始`NAME_None`路由、命令完成事件生命周期和未交付移动端资产路径问题，修复后重新编译、重启并最终复测7/7通过。
- 本轮资产与源码验证不冒充真实后端登录、PIE、Cook、移动设备或人工视觉验收；当前在线服务适配仍需独立联调。

## 2026-09-27｜应用流程架构说明与静态门禁补齐

- 新增 `DBAClient/Docs/ApplicationFlowArchitecture.md`，明确项目层只组合唯一平台流程执行器，并记录上下文、会话准入、世界就绪、恢复与性能边界。
- 新增 `DBAClient/Tests/Scripts/TestApplicationFlowArchitecture.ps1`，验证旧流程 API 为零、现行流程能力存在、业务 Tick/Ticker 为零且模块保持 `ClientOnly`；本轮脚本实际通过。
- 同步 DBAClient 与 GamePlatformApplicationFlow README，并以当前 UE5.8 全量 Client 构建结果纠正验证边界：Flow/UI 定向模块已通过，完整 Client 仍由主工程 Online/PCG 公开头依赖和 GamePlatformWorld 测试标志问题阻断。
- MOBA 竞技客户端新增通用 HUD／Screen 基类，竞技 ViewModel 接入平台 ViewModel 事件链；补齐 GamePlatformUI、UMG 与 CommonUI 直接依赖后，GamePlatformArenaClient 的 UE5.8 Editor／Win64 Client 定向模块编译通过。

## 2026-09-27｜主分支合并与UE5.8集成修复

- 将 `codex/plugin-merge-20260926` 合并回 `main`；变更日志冲突完整保留主分支跨平台契约门禁记录及开发分支Core、Data、Loading、Input实现记录，未用单侧版本覆盖另一侧证据。
- 补齐 `FGamePlatformAssetLoader::Cancel`，统一既有UI、VFX、Equipment和AI普通软资源加载的取消入口；无效句柄保持幂等，句柄释放仍由调用方生命周期负责。
- 修复登录ViewModel局部变量遮蔽成员的警告即错误；服务器生命周期快照的内部`uint64`代次不再错误暴露为Blueprint属性，保留C++过期回调判定语义。
- 将DBAServer已失效的全局`FWorldDelegates::OnWorldBeginPlay`改为UE5.8可用的世界初始化监听与具体世界BeginPlay委托；跨地图先解除旧世界委托，只有当前GameInstance世界真正BeginPlay后才进入注册门禁。
- 排除开发分支误带入的GamePlatformCore DLL/PDB。合并结果已通过六组原生C++ Debug／Release测试、Foundation资产脚本50项、Architecture Pester 65项、Go 1.23.12容器测试／vet／契约生成检查，以及UE5.8 Editor、Win64 Client、Win64 Server相关模块构建；不把这些定向检查表述为Cook、Stage、联机或人工签审。

## 2026-09-27｜跨平台契约生成门禁修复

- 修复契约生成器在Windows工作树与Linux容器之间因CRLF/LF差异误报生成物过期的问题；生成修订摘要和`-check`统一文本换行后比较，真实内容变化仍会失败。
- 增加摘要换行稳定性和生成物内容比较回归测试，重新生成Go/C++项目目录摘要；`go test -count=1 ./...`与`go vet ./...`通过。

## 2026-09-27｜GamePlatformInput跨端输入底座完善

- 第三轮语义分层：新增 `FGamePlatformInputSemanticId / FGamePlatformInputSemanticDescriptor` 和 `InputProfileCompiler`，Profile准备阶段一次编译为 `CompiledActions[CompactSlot]`；Enhanced Input高频回调、Interrupt和Touch更新均按Slot数组访问，不在高频路径查GameplayTag/TMap。
- 旧 `EGamePlatformInputSemantic` 继续保持原Tag字符串和API兼容，但AttackPrimary、AbilitySlot1～4、TargetLock降为Legacy兼容入口；平台长期语义只保留跨游戏通用导航/视角/UI/交互合同。
- `DBAClient` 新增 `DivineBeastsInputClient` ClientOnly模块，定义 `DivineBeasts.Input.*` 主攻击/四技能槽/目标锁定语义、项目Profile校验、项目输入事件桥和Touch项目入口；攻击/技能槽映射到 `Platform.Ability.Input.DivineBeasts.*`，TargetLock不伪装成GAS技能。
- 当前正式工程已通过 `GamePlatformInputClient` 与 `DivineBeastsInputClient` 的 UE5.8 Editor／Win64 Client 定向模块构建，UHT、编译与链接成功；Native Debug／Release 各410断言通过，输入专项架构脚本与三层继承边界门禁通过。全局设计基线另有 DBAArena→GamePlatformUIClient 插件依赖声明问题，与本次输入实现无关。
- 平台新增 `EGamePlatformBuiltInInputSemantic` 与 `GetBuiltInSemanticTag/GetBuiltInSemanticDescriptor`，只公开 Move/Look/Interact/Menu/Confirm/Cancel 七类跨游戏公共语义；新项目代码不再通过旧固定枚举消费平台公共语义。
- 将目标平台默认设备/禁用设备回退逻辑拆到 Private `Devices/InputDevicePolicy.h`，作为 LocalPlayerSubsystem 私有职责拆分第一步；对外仍保持唯一平台输入服务。
- 输入→GAS联调发现 `UGamePlatformAbilitySetDefinition::ValidateDefinition()` 只有声明未实现，补齐纯字段校验后 `GamePlatformAbilitySystem` Editor／Win64 Client定向构建通过；最终 `GamePlatformInputClient`、`DivineBeastsInputClient`、`GamePlatformAbilitySystem` 正式工程模块均通过。

- 第二轮性能收敛：BlockLedger改为低频32位引用计数+缓存组合掩码，高频 `IsBlocked/CombinedMask` 为 O(1)；13个稳定输入语义的 ActionGate 改为固定数组槽，避免高频哈希查找/首次节点分配。
- 设备默认策略改为按目标平台决定：Android/iOS默认Touch，桌面默认KeyboardMouse，修复触屏PC启动即显示移动提示的问题；新增 `DeviceRevision`，只有真实设备族变化才递增。
- Native Debug/Release 各410断言通过，UE5.8 Editor/Win64 Client模块在第二轮优化后再次构建成功。UE Automation已实际尝试，但在测试队列前被引擎 `ValidatePlatforms -AllPlatforms` 的Android r27c缺失和VisionOS SDK `MainVersion`缺失阻断，不误报用例失败或通过。

- 审查确认原HEAD只有Input Public契约/Profile/语义与测试源，缺少实际 `InputPolicy.h` 和 `GamePlatformInputLocalPlayerSubsystem.cpp`；本轮补齐生产策略内核和LocalPlayer执行层，不创建第二套输入系统。
- PC统一支持键盘/鼠标与手柄；移动端通过 `Begin/Update/EndTouchInput` 将虚拟摇杆、视角和技能按钮注入同一Enhanced Input语义链，具体UMG/手势布局继续归UI/项目层，避免GamePlatformInput反向依赖表现或神兽联盟项目代码。
- 增加设备族、Touch独立死区、通用视角灵敏度/XY反转、移动死区倍率，并进一步增加 `TouchLookSensitivityMultiplier` 与 `TouchMoveScale`，让移动端视角/虚拟摇杆手感可独立于PC调整；全部本地偏好仅显式保存时写磁盘。
- 性能采用事件驱动：不使用固定每帧输入Tick；只在存在弱Owner租约时用4Hz维护Ticker清理失效记录；Context/Block/Binding/Subscription/Touch均有容量上限，高频回调不加载资产、不写磁盘、不复制订阅数组。新增 `FGamePlatformInputDiagnostics` 统计事件/回调、设备切换、Mapping重建、维护Tick、Owner回收和维护耗时，不反向依赖Telemetry。
- Native C++17 Debug／Release 各1/1通过，共410条断言、0失败；UE5.8 Editor与Win64 Client的 `GamePlatformInputClient` 模块构建均成功。Android Client构建已实际尝试但当前Runner缺少UE5.8要求的NDK r27c，停在SDK校验阶段；iOS需macOS/Xcode或远程工具链，未执行。
- 新增插件 `README.md`、`Docs/Architecture.md`、`API.md`、`TestingAndEvidence.md`、`ManualReview.md`，并同步插件清单、实施进度和总体目录说明。

## 2026-09-27｜GamePlatformLoading加载屏障完善

- `LoadingPolicy` 增加任务/依赖容量门禁、哈希化ID/依赖校验和冻结 `TaskId → 索引`，减少运行期依赖查询的线性扫描；原生 Debug／Release 各1/1通过并输出46条断言，CMake启用警告即错误。
- `GamePlatformLoadingSubsystem` 从 GameInstance 全生命周期固定20Hz Ticker 改为按需调度：Idle零轮询、Running 20Hz、Ready且资源保留时2Hz弱Owner监视、释放后停表；新增 `FGamePlatformLoadingDiagnostics` 统计Ticker/Poll/快照/回调和Tick耗时，并为订阅/自定义工厂增加实例级容量上限。
- 保持 Loading 只依赖 Core/Data，不反向依赖 Session/Flow/项目层；《神兽联盟》推荐将 SessionAdmission、WorldDefinition/WorldPresence、CharacterReady、GameplayReady、EssentialUIReady 等事实由上层任务适配后交给同一Ready屏障，非关键表现可Optional/Degradable。
- UE5.8 `GamePlatformLoading` Editor／Client／Server 三目标模块构建均成功；UE Automation、真实Definition/地图、多PIE、Session准入、Cook/Stage和人工签审仍未执行，不宣称生产就绪。
- 同步插件 Architecture/API/TaskModel/ProgressModel/ReadinessBarrier/Integration/ConfigurationAndRun/TestingAndEvidence/ManualReview、README，以及项目插件清单、实施进度与生产验证记录。

## 2026-09-27｜GamePlatformData数据底座完善

- `UGamePlatformDefinitionBase` 增加 AssetRegistry 结构版本、内容修订和直接依赖数量标签，并在基础校验阶段拒绝 Definition 自依赖；Runtime 与 Editor 统一使用 `GamePlatformDataLimits` 管理依赖深度、唯一节点和单租约 Bundle 安全上限。
- `FGamePlatformDataDiagnostics` 增加当前唯一 Definition／Bundle 数、幂等释放记录数和 Accepted／Rejected／Succeeded／Failed／Cancelled 累计计数；不引入 Telemetry 反向依赖，也不改变现有租约与主资产公开 API。
- 修复 UE5.8 编辑器测试中已不存在的 `PKG_Transient`，改用符合新建未保存内存包语义的 `PKG_NewlyCreated`；原生需求账本 Debug／Release 各1/1通过，GamePlatformData Runtime／Editor／Client／Server 模块构建均成功。
- 新增 `GamePlatformData/Docs/Architecture.md`、`API.md`、`TestingAndEvidence.md`，明确 RequiredDefinitions 不是 Cook 软引用替代品、Chunk/Cook 归内容包与构建配置、Server-safe 必须由真实依赖图和 Server Cook/Stage 证明。

## 2026-09-27｜GamePlatformCore核心契约完善

- GamePlatformCore 新增 `FGamePlatformErrorCode（结构化错误码）` 与 `FGamePlatformVersionRange（版本兼容区间）`，并为 `FGamePlatformId` 增加安全 `TryCreate`；保留既有 `FGamePlatformResult.Code:FName` 和原有调用方式，不批量破坏领域错误码。
- `FGamePlatformResult` 新增结构化错误码 Failure／Unsupported 重载与 `TryGetStructuredCode`；默认未配置版本区间按 Fail Closed 拒绝候选，兼容政策仍归具体领域。
- 原生生产算法 Debug／Release 各 13/13 场景通过；UE5.8 `GamePlatformCore` 模块的 Editor／Client／Server 三目标构建均成功并经过 UHT。UE Automation、全工程构建、Cook／Stage 未冒充已通过。
- 新增 `GamePlatformCore/Docs/Architecture.md`、`API.md`、`TestingAndEvidence.md` 并更新插件 README、游戏端插件清单与文档索引。
- UE模块构建生成的 `GamePlatformCore/Binaries/Win64/*.dll/*.pdb` 不作为源码交付，已从工作树清理，并新增 `/Game/Plugins/**/Binaries/` 忽略规则防止后续误提交。

## 2026-09-27｜旧插件目录与生成物清理

- 删除旧 `Game/Plugins/GameFoundation/` 下误提交的74个 `Saved/NativeTests` 生成文件，并清理本机残留的 DivineBeasts 旧空分类目录。
- 增加 `/Game/Plugins/**/Saved/` 忽略规则；正式插件目录保留 `GamePlatform`、`MobaCommon`、`DivineBeasts` 三层，40个GamePlatform身份和5个DBA代码插件不变。
- 清理后架构回归65/65及设计基线通过；本次中止并清除了未完成的UE Client/Server构建产物，未宣称编译、Cook或运行通过。

## 2026-09-27｜游戏端插件清单设计

- 新增 `Docs/Architecture/游戏端插件清单设计.md`，按当前真实 `.uplugin`、模块和源码整理46个代码／机制插件的名称、层级、模块端侧、已实现功能、成熟状态、验证资料和后续重点。
- 同步 `Docs/README.md`、`Game/Plugins/README.md` 与总体目录规划说明，使插件清单成为后续插件新增、删除、重命名和职责调整时必须维护的主台账。
- 本次仅更新文档，不修改运行时代码，不把测试／文档存在误报为UE构建、资产审核、Cook或生产验收通过。

## 2026-09-27｜游戏端插件系统P0收敛审计

- 新增P0-1～P0-9审计与实施规格，补充Animation、Camera、SFX和统一Review Harness文档；明确当前真实资产为零且Session公开服务仍缺失，不宣称功能或人工审核完成。
- 新增PowerShell三层继承边界审计并接入设计基线；GamePlatformDeveloperTools增加对应编辑器验证器，GamePlatformData编辑器验证器按真实职责更名。
- 本轮架构回归65/65、实际继承扫描299个Public头／629个类型／42条边通过；头文件预检仍因`GamePlatformSessionClientSubsystem.h`缺失失败，未执行UE编译、Cook、联机或人工验收。

## 2026-09-27｜三层类继承与扩展规范

- 新增 `Docs/Architecture/三层类继承与扩展规范.md`，明确 `GamePlatform（平台基类） → MobaCommon（MOBA可选扩展） → DivineBeasts（项目派生）` 的单向继承与依赖边界。
- 规定公共/通用领域优先建立稳定基类、接口或Definition；纯内容差异使用DataAsset实例，运行时协作优先接口、组件、Provider和组合，避免机械深继承。
- 明确VFX、角色、世界、竞技、UI/ViewModel等推荐继承链，并要求后续在GamePlatformDeveloperTools增加Inheritance Boundary Validation（三层继承边界校验）。

## 2026-09-27｜应用流程与会话准入纵向修复设计（待审核）

- 新增 `Docs/Architecture/游戏流程与会话准入后端纵向修复设计规格.md`，依据当前代码记录 Flow API 断层、GameServerControl 内部路由鉴权缺口、Gateway 玩家分配入口缺失、进程内 Assignment 状态及未接入的 PostgreSQL 准入内核。
- 设计提出 Shared 真源、Gateway 认证主体、受保护控制面、PostgreSQL 准入与 UE 真实连接绑定的一条纵向路径，并将锁定 UE5.8 握手验证设为 Ready/端到端实现门禁。
- 仅新增待审核设计文档并更新索引；没有修改业务源码、契约、数据库迁移或部署，没有运行测试/构建。

## 2026-09-27｜工程缺项修复与真实验证

- 补齐六项真实UE默认配置，并将自动备份归入新增的EditorPerProjectUserSettings默认层；保留原有Engine／Game设置及用户Saved配置。必选配置8/8、总配置9份和实际结构审计通过，三角色、40个GamePlatform身份和46+N边界不变。
- 新增真实工程配置回归、自有头文件预检及失败／正常夹具；Architecture测试60/60通过。预检覆盖主工程／项目插件、续行／注释／字面量和空扫描，实际仍报告234处引用中的4处缺失；不能把检查器自身通过写成项目源码通过。
- 修正DBAWorlds自动化测试残留的独立Lobby合法角色，补全三角色七体验正向映射、旧角色拒绝和合法大厅携带竞技模式拒绝用例；新增与Shared真源一致性回归。UE自动化尚未执行，未改生产角色或生成契约。
- 找到D盘UE5.8源码工具链，原生Session／Loading／ApplicationFlow三个Debug测试入口通过；既有打包配置3项UBT行为测试通过。正式Editor UHT完成，随后完整编译因提交内存压力和120秒超时失败，Client／Server未开始，无Cook或游戏发布。
- 项目旧流程接口和Session真实连接／准入仍未修复；明确权威绑定与取消迁移顺序，不添加空兼容类型。详见[工程缺项修复执行记录](Architecture/工程缺项修复执行记录.md)。
- 独立复核指出的配置层与预检问题已补失败回归并修正；新TestProjectEditorConfig通过UE5.8 UBT验证两个层级和5项设置，不代表编辑器交互或完整UE构建通过。

## 2026-09-27｜三层目录与可选竞技迁移

- 按用户批准方案统一GamePlatform／MobaCommon／DivineBeasts：39个平台插件与MOBA层GamePlatformArena合计保留40个GamePlatform身份；MobaPresentation保持独立两模块。
- GamePlatformArena从GamePlatform/GameModes迁入MobaCommon；新增DBAArena承接原有项目竞技Runtime／Client／Server三个模块。公共项目插件去除竞技硬依赖，竞技客户端对公共流程扩展保留单向依赖，服务器不带入DBAClient。
- 基线更新为46个代码／机制插件＋实际登记内容N；新增ContentPackRegistry及完整中文内容归属规划，当前N=0，无空内容插件和假UE资产。90个迁移文件在移动时逐项哈希一致。
- 统一10个插件描述的旧编辑器层名，修订DeveloperTools层级映射和对应引擎用例；未改变稳定模块、反射或协议身份。新增按目标装配审计与失败夹具，同步所有正式规划、根规则、规范、入口和迁移说明，旧计划保留为历史记录。
- 最终PowerShell架构回归47/47；六种公共／竞技声明装配通过。实际结构审计仍有6个既有默认配置缺失，项目流程仍有旧API引用；UE构建前置返回NotExecuted／2，未编译、Cook、联机或发布游戏。详见[三层架构实施规划](Architecture/游戏端插件三层架构实施规划.md)。

## 2026-09-27｜业务后端核心要求基线

- 新增 `Docs/Backend/业务后端核心要求.md`，统一 Go 业务控制面的领域模块化、五薄入口、跨游戏复用、UE Dedicated Server 权威边界、共享契约、数据一致性、安全、可观测与真实验收要求。
- 明确当前单团队优先采用单 Go Module（Go模块）+ 清晰领域边界，只有在独立扩缩容、故障隔离、数据所有权或发布边界明确时才增加新服务，避免无意义微服务膨胀。
- `Docs/Backend/业务服务说明.md` 增加核心基线入口；同步维护工作空间文档索引与总体目录规划说明。
- `Backend/DirectoryTree_CN_V1.1.0.md` 增加业务后端核心要求入口，确保从后端源码目录开展开发时也能直接定位现行核心基线。

## 2026-09-27｜恢复三角色并将大厅归入 OpenWorld

- 按用户最新明确要求覆盖先前四角色中间方案：正式服务端角色为 OpenWorld、Village、MainArena；大厅使用 OpenWorld 角色，`Experience.OpenWorld.Hub` 为 OpenWorld Profile 默认体验。
- `Experience.Lobby.Main` 仅作为历史兼容体验标识继续映射到 OpenWorld；不再创建或注册 `GameServer.Role.Lobby`，不保留独立 Lobby Profile。因移除已发布角色，Shared 契约提升为 2.0.0，当前兼容范围为2.x。
- 更新 Shared 真源、Go/C++ 生成物、后端注册与分配、Agones 标签、新玩家默认落点、部署 Profile、UE 角色过滤、架构规格和目录树。Go全量测试、vet、race及生成器`-check`通过；角色/Profile Pester 8/8。整体Architecture Pester尚有1项旧DBAClient模块依赖失败；真实工程结构审计为45/45插件、3/3 Target、2/8默认配置，缺少6项配置；UE构建未运行。

## 2026-09-27｜游戏端核心要求基线

- 新增 `Docs/Architecture/游戏端核心要求.md`，统一客户端与 Dedicated Server 的插件化、多项目复用、独立解耦、边界定义、独立演示、人工审核、端侧权威、三服务器角色、1v1～5v5、GAS、数据驱动和真实验收核心要求。
- 明确当前单团队开发采用“按职责/复用/端侧/生命周期/测试边界适度拆分”，禁止机械拆分空插件。
- 修正文档入口及总体目录规划中仍存在的“当前四角色”表述为现行三角色；历史变更记录中的旧阶段事实保留，不回写伪造历史。

## 2026-09-26｜设计基线整合实施（任务 1–2）

- 新增只读结构审计与 Pester 回归测试，7/7 通过。当前实际结构为37/44描述、30/40平台插件、0/4 DBA、1个工程、3个Target、2/8默认配置；审计明确报告57项差异。
- Shared契约升至1.4.0，新增Lobby角色和Lobby.Main体验；旧OpenWorld.Hub保留为兼容旧OpenWorld实例的别名、不进入活动目录。角色—体验映射由ServerCatalog生成到Go/C++，后端注册、分配、Agones标签和新玩家大厅落点已更新。
- Go 1.23.12容器内 `go test ./...`、`go vet ./...`、`go test -race ./...` 与Codegen `-check` 均通过。锁定版oapi-codegen 2.4.1无法生成现有OpenAPI 3.1规范的Go模型（其不支持规范中的nullable oneOf）；未写入客户端生成物。
- 插件目录迁移、DBA职责收敛、服务器Profile、UE编译/Cook/Stage及部署仍在后续任务中；本记录不代表整份设计基线已完成。

## 2026-09-21｜Foundation M0增量实施与原位阻断

- 按用户明确确认补齐00→03源码：正式薄主工程、Core、Data及Flow兼容扩展；旧流程公开入口保留，不创建第四个启动插件或新宿主。
- 新增显式FoundationStandalone配置、引擎内Maps/Probe/Flow生成脚本、三目标构建/开发Cook/受控进程/分项证据入口，维护项目节点与中文接口说明。
- 原位保留GamePlatformArena、MobaPresentation、DivineBeastsPresentation三个历史空描述，遵守用户“保留原位，记录构建阻断”的决定。正式Editor首次扫描退出6，未进入本批反射编译；不把历史临时宿主编译当M0验收。
- 原生算法、离线脚本及配置解析分开记录；UE三目标、真实四资产、Cook/Stage、三维与多PIE尚未通过。完整结果见FoundationM0Verification，不宣称可运行或完整游戏完成。
- 独立复核提出的数据调度、大小写身份及外部资源所有权问题纳入本批修复；源码/测试状态以执行进度和最后证据为准，不覆盖原失败记录。

## 2026-09-21｜插件编译续查

- 使用显式启用 ApplicationFlow 与 VFX、直接引用唯一正式源码的临时验证宿主，解决本次 VFX 构建入口的模块发现阻断；未改写正式游戏占位工程。
- 修正 VFX 世界子系统清理复合定时器时的只读句柄错误，补充中文说明；不改动公开接口。
- UE5.8.0 UHT 通过，ApplicationFlow／VFXClient／VFXEditor 三模块 C++ 编译通过；完整构建退出码 6，DLL 链接分别缺少引擎 Core／Projects／UnrealEd 库，未产出可加载插件 DLL。
- ApplicationFlow 原生 Debug／Release 各 21 个场景重新通过；没有执行 UE 自动化、Client／Server 构建或 Cook。同步更新 VFX 接入与验证说明。

## 2026-09-21｜历史工程参考与 ApplicationFlow

- 新增《神兽联盟历史对话来源索引》《神兽联盟历史对话与插件工程实现参考》，记录七个已读取会话的四十八轮消息、决策演变及后续插件职责。
- 新增 GamePlatformApplicationFlow 0.1.0 运行时代码、中文工程说明、原生行为测试及 UE 自动化测试代码。
- 同步维护总体规划、总体目录规划、插件规范及根规则中的参考入口；扩充现有工程文档首页。
- 本轮实际证据：生产调度核心 MSVC Debug／Release 编译成功，各运行 21 个行为场景。补充发现 F 盘 UE5.8 源码引擎并尝试独立插件验证；游戏主工程仍为空占位，不能宣称正式游戏验收。详见插件《测试与验证说明》。

## 2026-09-21

- 将根目录的普通文档迁入 `Docs/`，保留根级 `AGENTS.md` 作为工具规则入口。
- 新增后端五服务 Docker 本地开发部署、启动/停止脚本、自动配置校验和中文部署说明。
