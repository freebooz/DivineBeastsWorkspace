# 施法动画、起手表现与测试怪物实施计划

> 执行方式：使用设计、测试先行、原位执行和最终独立代码审查规范。用户已要求直接执行、不再反复确认；仅在main修改，不创建分支或worktree，不自动提交、推送。

**目标：** 三种开发魔法拥有可辨认的施法动作及起手表现，并在新手村开发配置下提供一只可受伤、死亡和重生的测试怪物。

**架构：** 平台技能模块提供中立、服务器确认的施法事实；项目技能使用该事实并保留唯一GAS消耗、冷却和Combat结算。项目客户端用事件适配事实，播放项目动画和平台中立VFX请求。测试怪物在项目角色模块实现，项目专服负责开发场景装配，不强制依赖MOBA。

**工具链：** UE5.8、Monolith MCP 0.23.0、现有构建/Cook入口、原生自动化测试。

**设计基线：** 本文下一节；来源为2026-10-13用户要求“增加施法动画和技能施放效果，增加一个怪物角色”。

## 范围与设计

- 火焰弹：双手聚拢后向前释放，橙金手部能量与短促放射。
- 寒冰箭：侧身单手抬起并前推，蓝白凝结光晕与释放寒雾。
- 奥术飞弹：双手维持引导姿势，紫蓝脉冲；保留三枚真实螺旋飞弹与独立命中效果。
- 动画不计算伤害，不以可选动画/VFX是否播放成功决定GAS权威结果。保留现有瞬发和0.2秒奥术连发时序；火焰/寒冰确认表现窗口0.7秒、奥术1秒，由服务器发布起手/释放/回收事实。取消、死亡、换化身和退出清理未释放的计时器。
- 怪物暂定“远古石灵”，开发测试敌人而非正式怪物美术交付；生命300，受击/死亡由既有平台Combat/GAS结算，死亡后10秒由服务器恢复，代次推进。第一版为静止训练敌人，不承诺完整追击AI、掉落和任务系统。
- 怪物出生参考现有PlayerStart的前方空地，先检测地面和碰撞；不修改、保存或移动用户PlayerStart，也不重建地图。
- 正式/Shipping/Test关闭开发怪物。开发技能仍须配置与显式启动参数同时满足，正式英雄集合不变。
- 动画、怪物视觉和起手Niagara为第三层内容；纯视觉不进入专服。所有真实资产经Monolith创建/修改、编译、保存、重载留证。
- 保留现有未提交的Demo和UI流程修改；这些不是本轮代码，不覆盖、不重置。

## 重点验证

1. 非权威端不能发布有效施法事实或生成怪物；无效身份、非法时间及旧代次失败关闭。
2. 取消/死亡/换化身/退出撤销未释放动作，迟到动画及加载回调不能影响新世界或新施法。
3. 重复复制事实不重复播放，晚到事实只播放剩余有效阶段；新动作仅停止自己拥有的旧Montage。
4. 怪物不接收重复命中/死亡后伤害；重生推进代次，旧正目标代次的CombatSpec不能污染新一轮生命。现有弹体命中时才捕获目标代次，不能把这项测试误报为“死亡前发射、重生后才命中的所有弹体均已阻断”。
5. 实际Client/Server产物齐全，服务器纯表现剥离，双端施法与人工视觉另行验收。

## 任务1：施法事实与项目技能时序

文件：GamePlatformAbilitySystem中新增`Types/GamePlatformAbilityCastFacts.h`、`Components/GamePlatformAbilityCastComponent.h/.cpp`与`Private/Tests/GamePlatformAbilityCastTests.cpp`；项目`DivineBeastsDevelopmentMagicAbility`、Definition及GameplayCharacter接入。

接口：`PublishConfirmedCast`发布带ActivationId、AbilityId、PresentationStyleId、AvatarGeneration、服务器开始时间和有效时长的中立事实；`CancelConfirmedCast`只撤销匹配身份。客户端只订阅只读事实，不提交服务器伤害。

- [x] 先编译运行真实组件/世界回归并观察RED。
- [x] 实现有界时长、复制、取消/退出和一次完成；项目接入真实Commit及取消保护。
- [x] 原生技能及平台回归GREEN，保留成本/冷却/真实螺旋行为。

## 任务2：测试怪物与开发专服装配

