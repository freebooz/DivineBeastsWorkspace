# GamePlatform 设计审查修复执行记录

计划：`Docs/superpowers/plans/2026-09-30-gameplatform-design-remediation.md`。

执行基线：`6f25a6519c9cc3850dd612bd073a3d79c0ba130a`；分支：`codex/gameplatform-design-remediation`。用户已授权连续执行并提交远程。

## 基线证据

- 原工作树干净；创建原生独立工作树，保留原main工作区。
- ValidateDesignBaseline：退出1，18条装配诊断；435公开头、902类型、147继承边通过；62实际描述/40GP身份/46机制基线/16内容插件。
- 此处为执行前基线；后续真实检查结果见本文件更新记录。

## 执行取舍

- 用户明确要求生成计划后执行并推送，因此不重复请求计划审批。
- 按互不重叠文件并行任务2/3/4；主执行者统一拥有Data、Server、所有描述文件、全局文档、提交和推送。
- F19以成熟度和立项门禁收口；F20以文档真实状态收口。未批准的新插件能力/生产后端不冒充缺陷修复。
- 引擎动态用例与可运行Native/静态测试分开记录；未跑引擎的用例不得记录为通过。

## 任务状态

| 任务 | 状态 | 证据 |
| --- | --- | --- |
| 1 基线与计划 | 已登记 | 上述基线 |
| 2 应用/玩家 | 源码及专项回归完成，等待UE | Task2报告；14项源码回归，Input/Loading Debug及Release通过 |
| 3 世界/玩法 | 源码与补修完成，动态证据待补 | Native红绿回归；实际DBA角色租约与玩家Gate接入 |
| 4 表现 | 源码与补修完成，动态证据待补 | Native解析/预算与SFX/VFX/Surface门禁通过 |
| 5 基础/服务器/装配 | 源码及静态/Native完成，等待UE | 统一普通资源租约、签发证明、准入关闭/32KiB接收上限、117/117插件目标装配 |
| 6 全局说明 | 已完成受影响范围收口 | API/迁移/剩余成熟度；76新增文件已登记，存量全量中文审计未宣称完成 |
| 7 整体验证/提交 | 修复分支交付；UE动态验收阻断 | 72/72架构、117/117装配、Native68次；Client/Server模块编译0，Editor6与动态阻断如实列明 |


## 集成检查更新（2026-09-30）

- 架构基线最新通过：62描述/40GP/46机制/16内容；436公开头、911类型、148继承边；整合后再次运行退出0。
- 39个实际GP插件分别Client/Server/Editor声明装配：117/117通过。
- 18组Native CMake套件在Debug、Release各34个CTest用例通过（共68次执行）；AI补门禁和UI正式可复现入口最终重跑。它们验证生产策略，不替代引擎反射/生命周期。明细在Game/Saved/Reviews/NativeIntegration/counts.json。
- Architecture Pester最初70/71通过；失败是旧测试把合法已登记Village地图挂载点限制为/Game。已修改为主工程或登记内容包真实所有者检查，相关6项及Telemetry7项共13/13通过；修正后全量Architecture Pester 71/71通过，Skipped=0。
- 三个独立复核问题：UI AddWidget同步重入（表现任务补修）、AI技能激活缺少Gate（世界任务补修）、进程共享竞技预热/Adapter（主执行已改每GameMode桶并监听WorldCleanup，待编译与双世界回归）。

## UE构建边界与实际尝试

引擎锁定F:/UnrealEngine-5.8.0-release，5.8.0，Win64 Development，不更改版本。最初Editor全插件构建8783b7e1退出6：Windows260字符路径限制；UHT已执行，不能称编译通过。以临时R:映射同一独立工作树重试，没有复制正式源码。

2e93b30f与8ed94476两次全目标构建分别排出4206/4248动作；主执行按PID及精确启动时间终止自有构建（退出-1），改为明确的模块产物范围，避免把无关完整引擎重编译充当插件验收。ec4a126f在前一构建未退出时被现有引擎锁拒绝（NotExecuted退出2），没有并发UBT。

