# 执行台账 — 计划：Docs/Architecture/GamePlatform插件整改执行计划_2026-10-09.md

日期2026-10-09；原main=8a12bbe1d1d02db835b92207646138fabe782b50且干净。已有managed worktree复用，新分支codex/gameplatform-audit-fixes-20261009；旧codex/gameplatform-design-remediation分支与554ee8a保留。

- Task 1: in progress；原审查72项Architecture 69通过3失败/4独立缺依赖将本轮复现；没有沿用历史成功。
- Pre-flight: T2/T4/T5共同消费T6 Data/Online公开接口；先合入历史稳定接口，再按归属独立修改。DBAClient Presentation两模块归T4，Application/Input/UI归T5，根规划/描述/Build入口统一由根处理，禁止跨域回滚。
- Ruling: 用户已明确授权“生成计划然后执行”，不再等待技能默认的计划确认。成本：如用户后续调整范围，按独立分支提交可回退。
- Ruling: 复用本会话已有隔离worktree并从当前main新建分支，不新建目录、不切换原main。成本：旧Saved缓存不作为本轮证据，所有验证使用独占RunId。
- Ruling: 554ee8a作为历史代码三方整合，保留当前main资产/输入/命名变更；每个本轮审查ID重新核对。成本：合并冲突与接口漂移需重编验证。
- Ruling: 五模式规则/真实资源/生产合同缺失不改固定成功，已向用户询问批准配置路径，独立代码修复继续。成本：完整产品验收仍需材料与真实联机。

- Task 1: 三方冲突4文件已逐段整合：目录规划保留两段新增历史/文件表；预览保留当前动画/拖拽与旧完整头；Input README两方说明并存；ServerRole测试保留当前严格世界前缀并补内容登记核对。
- Task 1: RED baseline Architecture=72/69/3；GREEN integrated Architecture=73/73/0（新增现有main世界挂载负例与旧回归并存）；结构六装配/继承退出0，头文件482处0缺失。Native及UE在Task8源码冻结后统一执行，未沿用旧成功。
- Task 1: Ruling: 历史合入未引入.uasset/.umap、Backend/Shared/Deploy变更，当前资源与英文命名保持main；Native测试与当前资产/业务差异分别验收。成本：仍需后续源码/编译复核。