文件：DivineBeastsCharactersRuntime新增`Characters/DivineBeastsDevelopmentMonster.h/.cpp`与内部测试；DBAServer新增开发怪物WorldSubsystem及内部测试。配置只改开发CustomConfig。

接口：怪物复用IGamePlatformCombatant、项目角色唯一ASC/Combat；只允许服务器初始化及重生。客户端从复制事实显示外观与生命，不授予怪物玩家身份或准入权限。

- [x] 先运行真实受伤/死亡/重生/开发关闭测试并观察RED。
- [x] 实现一只训练敌人及受控出生/重生，世界退出销毁自有对象和计时器。
- [x] 正常、缺资源、客户端拒绝、重复死亡和退出边界GREEN。

## 任务3：Monolith制作与客户端适配

文件：DBAClient/DivineBeastsPresentationClient新增开发施法/怪物表现适配；公共动画和怪物视觉归DBAContentPack_Common，起手VFX归DBAPresentationPack_Core；作者与门禁位于Tools/Unreal、Tests/Assets。

- [x] 实际资源缺失门禁RED；核对当前编辑器/服务身份与脏包，保护用户资产。
- [x] Monolith制作三份可编辑骨骼施法序列/Montage、三种起手系统和石灵视觉；为原动画图保留移动分支并添加槽位。
- [x] 客户端通过GamePlatformData租约预加载，事件驱动播放/取消，保存并独立重载资产门禁GREEN。

## 任务4：发布与验收

- [x] 全部受影响原生回归、架构检查及独立代码审查；如有既有失败，逐项如实记录。
- [x] Editor额外装配DBAServer的完整构建。
- [x] Client/Server真实构建，重新Cook开发Client和VillageServer并检查资产/纯表现剥离。
- [x] 新版本地专服及两个空白登录客户端；不自动填写密码。
- [ ] 人工双端检查起手、飞行、命中、石灵伤害/死亡/重生。

## 执行记录与边界

开始时main为8c0b1a9b；随后出现外部Demo/UI流程未提交修改，保留。前一轮已启动新版测试服和两个客户端，人工关闭两端；本轮已请求结束自己的运行所有者，不停止其他运行任务。

上轮运行证据为`Saved/Validation/MagicManual20261013/865398f6-3d93-4925-a196-bcd06a70c9cd`。其中一个客户端实际进入17778新手村，服务器角色Ready，但技能栏目视仍为旧图标，不能把上轮记为双端魔法验收通过。

首轮Editor编译阶段成功但8个DLL因暂时占用链接失败；独立核对占用解除后，fbe943d9完整Editor目标通过。真实施法RED报告`MagicNative-1b0b79eb-5203-475a-a815-b826823b23e8/Report/index.json`唯一用例明确因尚无组件类型失败。磁盘资产门禁2个用例在13个预期新资产缺失处失败。以上是测试先行证据，不是功能通过。

随后已接入中立施法事实及客户端动画/起手请求代码；新的编译、真实资产、怪物实现和人工验收仍待后续记录。

### 2026-10-13增量验证（最终构建前）