af0708e4以现有BuildFoundation新增OnlyModules、AdditionalPlugins参数启动Editor：39GP插件及DBAArena/DBAServer组合，共81个实际可达模块，NoPCH、NoSharedPCH、最大并行动作2，共551动作。OnlyModules非空构建记录必须标为“所列模块及链接依赖”，不是完整游戏目标、Cook或Stage通过。新增参数默认空，保留现有正式默认装配和全目标工作流。

## F01—F30处理矩阵

“源码修复”只表示当前实现与合同收口，不表示尚未运行的UE动态用例或生产链路通过。

| 编号 | 源码/合同处理 | 验证边界 |
| --- | --- | --- |
| F01 | 四领域HTTP循环改为Online句柄/弱所有者 | 静态回归；UE终态/释放用例已补 |
| F02 | 四领域统一Online认证/刷新/重试入口，旧构造明确迁移 | 无项目旧消费者；真实后端联调待验 |
| F03 | Navigation重复请求ID先拒绝，不覆盖旧状态 | 实际生产索引Native与UE回归 |
| F04 | Quest队列满不提前消耗幂等身份；同Revision客户端快照内容核对 | 实际生产策略Native与UE快照回归 |
| F05 | 准入Shutdown/取消先移账本、解绑再一次通知；启动失败解绑 | UE真实未发送请求所有权用例；网络在途未验 |
| F06 | Loading订阅重入返回后核Scope/代次 | 源码检查与UE SubscriberShutdown |
| F07 | Equipment EndPlay/Destroy清Grant/Port/投影并拒绝旧回调 | UE生命周期用例，后端装备Port未装配 |
| F08 | Presentation统一确定性分层评分并拒绝完全同键冲突 | 真实生产解析策略Native红绿 |
| F09 | VFX附着目标改弱引用并核所属世界 | UE附着/GC用例待运行 |
| F10 | Composite Steps强制声明RequiredDefinitions边，Data DFS拒绝环并回滚 | Editor/Runtime合同与Data既有循环用例；旧内容须迁移 |
| F11 | SFX播放失败明确终态，总活动预算计入淡出 | 生产预算Native；Concurrency拒绝UE待验 |
| F12 | UI暂失活保留租约/栈，真实移除才释放，通知释放；补AddWidget事务 | Native事务红绿；CommonUI动态待验 |
| F13 | Input只重置自有行；保存提交与已验证落盘分开 | Native/UE回归，不伪造SaveSettings结果 |
| F14 | Settings提供者工厂按GI克隆，UnsupportedScope明确拒绝 | UE两GI交错IO待验；生产Provider仍缺 |
| F15 | Data完整签发证明替代释放历史；PCG取消累计容量正确释放 | 生产终态/PCG Native；实际长会话内存待测 |
| F16 | Data普通资源租约，Character/AI/UI及DBA实际草稿/预热迁移 | 保留资产身份；真实多实例资源/GC待验 |
| F17 | VFX/SFX有界世界终态防迟到确认/预测复活 | UE乱序/取消用例与当前源码合同 |
| F18 | 补真实插件依赖，保留客户端/服务器允许列表 | 117/117声明目标装配及全局门禁 |
| F19 | 复核五个预留身份真实成熟度，独立职责/已批准后续计划 | 基线已有说明，不实现空框架、不宣称功能交付 |
| F20 | 修正虚假后端路径/迁移/Outbox/经济交付叙述 | 文档与实际代码核对；没有新增生产后端 |
| F21 | Surface修改绑定后重置发布缓存，事件刷新MPC | UE绑定回归源码，无资产变更 |
| F22 | GAS所有原生入口查询Gate；DBA玩家/服务器AI分开注入 | Native资格策略；真实技能资产/玩家ActorInfo未接通 |
| F23 | Interaction支持迟到Possession并解绑旧控制者 | UE真实控制权变更回归 |
| F24 | 准入接收阶段32KiB增量上限，拒绝超限 | 生产预算Native，真实慢响应待验 |
| F25 | Telemetry限定BeginSubmitBatch直接实参capture，保留危险正例 | 7项Pester与真实门禁通过 |
| F26 | AI Registry/Controller按Game/PIE+authority过滤Editor/commandlet | Native世界策略与UE预览世界用例 |
| F27 | Spawn生成使用安全策略返回的最终Transform | 实际生成操作Native，真实碰撞/出生UE待验 |
| F28 | Save主档/备份在读取前及句柄阶段限长 | UE超大真实文件回归待验 |
| F29 | Progression/LiveOps完整视图事件、同Revision/边界/重置通知 | 源码回归，UE本地时间边界用例待验 |
| F30 | 受影响公开合同/私有流程/测试中文说明，新增文件目录登记 | 代表性人工审查，不宣称全部存量中文注释合规 |