- Task 2–7: 第一轮源码整改及领域报告已归档；根新增实际主工程Server装配回归，RED包含DBAClient，修正13个纯表现根/依赖和Stage后GREEN。Data维护改为按需0.25秒，Telemetry响应预算原生RED→GREEN，Server世界退出及准入MatchId投影/排空分离已落。
- Task 6: Ruling: 正常Drain只停止新准入并取消未发布握手，保留既有Verified玩家；World退出才ResetTarget撤销全部。本地停止先于控制面确认，Register/Ready重试所有权先于同步通知发布。成本：新增StopAcceptingAdmissions接口及关闭栅栏须UE回归。
- Task 6: 发现DeveloperTools两个入口都使用过期DefinitionId/类名猜测：聚合审计及实际Editor Validator已统一真实DefinitionBase/LogicalId/PrimaryAssetId/DataVersion，图遍历改显式栈；新测试同时覆盖两个入口，未执行UE测试。性能场景执行器仍缺真实批准场景，不造测量。
- Task 8: 根6个CMake入口Debug/Release配置/构建/CTest共12组退出0；另外3领域的本轮原生报告需按完整日志统一汇总，未使用历史数字。
- Task 8: 两个新的独立只读复核座位首次分别发现7与9项Important，无确认Critical。已纳入修复：Quest按任务重放/完整快照/同步注销、Navigation旧句柄、Interaction跨会话终态、Server重试/排空、Settings异步拓扑/同步Save/关闭、五领域关闭、Inventory恢复、Equipment候选授予、HTTP严格数值/集合及请求终态、Editor Validator真实入口。源码未达到集成结论前继续整改，不把“首次冻结”计为最终完成。
- Task 8: 表现领域第三个独立只读复核进行中；统一UE5.8构建等待上述源码再次冻结。没有执行资产修改或UE运行/Cook，也没有向main合并或推送。
- Task 8: 第三组首轮复核确认8项Important（首轮三组共24项）；Data抽象约束由根修，表现领域执行剩余7项。第二轮玩法/服务器复核确认首轮7项原触发链已修，又指出Quest编译、Stop通知升级Reset、死亡统计结束重入三项；前两项根已修并经复核确认，最后一项由原领域实现者补只读所有权和Timer核验。当前PreBuild Architecture=74/74/0；三个结构门禁退出0。
- Task 8: FinalNative在当前24个真实CMake入口完成Debug/Release，共48配置组、41独立注册用例、82次CTest测试执行，所有配置/构建/CTest退出0；完整命令输出在本轮Saved/FinalNative，正式结果表NativeValidationResults.json。未将此前失败、重复执行或UE测试源码计为通过。Application全组再次冻结后进入第二轮定点只读复核；新增ABI须统一重编。
- Task 8: 第一Server构建参数生成误将DBAClient作为额外验证根，虽真实Server默认闭包19插件/23模块不含公共客户端，强制测试参数违反本轮端侧要求。已终止本次唯一受控UBT PID，保留失败尝试ExitCode=1；没有称该次通过。DBAClient五模块显式Game/Client/Editor允许列表，重新从真实宿主/列表生成集合并加入禁止公共客户端断言；后续重新执行，实际生产闭包与全端侧模块审查仍分开。Editor外部Monolith插件不在本仓库静态图，保持真实工程启用状态由UBT处理，不删除/禁用它。
- Task 8: 修正集合后的实际Server/Win64/Development、NoPCH/NoSharedPCH、45模块UBT退出6（外层pwsh封装退出1，原始result.json以UBT6为准），159.83秒未超时；编译暴露自有Timer可变引用、TObjectPtr推导、测试SharedPtr/枚举、lambda成员访问、明确头与Telemetry UniquePtr完整类型问题。按域修正不删除测试/降告警；Telemetry新增外置构造/热重载构造/析构保持私有策略。失败日志保留待增量重跑。
- Task 8: 三组最后定点只读复核均未发现其已修调用链尚有Important：玩法最后死亡结束/旧Adapter链，应用最后准备失败/监听器接管链，表现8链。结论仅源码，UE用例尚未执行。最终Architecture=75/75/0（新增公共客户端Runtime强制启用的Server模块限制回归）；新增62行当前矩阵和61原始审查ID完整处置表。真实构建错误继续整改，不将复核关闭当作编译通过。
- Task 6/8: 额外窄复核确认Telemetry自定义Sink同步重入Important：Start通知关闭后可重新装Sink、Shutdown内重配/记录可留下Ticker、GetHealth换代后旧刷新向已关闭Sink投递。不是假设网络回调。已交明确的新责任组修closing/生命周期/实际Sink代次及真实公开接口回归；统一重跑构建等此修冻结。三个原领域的定点关闭仍只覆盖此前确认链。

- Task 6/8: Telemetry生命周期三条自定义Sink链已修并独立复核；复核又确认Sink换代错误取消EndSession/BeginSession/Travel的上下文边界，已用独立ContextOperationGeneration修复，最终只读窄复核未发现对应未解决Critical/Important。八个真实Subsystem/可重入Sink回归已写入Private/Tests，源码哈希匹配，尚未执行UE Automation。Native策略未变化，不重复累计此前82次。