- Monolith已真实生成三套AS/Montage、上身槽位混合、三起手Niagara/Attached定义及17部件石灵装饰。姿势门禁读取Raw和Compressed，手部达到胸前且起止与实际MM_Idle衔接；不是以静止预览图宣称动画通过。
- 石灵原型只有胶囊命中、静止训练和生命/死亡/10秒重生，不承诺追击AI、掉落、任务、逐骨骼命中或最终怪物美术。客户端蹲低石体表示死亡，不称其为骨骼死亡动画。
- 名称与生命采用事件驱动的TextRender槽；Common没有UI硬引用。UIFont归DBAUIPack_Core，DBAClient使用同一Data世界租约预热BP与字体，再装配真实中文。开发UIFont为离线距离场，ASCII256加16中文共272字形；原生RemapChar/GetCharSize已核验。
- 字体来源为锁定UE的DroidSansFallback.ttf；来源TPS沿DroidSans.tps→AndroidOpenSourceProject.tps指向引擎AndroidOpenSourceProject_License.txt（Apache-2.0）。没有安装或分发Windows系统字体；正式发布仍须按项目第三方许可清单保留相关声明。
- 独立编辑器bd38a87c重开并读取动作、Montage、动画图、石灵父类、中文字体和三份定义，Niagara复编译0错误0警告并保存。摘要：`Saved/Validation/CastingMonster20261013/ColdReload-bd38a87c.json`。脏包0，未改地图或PlayerStart。
- 真实RHI颜色测试c0ee922a唯一SourceToSlate成功，五张单次编码截图齐全，覆盖最新UI输入指纹；不是Monolith widget截图或UIFont屏幕渲染验收。
- Editor+DBAServer完整构建351a4fe2通过；默认Editor不含服务器模块，已经显式额外装配，不把此前未加载的3项记成功。
- 原生Native-406e651c实际25项：19成功、6失败。失败覆盖同资产重播所有权、死亡客户端碰撞、GAS回调撤许可后重试、中文标签、同代旧生命、同步Spawn伤害重复装饰。依据这些实际RED实施最小修复，不削弱断言。
- Transform回归旧日志真实Before0/After1 Timer显示退出漏计时器；旧夹具设备在异步日志送达前移除导致读取-1，已按UE5.8公开同步Flush修夹具。最终27项入口要求真实全数成功，前置失败、缺报告、0项均失败。
- 资产存在2项、语义/配置3项、ASCII命名和设计/三层继承/目标装配静态门禁通过；它们不代替下述最新原生GREEN与Cook。
- 独立代码审查未确认额外P1/P2；生产最小修复已冻结。下一步：完整Editor重构建，27项原生与既有魔法/Combat/Presentation回归，Client/Server构建、两端干净Cook及本机空白登录人工验证。

中文审核覆盖新增公开施法契约、怪物生命接口、客户端事件/租约/计时器、服务器资格/落点/重生、作者与门禁脚本、开发配置。既有外部Demo/UI改动保持；总体目录文档原有历史冲突标记不在本功能范围内擅自合并，本轮附加目录没有改动其未知历史方案。

### 原生GREEN与最后边界

完整Editor 1f5575d8及Native-dda7c52d实际90项全部成功（其中本轮27项），含移动动画、消耗/冷却、真实螺旋弹体、Combat和表现回归。该结果早于最后新增守卫用例，不复用为最终91项证明。

独立复核又定位重生Health通知撤开发许可：Combat真实提交新活体，但项目返回失败仍保留旧死快照。模块级OnlyModules d9c89d35仅作RED诊断，真实新用例`ConfigOffDuringGASRespawnRetiresActor`在CommittedRespawn-d9583c84观察到CombatGen3/GAS300与LifeGen2/生命0/死亡，退役及僵尸清除两断言失败；未称为完整Editor验收。随后最小退役分支只销毁仍属于本Actor/World/操作及本次提交代次的对象，不回滚GAS、不影响尚未进入Reset时的初始化重试，不销毁后继。

最后写集冻结后，完整Editor构建`01729f5b-e720-4db4-a694-379e4d09295e`实际退出0；`Native-8e3c7843-e96e-4353-bf98-5a6cdd5d702b/Result.json`实际91项Success、失败0，包含本轮28项及既有63项。默认施法/怪物入口现在要求28项。结果明确保留`NetworkAcceptance=false`、`ManualVisualAccepted=false`，不以原生回归冒充双端人工验收。

直接核对UE原始`Report/index.json`：80项无告警成功、11项带告警成功、0失败、0未运行、0仍执行；所有91项`state=Success`。告警保留为证据，不隐藏：测试临时World销毁时无Context；既有GameplayCue扫描路径未指定；一次RHICore报告保留虚拟资源258GB超256GB预算。最后一项不是物理显存实测值，也不因为断言成功就宣称性能通过；需结合真实客户端运行另行评估。没有以宽松过滤排除这11项。

### Client/Server构建恢复

`cc5b1664`对Client使用UsePrecompiled，真实缺少Slate等引擎预编译清单，未完成；改为既有常规完整构建入口。`5ee00e4a`双并行遭遇MSVC编译Chaos.25的0xC0000005，属于编译器进程异常，不记为源码回归RED。核对自有UBT路径、PID和启动时刻后停止该轮，保留日志与缓存。单并行`75b31a43`的Chaos.25已真实编译成功；随后HWeb管理任务丢失，而其UBT仍继续，未并发重复启动。