独立复核额外三项源码问题已补修并逐项复读确认：UI构造重入、AI Gate缺失、竞技进程状态覆盖；同Mode配置发布顺序在复核后修正。真实CommonUI/Data、Tag攻击决策与两个竞技世界12资产/出生仍需动态验收。

## NoPCH编译发现与环境阻断

af0708e4的真实编译发现AI/Equipment/DBA角色直接使用AActor却缺完整头、角色预览使用UAnimInstance却缺直接头、Settings引用不存在的Misc/LexFromString.h；均已补真实UE5.8声明头。Character/AIServer/DBA角色公开接口直接使用FGamePlatformResult，增加GamePlatformCore直接构建/插件依赖。UI编译遇到修复期间旧UHT行号宏与新头不匹配，必须在源码冻结后串行重新生成，不手改.generated.h。

同一日志还发现锁定引擎既有Engine/Source/Runtime/Online/HTTP/Private/Tests/HttpRedirectPolicyTests.cpp无法编译：调用FCurlHttpRequest私有CleanupRequest，以及当前引擎不存在的ApplicationContextMask枚举。该文件不属于本仓库；未修改引擎、没有关闭出错模块。HTTP DLL时间为2026-07-20，相关定制接口头及测试为2026-09-21，因此不能盲用旧预编译二进制验证当前接口ABI。项目模块源码问题继续整改，整Editor链接/Automation目前受此依赖阻断；最终状态以最后一次真实构建为准。

后续实际Editor重编：9dee1911（143动作，退出6）暴露独立链接所需Core/UMG/CommonUI依赖、Settings日志导出、Save const捕获移动和项目预览完整类型；a594ce48（42动作，退出6）剩一个Debug Registry UWorld头问题。均据真实日志修正；07ef856b最终重编退出6，所选项目模块不再有编译/链接诊断，剩余5条诊断全部归上述引擎HTTP测试。不能据此将整个Editor构建记为成功，也不能把没有运行的Automation记为通过。

Client/Server验证采用现有正式目标Win64 Development、NoPCH/NoSharedPCH、最大并行2；声明闭包分别为63/43个项目及平台模块（包含薄主模块），具体运行结果见最终检查表。UE单体目标的OnlyModules产物为所列模块对象文件；它不生成完整可执行程序，不证明全部引擎依赖链接、Cook或服务器表现资产剥离。该边界已由锁定UBT的UEBuildBinary.SetOutputItemsForModule实现核对。

## 独立复核与工作树边界

独立审查先发现UI同步构造重入、AI门禁与竞技跨世界共享三项P2，补修后逐项复读关闭；同Mode发布接口顺序与UI合同旧说明也已同步。最后追加只读审查Data签发/普通资源、Server接收/关闭和Creation真实租约消费者，未发现限定范围的新P1/P2；Loader同步拒绝仍可能延后通知的P3合同已纠正。上述是源码审查，不是引擎动态或真人视觉验收。