- Task 8: 冻结后合法Server模块增量重试9d638e88-541f-4e13-84df-11f2bdc326d4，UE5.8.0/Win64/Development、45模块/33额外根、NoPCH/NoSharedPCH、Max2，UBT退出0、39.08秒；TelemetryLifecycleTests.cpp真实编译（12/14动作），八个用例未运行。不是完整可执行服务器、Cook、联机或三角色验收。
- Task 8: Client首次680d1f5e-f1e9-4c26-84fe-d5300709968f退出6，四条诊断统一来自Presentation公开头缺TSubclassOf完整定义；显式补Templates/SubclassOf.h及中文调用前提。客户端重试29982db5-e57e-4e85-8a49-2e028bd93303排队，保留首次失败日志，不删除测试/改变注册行为。
- Task 8: 最新Pester3.4.0 Architecture=75/75/0，四结构门禁均退出0；自有头531处/0缺失，Public边界440头/933类型/153边，命名56440项/62插件/0违规。FinalNative仍为24真实CMake入口、48配置、41独立测试/82执行全部退出0；追加的UObject回归未冒充原生通过。
- Task 7/8: FindingDisposition保留61项，CurrentPluginReviewMatrix保留62插件逐行修改前设计/性能/规范及修后处置，已把APP英文状态补为中文，并关闭根已完成的Telemetry/F16源码记录。全部原审查ID存在；缺批准资源/合同、性能实测、全量中文历史仍未计通过。
- Task 8: Foundation补UBT-WaitMutex（等待计入原超时且仅清理自有进程），可选UsePrecompiled默认false并记录mode。Editor项目模块验收将显式复用锁定SDK，保留原默认源码构建工作流，不以该模式证明引擎源码或完整程序通过。
- Scope: 后续60枚生肖图标是另行授权的任务，在原main英雄内容插件写图源并通过Monolith接入；main同时已有其他任务未提交源码，未被本审查整树覆盖或暂存。本分支审查仍为8a12bbe合入历史修复后的82模块基线，原main当前新增模块不被混计。

- Task 8: BuildFoundation新增互斥等待/预编译模式后执行现有Tests/Foundation/Scripts/TestFoundationScripts.ps1，退出0，28通过/0失败；证据ScriptTests-72812416-c1c1-4366-906f-c7d68ccdcb51。验证真实退出码、未执行、超时和进程所有权等脚本行为，不计为UE测试通过。独立窄复核对照本地锁定UBT源码未发现两处编译追加的Critical/Important。

- Task 8: Client增量29982db5-e57e-4e85-8a49-2e028bd93303实际退出6，Presentation完整类型修正相关文件已编译通过，新增两条VFX UWorld弱引用诊断分别定位到实例账本cpp的直接包含缺失及私有Provider头的内联构造。已补Engine/World.h并外置Provider构造，不改运行合同；相同65模块/59额外根的24ad5af1-0b56-4829-8d64-166cf78227c5再重试。等待期间不计通过，不重复原生测试。

- Task 8: Client第三次24ad5af1-0b56-4829-8d64-166cf78227c5真实退出0，UE5.8.0/Win64/Development、65模块/59额外根、NoPCH/NoSharedPCH、Max2、UsePrecompiled=false；1338.02秒包含等待其他Main完整Editor构建的全局锁，实际4个增量编译动作均成功。完整模块集合先前已真实编译，三次失败/成功分别保留；这不是客户端完整可执行程序、UE Automation或Cook通过。

- Task 8: Editor真实启动7b8fb33b-b859-4582-b4a0-47a7c13fb621，83模块/62额外根、UE5.8.0/Win64/Development、NoPCH/NoSharedPCH、Max2、UsePrecompiled=true，复用已恢复的锁定SDK二进制，仅编译本隔离分支项目模块/依赖。未启动第二个Editor，不占用Main Monolith资产服务；进行中不计通过。

- Task 8: 首次Editor在191.62秒退出6，准备动作图阶段两条真实第一包含规则错误：Telemetry两个既有cpp文件名仍匹配旧Public不透明头，却首先包含新Private完整头。已先包含对应同名Public兼容合同、再包含Private完整定义并补中文职责/线程说明；未改类名、源文件身份或缓冲/Schema算法。待最新Monolith升级工具链稳定后以独立RunId重试，原结果保留；没有启动源码编译动作或引擎实例。

- Task 8: Editor首包含修正已获独立只读复核无Critical/Important；重试aaf5dd3a-fae8-41d2-9940-c82ed77a100a真实UBT退出6、49.27秒，原因是ActionGraph拒绝260字符以上路径，尚未进入C++编译。临时未占用K盘映射同一隔离根，未复制/迁移源码，83模块/62根的6b3cdf34-b27a-4c83-8831-87c11a8f2b99再次真实编译，407动作进行中；映射完成后仅撤销本次虚拟路径，不改系统长路径策略。