观察程序未能取得已结束原编译器的ExitCode（记录为null而非0），因此拒绝继续发布；原UBT日志实际记录Succeeded、4954.80秒。独立核对原PID与其子进程均已结束、新RunId尚不存在后，重新调用完整Client/Server入口`9320a4bb-633c-4613-a725-29e3d968e0c5`形成新的真实收据；原观察记录不替代Build/result.json。取消只等待失去管理者收据的旧Cook任务，新Cook只消费恢复后完整构建成功及91项原生GREEN，不降低引擎版本、不关闭模块、不手改编译缓存、不伪造成功报告。

### 人工验收步骤（实际发布、启动后执行）

1. 两个客户端分别从空白登录页输入自己的测试账号；首次无角色进入创建，已有角色进入选择，然后进入新手村。启动不自动填密码或使用绕过登录的直连。
2. 找到出生点前方安全空地的“远古石灵”，应显示当前生命300/300；本轮不移动或保存PlayerStart。现有输入映射保持：2为火焰弹、3为寒冰箭、4为三发螺旋奥术飞弹，朝石灵释放。
3. 观察本地和另一客户端：上身施法、移动腿部不被锁死、橙金/蓝白/紫蓝起手、飞行与独立命中，以及生命值下降。瞬发保持原伤害时序；施法动作不是新增强制读条。
4. 击杀后检查死亡表现、不再受伤、10秒后恢复300生命；重复释放不能产生重复命中或重复怪物。退出地图或关闭客户端应清理自身动画、特效与装饰。
5. 上述是待执行人工标准，不把客户端窗口出现、控制面Ready、服务器生成怪物或原生测试通过写成视觉/联网施法验收通过。

### 本轮最终发布与启动证据

- 完整常规Client/Server构建`9320a4bb-633c-4613-a725-29e3d968e0c5`：两目标真实退出0；Client核验1.48秒、Server18项动作128.68秒。此前原编译器的82分钟编译日志不是该1.48秒所取代或隐瞒的工作。
- 开发Client Cook/Stage `65a7bd44-e272-4fb9-9198-3a2b3b36b80a`：真实UAT与后续门禁退出0。原524项必需依赖与本轮19项动作/起手/石灵/字体闭包均无缺失；鸟群及既有内容门禁同样通过。最新UI真实RHI颜色证据已在发布前核验。
- Village开发Server Cook/Stage `c38dc6c5-1ae4-47e4-ab05-e1a4c622bb02`：真实UAT与剥离门禁退出0；读取实际Stage和三个容器共2824条记录、997项资产，ForbiddenEntries=0。该审计不声称完整二进制载荷校验或客户端效果渲染。
- 本机人工验收启动`ce10359d-ff7d-48d3-86e6-34608d4d0ef6`：五业务入口ready均200；新专服PID65680、UDP17779，真实唯一`DevelopmentMonsterReady Generation=2 Health=300 MaxHealth=300`，位置X400/Y0/Z90厘米。不修改PlayerStart或地图。
- 在确认新Boot注册、分配和真实监听后，仅停止已核对PID/路径/启动时刻的旧自有测试服17936。未停止其他运行任务。
- 两新版Client PID54404/19384：1920×1080、60fps、无自动登录；实际日志均出现`WBP_DBA_UI_Login`输入叶节点及`AccountInput`焦点。三进程身份核对正确、Fatal/Assertion/Unhandled匹配数0。这是日志启动证据，不是界面截图或人工视觉确认。
- 启动证据：`Saved/Validation/CastingMonster20261013/Manual-ce10359d-ff7d-48d3-86e6-34608d4d0ef6/StartupVerified.json`；原生、Cook、剥离、Startup报告均明确联网施法与人工视觉尚未验收。

最终发布后仅同步本文及三份Monolith清单的审计元数据，没有修改已验证的C++、配置、脚本或引擎资产。清单更新不冒充新的编译/原生执行；今后入口若要求全文件指纹一致，须按其正常门禁重新取得证据。所有改动留在main工作区；本轮未提交或推送，未覆盖外部Demo/UI修改。

清单更新后JSON解析、资产存在2项、语义/配置3项再次通过，git diff --check退出0；换行转换提示不是已消除的全工程告警。本轮Cook更新的Game/Build/WindowsClient与WindowsServer/FileOpenOrder生成排序日志保留在未提交工作区，不视为人工源码变更或擅自清理其他文件。