执行中原E:主工作区出现其他写入者的三个未提交VFX修改（PerformanceProfiling.md、VFXWorldSubsystem.cpp、ValidateGamePlatformVFX.ps1）。本轮没有覆盖、删除或纳入它们；独立修复分支仍从已记录6f25a651基线开始。后续若合并主线，须针对这些并行改动单独比较，不把切换分支当作覆盖授权。

## 复现入口与剩余验收

以下命令从仓库根运行；引擎路径替换为真实锁定5.8.0安装，不使用个人工作树路径生成另一份源码。

```powershell
pwsh -NoProfile -File Tests/Architecture/ValidateDesignBaseline.ps1
pwsh -NoProfile -File Tests/Architecture/ValidateProjectHeaders.ps1
Invoke-Pester -Script Tests/Architecture -PassThru
# 每个有本轮Native证据的插件使用自身Tests/CMakeLists.txt：
cmake -S Game/Plugins/GamePlatform/Foundation/GamePlatformData/Tests -B Game/Saved/Reviews/NativeIntegration/GamePlatformData
cmake --build Game/Saved/Reviews/NativeIntegration/GamePlatformData --config Debug
ctest --test-dir Game/Saved/Reviews/NativeIntegration/GamePlatformData -C Debug --output-on-failure
# 同样运行Release；AI/UI新生产策略也已用各自入口最终重跑。
```

Native范围为Core、Data、ApplicationFlow、Input、Loading、AbilitySystem、AI、Gameplay、Quest、Online、Server、Session、Presentation、SFX、UI、Navigation、PCG、World共18个插件入口。本机Native使用Visual Studio 18 2026 / MSVC19.51，UE使用锁定工具链MSVC14.44，未把Native工具链与UE链接验收混为一谈。

UE复现仍使用 `Build/Game/BuildFoundation.ps1`：显式指定Editor/Client/Server、EngineRoot、NoPCH、MaxParallelActions=2；AdditionalPlugins为对应端侧有真实模块的39/37/26个GP插件，Editor再加DBAArena/DBAServer，Client加DBAArena/DBAClient，Server加DBAArena/DBAServer。OnlyModules取 `Test-PluginComposition` 返回的ReachableModules并加薄主模块DivineBeastsArena。运行时记录的完整参数保存在各RunId的Build/result.json及UBT.log，不靠旧二进制/旧报告凑通过数。

必须后续补证：修复锁定引擎HTTP依赖后完成Editor构建及本轮Private/Tests动态用例；真实多GI/多World资源驻留与GC、CommonUI构造重入、AI真实技能Tag链、玩家ActorInfo接线、竞技预热/出生、HTTP慢响应/超限/卸载；三角色与五模式联机、干净Cook/Stage及服务器表现代码/资产剥离、设备和人工视觉验收。本轮没有改UI资产，没有新增生产后端，五个预留身份与Settings生产Provider的成熟度限制仍存在。

## 最终检查表（提交前）