- Task 8: 短路径Editor6b3cdf34真实407动作、790.88秒、UBT退出6；无C++编译器错误，实际DLL链接发现PresentationClient/SurfaceClient缺直接Core库、ArenaServer缺MobaCore/GAS库，以及SDK三个真实导入库缺失。已按公开/私有使用补三个Build.cs与两插件描述，并获独立窄复核无Critical/Important；SDK NetCore/ApplicationCore/ContentBrowser均按已有lib.rsp实际lib.exe恢复0，不手改SDK源码/DLL。新增fef060dd-9908-4d53-968c-2b1e852608f8同集合重试，前次结果保留；短路径清理中的单结果标量已修并核映射身份后撤销原K映射。
- Task 8: Editor重试fef060dd实际退出6、437.03秒，新增依赖的Core/GAS/准入合同链接已成功；剩余两符号真实属于MobaData五模式实现，已补Server模块Private依赖，SDK GameplayDebugger/ToolMenus缺库按原始响应生产者恢复。按83模块实际DLL响应文件预检SDK输入39项现均存在；首次预检误把系统LIB搜索名advapi32解析成源码路径并停止，已修正无目录系统库过滤，未伪造库。Architecture首次74/75/1源于项目测试只认识CommonUI内置身份，已依据真实UE5.8 GameplayAbilities.uplugin补限定身份并保留其他未知依赖失败；重新执行中。下一次Editor89e1c40a进行中，不计通过。

- Task 8: 89e1c40a相同Editor83模块真实退出0、48.96秒；最后MobaData模式实现依赖和SDK库已闭合。Architecture修正后75/75/0，DesignBaseline/531头/440公开头边界门禁均0。英文命名首次发现本任务已通过脚本回归留下的Unicode参数专用临时夹具，核绝对白名单后移至本任务Temp保留且逐文件SHA不变，原28回归日志保留；重验57673项/62插件/0违规。
- Task 8: 真实Automation首次进程29108退出1/208.42秒，OnlyModules不生成原生UnrealEditor.modules，主模块无法发现，尚未运行用例。665f84cb完整Editor目标（不指定OnlyModules、SDK预编译）真实退出0/33.44秒，原生生成cf BuildId模块索引，不手改metadata。第二Automation32180命令重复Automation前缀被实际解析器拒绝，无测试队列，按PID/启动时间/隔离项目停止并保留-1；已据锁定AutomationCommandLine.cpp改为单个前缀和分号子命令，第三次136精确批次进行中，不计通过。
- Task 8: 依赖补齐后Server45复验baf71a45真实0/28.18秒；Client首个8893e42b因同入口Foundation锁占用返回NotExecuted2，未编译，待Editor完整目标结束后顺序82c91aeb真实0/68.51秒。未停止其他Main构建/编辑器。