| 检查 | 实际结果与产物边界 |
| --- | --- |
| ValidateDesignBaseline / Inheritance | 退出0；62描述/40GP身份/46机制/16内容，436公开头/911类型/148继承边 |
| 每个GP插件三目标声明装配 | 117/117，退出0；不检查引擎安装或资产硬引用 |
| Architecture Pester | 最终72/72，Skipped=0，退出0；先前71项中的CommonUI误报已按真实引擎身份修正，并新增客户端直接链接/服务器隔离用例，其他未知外部身份仍拒绝 |
| 自有头文件引用 | 470处，缺失0，退出0；真实UE完整类型问题由编译另行发现并修正 |
| Save/Settings/Telemetry/Server/SFX/VFX/Surface专项 | 7个源码/布局门禁均退出0；Settings仍明确无生产Provider，VFX实际观察资产0 |
| Native CMake / CTest | 18入口，Debug/Release各34个用例，共68次执行通过；没有用Native代替UE生命周期 |
| Editor最终模块构建 | 07ef856b-2ef7-4324-a0ad-8f0f1370c838，退出6；选定81模块的项目编译/链接诊断清零，剩余5条均为既有引擎HTTP测试；目标未通过 |
| Server模块编译 | b4f08384-5a64-47a4-abf6-e85a8c9201eb，Win64 Development，43个模块/116动作，退出0；单体目标只生成选定模块对象，不证明可执行程序链接 |
| Client模块编译 | 70b77617-b26e-473f-b9b6-097ccc0fc2b6，Win64 Development，63个模块/170动作，退出0；同样不证明整游戏链接、Cook或运行 |
| UE Automation / 真实资源 / 联机 / Cook / Stage | 未执行；Editor前置失败，相关真实Fixture与完整业务接线亦需补证；没有复用旧报告 |
| 文件与差异 | 76新增文件全部在总体目录规划登记；未生成/提交二进制资产、Saved日志或Intermediates；最终diff检查退出0 |

本表只确认本轮已执行的检查，不提升五个预留插件、后端、玩家ActorInfo、设置生产Provider、端到端世界/竞技及存量中文注释的成熟度。

## 2026-10-09 主分支整合复核

用户要求所有修改统一留在main。平台整改分支的内容已与近期登录、角色预览、战斗UI和Village行走接线共同整合；保留现有稳定身份、真实资产和三层依赖。主分支内容提交与正式分支祖先关系分别核对，合并记录不得通过重置或强推改写已有主线。

本次人工审查覆盖20处冲突及其调用路径：插件直接依赖、公开技能事件与激活资格、预览完整类型包含、SFX预算/终态、CommonUI构造重入与Data租约、服务器地图登记，以及对应中文合同和目录增量。界面平台变体的一次默认回退保留，但两代请求均使用Data租约；新租约接替后释放本调用者旧需求，旧完成不能提交新页面。真实编辑器回归发现LocalPlayer夹具违反ClassWithin约束，已改用真实Engine作为Outer，并保留取消期间构造租约的验证。补齐DBAClient对Gameplay/World的真实插件声明；旧模块清单补回既有DivineBeastsAbilitiesRuntime身份。上述范围不是全库历史中文注释合规宣称。

本次使用D:/UnrealEngine-5.8.0-release，UE5.8.0、Win64 Development。以下均为本次实际执行，不复用2026-09-30的结果：

| 检查 | 结果与独立证据 |
| --- | --- |
| Architecture Pester | 73/73，Failed=0、Skipped=0；Saved/Validation/VillageFlow/20261009/MainMerge.Architecture.json |
| 三层公开接口边界、英文命名 | 480公开头、992类型、177继承边通过；62活动插件、路径违规0；MainMerge.EnglishNames.json |
| 平台Native策略 | 18入口、Debug/Release共68次测试执行，失败0；本机VS2022，MainMergeNative/Results.json |
| Editor项目模块 | 53模块编译/链接退出0；FoundationM0/2f0062cf-5859-4657-bbc5-44afd759ee7f/Build-ProjectEditorModules；不冒充完整引擎Editor构建 |
| 正式Client目标 | 完整编译/链接退出0；FoundationM0/9353c867-dd07-4f17-9d4c-c2b9e8d035d7/Build；使用NoSharedPCH |
| 正式Server目标 | 完整编译/链接退出0；FoundationM0/1d891c5c-6f05-47e4-a63f-d8ce48ac5aef/Build；未传NoSharedPCH，含实际引擎依赖构建 |
| 真实UE Automation | CommonUI暂停保留资源、同步取消撤回新页面、错误路由、鼠标预览生命周期共4/4，错误和警告均0；MainMerge.Automation.json |
| 客户端Cook/Stage及最终IoStore | 退出0；25项必需资产缺失0，含新Pawn/体验定义；FoundationM0/530b9825-a2e3-4d92-96bd-fc52e9d5f426/Cook-FrontEndClient |
| Village Server Cook/Stage | 退出0；FoundationM0/18e90b7d-dc32-4aa0-86b4-ecdc32f92363/Cook-VillageServer；保留旧包供回退 |