- Task 8: 第三Automation进程45632采用正确命令，实际发现134项（预期136），首个死亡桥用例开始后在72.66秒退出3，0项完成。真实调用栈显示测试在CreateWorld后再次InitializeNewWorld，重复创建固定名WorldSettings；锁定World.cpp2817—2850确认CreateWorld已同步初始化。14个测试文件、18处调用改为把原InitializationValues一次传入CreateWorld第7参数，保留全部原参数、断言、清理及GameInstance顺序，不用SkipInit掩盖，不改生产行为。修后真实编译和Automation仍须重跑。
- Task 8: 134/136缺项另有注册根因：Commerce、Entitlement、Progression、LiveOps四文件复用FDomainTransportOwnershipTest。AutomationTest宏以C++类名注册，Core按键first-wins，UE日志三条明确拒绝注册。默认清单中的Entitlement/Progression因此缺失；不能删除预期项。修复限四宏类及RunTest限定名唯一化，保留PrettyName、标志和原测试逻辑，重编后须核136项实际发现与四个OnlineOwnership各自可见。
- Task 8: e9a54c64完整Editor目标实际退出6，测试源码编译完成，SDK六份中间导入库缺失导致LNK1181；按原始lib.rsp和既有对象恢复6库，39实际SDK链接输入齐全，不改SDK源码/DLL。58c3c771增量完整Editor目标实际退出0，单次世界初始化和四唯一注册名已编译；后继WorldFixtureRetry实际发现完整136项，首个死亡桥因RespawnTimers.FindChecked缺键再退出3，118.06秒、0项完成，不计通过。
- Task 8: 回读锁定TimerManager确定死亡桥夹具首次Tick只激活Pending、同GFrameCounter后续Tick直接返回。测试跨真实Automation帧驱动，保持3条路径/期望并把FindChecked改安全失败；未证明生产Adapter有此缺陷，不改生产。VFX测试在ClientOnly定义Native标签触发非Fatal ensure，改用既有配置Presentation.Test并缺失明确失败，不新增生产标签或改变宿主。
- Task 8: OtherCasesDiagnostic保留133项及临时排除的3死亡桥项，启动仍在模块加载/Native标签ensure时已收到新冻结源码，严格核PID39656/UTC启动时间/工程后停止本独占实例；进程-1、781.60秒、0项开始，不能算133项运行。只读被动线程快照与完整原日志保留，未停止Main Editor。
- Task 8: fc4d3029在等待其他工程UBT锁时独立复核新增Important：latent中止只析构命令，不执行Update；原测试可能留rooted World及已释放接口。根按自有PID49280/启动时间/工程严格停止待锁构建，保留-1与未编译事实；交原单文件实现者补共享幂等fixture owner和析构清理，不计为编译器失败或通过。回读Root K映射已撤销。
- Task 8: RuntimeFix结构门禁均退出0，531自有头引用、440公开头/933类型/153边不变；命名57785项/62插件/0违规。Pester第一次输出重定向丢失PassThru对象，虽原输出为75/75/0，没有结果JSON，未凭空生成；改为捕获真实对象并重验，RuntimeFixArchitectureRetry.json实际75/75/0。原生策略未变化，仍保留此前82次结果，不重复累加。

- Task 8: f9483e8a测试源码真实编译后SDK导入库缺失，恢复后731b6219完整Editor目标退出0。SDK旧lib.exe包装调用曾无进度；严格核对本任务PID后停止并保留诊断，改用同工具链link.exe /LIB与原始响应/对象，恢复实际输入，不伪造库或修改引擎源码/DLL。
- Task 8: ActualEditorAutomationFinalFixture真实发现136项，完成59项（53成功、6失败），第60项Equipment因TArray自引用Add断言退出3。六失败涉及死亡桥3、Pawn死亡通知、项目表现LocalPlayer Outer及竞技重连。完整失败/堆栈保留，不把剩余77项记作运行。
- Task 8: Equipment先复制元素再Add，Input及六个LocalPlayer/Viewport夹具使用真实Engine Outer；两组Actor夹具按锁定引擎初始化并跨帧推进Timer，保留原断言和RAII取消清理。六文件独立审查发现Moba双向Controller拥有缺失Important，根用SetPlayer修复后复核无剩余Important；三Gameplay夹具的独立追加复核进行中。
- Task 8: c48b6a2e待锁构建在上述Moba修复前按自有PID严格停止，保留-1。2c00dab3完整Editor编译无C++错误，26处链接缺SDK中间库而退出6；随后按原始响应真实恢复12份缺库、39项输入齐全。尚未据此宣告最终UE通过。
- Integration ruling: 用户2026-10-09明确要求提交到主分支。将先保存本整改检查点，在隔离工作树整合最新origin/main和本地main已提交内容，复核冲突及真实验证后正常推送main。原main工作区其他任务的暂存/未提交文件不纳入整改检查点、不重置或覆盖；不强制推送。

- Integration: 主线范围已增至83个插件模块＋主模块84，保留真实DivineBeastsAbilitiesRuntime。ApplicationFlow真实生产核心按当前合并源在Debug/Release各1次CTest退出0；初次Ninja工具缺失、长路径VS配置失败原输出均保留，改用既有VS17/x64及本任务短Temp输出，不降版本/删测试。
- Integration: 当前主线目录文档本身已有历史嵌套冲突标记，本轮将三组独立追加段完整合并并删除标记，源码登记仍置末尾；未丢失登录等待、UI计划、图标或整改登记。机器台账JSON按ASCII规则仅迁路径、同步六引用，逐字节SHA不变，263项工具静态验证退出0；不是263个Widget交付。