FoundationM0路径均位于工作空间Saved/Validation；资产打包检查不等于新手村网络流程完成。双客户端本次手动登录、创建后返回选择、真实WorldReady及行走尚待人工验证，服务器表现资产剥离和三角色五模式联机也未在本表验收。

## 2026-10-09 登录后黑屏定位与修复

本次两个正常客户端由用户手动完成登录，再从角色选择进入Village时黑屏。日志与实际资产确认：教学体验Purpose为空导致MissingExperienceContext/ExperienceLeaseFailed；注册完成即Ready过早；平台FindPlayerStart在UE InitNewPlayer查询Controller初始位置时返回空，引擎以Could not find a starting spot拒绝Login；LoadingTravel/ErrorReconnect规划路径没有真实资产，World销毁还会移除仍被引用的RootViewport。

增量修复真实教学体验定义、Village Ready双完成门禁与失败排空；UE控制器初始位置查询不生成Pawn、不修改资格，真正出生仍受准入/候选/资源/Active约束。Monolith创建并保存两张必要页面，固定字号和控件尺寸，只消费ViewModel事件和既有Retry命令；平台Root按LocalPlayer跨图挂载，取消事件清理上层Opening身份，新控制器重新消费已有快照。无自动登录，无伪造WorldReady，无新增插件或模块。

真实UE回归先复现MissingExperienceContext、两页无法加载以及登录位置查询失败，修复后玩法5、平台UI12、世界3、项目UI14项均通过。扩大测试发现并修正既有LocalPlayer ClassWithin/AttributeSet外层夹具；新增测试夹具曾二次初始化World导致测试编辑器退出，已删除重复初始化，实际GameMode位置与出生禁止回归通过。Gameplay与PlayerStatus瞬态世界夹具共5条非失败警告保留。Architecture 73/73、英文路径违规0、源差异空白检查通过；不宣称全库中文说明已合规。

锁定UE5.8 Win64 Development：Editor项目53模块最终退出0（FoundationM0/7c5ffa91-1ba6-4733-acd8-b813c1afdd01/Build-ProjectEditorModules），完整Server目标退出0（dd4f04f8-7a25-47fd-9fef-23c8402e9ff4/Build），完整Client目标退出0（8dcb5a43-3c4c-410b-9131-e7f372ad05ce/Build）。Village Server新Cook/Stage退出0（51f863bb-00b2-4afb-817b-b55ebb606bc2/Cook-VillageServer）。启动新专服后实际Gameplay日志Stage=2/Active、Error=None，并有持续控制面心跳；这证明专服体验可用，不代替两个客户端准入和行走。

全部记录位于Saved/Validation/VillageFlow/20261009及FoundationM0。旧制品和日志保留用于回退，后端五服务沿用现有健康实例，没有重置账号与角色数据。客户端最终Cook与人工登录后WorldReady/行走仍须继续实测，不能由上述编译或服务端事实推导通过。

本轮最终客户端Cook/Stage及27项IoStore资源检查退出0，缺失0：FoundationM0/e738d0ca-f9eb-46f1-b329-e8a5bcc85dbd/Cook-FrontEndClient。错误页已补齐中文Tooltip和六向Wrap导航，Monolith可访问性检查错误/警告均0。新专服PID54368、两个正常客户端PID7808/64544分别记在BlackScreen.Server.json与BlackScreen.Clients.json；两个首屏日志均激活WBP_DBA_UI_Login，无自动登录参数或密码注入。运行时屏幕检查遇到Windows锁屏，依电脑操作规则暂停UI输入，等待用户解锁及手动登录。此时尚未确认本轮双客户端WorldReady/行走，不能标记端到端验收完成。
