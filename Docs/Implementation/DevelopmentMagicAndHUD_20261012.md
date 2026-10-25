# 头像、技能栏与开发魔法实施及验收记录

## 范围与职责

本轮在主分支原位增量实现，不增加插件、不修改正式英雄技能集合、不复制第三方游戏资源。

- GamePlatformCombat（平台战斗）：唯一权威直线／螺旋投射物、细分球体碰撞、一次伤害及来源代次检查。
- DBAGameplay（项目玩法）：开发魔法 Definition（数据定义）、GAS（技能系统）执行及三枚连发。非竞技世界不强制依赖 MOBA。
- DBAClient（项目客户端）：事件驱动头像／技能投影，复制事实转中立表现请求；不直接调用 Niagara，不判断伤害。
- DBAPresentationPack_Core（项目表现包）：三种魔法共六套飞行／命中特效、三份GPU模块、两份材质、六份平台定义。
- DBAUIPack_Core（项目界面包）：三张原创技能图标，保留原稿RGB，执行真实导入及GPU颜色检查。

所有新增／修改代码路径配有中文职责、权威、取消及资源生命周期说明；这不是全仓中文注释合规声明。

## 开发技能参数

| 技能 | 默认键 | 基础伤害 | 一次消耗 | 冷却 | 实际路径 |
| --- | --- | --- | --- | --- | --- |
| 火焰弹 | 2 | 40 | 气势10 | 2秒 | 1200厘米／秒直线 |
| 寒冰箭 | 3 | 30 | 气势8 | 1.5秒 | 1600厘米／秒直线 |
| 奥术飞弹 | 4 | 每枚12，共3枚 | 总气势12 | 3秒 | 1400厘米／秒、半径18厘米、3转／秒螺旋 |

射程均为轴向2000厘米。奥术每次激活只提交一次成本与冷却，三枚间隔设为0.2秒；UE定时器在帧边界执行，不承诺亚帧触发。服务器沿真实螺旋路径碰撞，客户端光带不能代替权威路径。取消清除尚未发出的弹；已发出的弹仍按来源存活和代次检查。开发数值不是正式平衡；尚无持续燃烧、冰冻、减速、追踪或完整火法体系。

仅非Shipping／Test、开发配置与 `-DBADevelopmentSkills` 同时满足时启用。12个英雄各自保留稳定技能ID，执行器和表现共用。每个开发集合的普攻／被动保留，正式 DefaultAbilitySetId 未修改。

## HUD与视觉

头像以归一化UV `(0.25,0.08)..(0.75,0.58)` 裁取当前源图头脸，不修改512×512原稿RGB。当前源图仍是开发人形，不是正式生肖肖像。

技能栏读取实际英雄授权、ASC技能及图标；空状态清除旧画刷。同步槽位事件返回后检查投影令牌，避免旧角色的图标／冷却覆盖后继状态。所有状态由既有事件驱动，不增加业务轮询Tick。

特效采用可读能量核、低频流动材质、宽柔光晕、有限重叠光带、世界空间连续尾迹及独立命中扩张环。火焰橙金、寒冰蓝白、奥术紫蓝。Niagara／材质／定义均由Monolith创建、编译、保存、独立重载；未使用已知错误的widget截图Gamma链路。

## 可复现入口与证据

- 实施计划：`Docs/Implementation/MagicHUDImplementationPlan_20261012.md`。
- 技能作者：`Tools/Unreal/Abilities/AuthorDevelopmentMagicTests.py`；真实资产门禁：`Tests/Assets/TestDevelopmentMagicAssetsMonolith.py`。
- 特效作者：`Tools/Unreal/VFX/DevelopmentMagic/AuthorDevelopmentMagicVFXMonolith.py`；精修先备份本轮专用生成目录，不覆盖用户地图。
- 服务器剥离真源：`Build/Game/ServerPresentationExclusions.json`；统一生成／检查：`Build/Game/ServerPresentationPolicy.py`。
- 本轮独占证据：`Saved/Validation/MagicHUD20261012/`；Editor构建及颜色发布门禁：`Saved/Validation/FoundationM0/`。

已实测：UI64项、平台Combat24项、项目Presentation18项、项目表现重入清理1项均通过；GPU SourceToSlate 覆盖既有9张加新增3张图标。36份真实魔法GA通过编辑器资产验证，六份已保存Niagara系统重载编译零错误零警告。这些是对应阶段的真实证据，不替代最终新版构建和Cook。

精修后最新回归：完整技能16项（977a6da5）、UI64项（c554f217）、Combat24项（8f898808）、平台Presentation5项（84464bbf）全部成功；真实RHI为6a8fef5c，Passed=true／NullRHI=false。项目Presentation18项的前述报告为5688bb27，与平台Presentation5项不是同一测试范围，不能混算。

新增真实GAS九项回归首轮 `MagicNative-ac3889fd-459f-40e2-89b8-0ef0888b136b` 为7成功、2失败。其中Spawn同步回调推进来源代次导致Active残留已定位并修正；后续 `MagicNative-483d64af-a1f3-4013-abb8-5f760a95c5ee` 为9项全部成功。另一项是测试错误要求每段0.2秒定时器在亚帧截止时必定触发；对照锁定引擎TimerManager源码后，改为读取真实开始时刻，并验证每段不少于0.2秒且最多本夹具一帧0.05秒量化，仍检查早发／迟发／多发。

架构Pester实测80项中79成功、1失败：既有DBAPluginConsolidation测试仅预期5个DBAClient模块，工程现有6个含DivineBeastsDemoClient。本轮未删除该真实模块或放宽测试，不宣称全工程门禁通过。

## 当前交付边界

完整Editor构建 `36c5de19-d88a-4e36-8b24-766ee63619f5` 在当时输入快照下Passed／Eligible；Client／Server首次构建 `1a254de9-3399-4c67-a179-5b204870242b` 中Client成功，Server在Chaos和IrisCore引擎单元遇到MSVC14.44 C1001。未修改引擎或优化配置，以MaxParallelActions=1重试；`a3edf97a-efe4-4b2a-a2e1-79908abd700e` 两个目标全部通过。此后仅新增资产边界、Build层Python审计与文档变化，没有修改已构建的UE C++；旧Editor输入快照不冒充此后全部文档/脚本的构建证据。

最终开发客户端独占Cook／Stage：`6bfc330a-f7a8-4444-b4cf-d6814445233b` Passed，实际IoStore524项必需资源齐全，涵盖新魔法、图标、六飞行／命中系统及六定义。最终专服独占Cook／Stage：`aab68dd8-9f31-4fea-be57-f1b2ee198c15` Passed；读取真实Pak、项目utoc及global.utoc共3个容器，997条资产记录、禁止目录命中0，要求的新手村地图存在。纯表现剥离检查通过不等于完整payload校验或联网通过。

最终原生报告：完整技能16项 `MagicNative-0ece6c42-2afd-4222-aa50-fa935947cbb8`，UI64项 `Native-a9b19ccc-282e-40e4-82dc-872bcc482c7b`，Combat24项 `Native-5571a73c-61db-4cf5-b1a4-7d440fc5f986`，平台Presentation5项 `Native-ee0eb45b-e4b6-4bbe-b3bd-1c8f9962267a`，项目Presentation18项 `Native-edccb5e0-4886-4c9a-a65a-0f4d9c2ca155`；均全部成功。最终GPU颜色为 `05613996-fb08-4dec-a4f1-445a1f144120` Passed／NullRHI=false。

服务器容器审计曾误将真实 `<ContainerHeader>`／`<ScriptObjects>` 当文件路径。现仅接受标记与块类型的精确配对；Unknown平台只对global容器的ScriptObjects例外，真实资源仍须正确目标并完整检查禁止目录。补充先RED再GREEN的回归，30项全部通过，随后重新Cook／审计通过。没有泛化跳过非法路径，没有修改失败报告。

新版产物已生成，但没有自动替换正在人工验证的旧客户端／Village专服。登录后网络施法、实际GPU时间演化及人工视觉验收仍未完成，不自动提交账号密码；也未新增生产或默认技能开关。

已准备独占流水线 `Saved/Validation/MagicHUD20261012/VerifyAndCookMagic.ps1`，准确消费通过的构建ID，依序执行16项完整技能、UI／Combat／平台及项目Presentation、RHI、开发客户端Cook和新手村专服Cook；任一失败立即停止，不以架构既有失败或人工未验收换取伪造全绿。首次流水线因准确检测构建失败而停止，未Cook旧程序；通过的串行重试后才重新执行发布。另一次本轮Cook在新包围盒修正前主动取消，仅停止自己的流水线，旧客户端／专服不动；取消目录不作为交付证据。

飞行系统与世界空间Trail发射器包围盒改为各轴±950厘米，覆盖1600厘米／秒×0.5秒拖尾及光带余量；命中系统维持±250厘米。作者和独立真实资产门禁同步更新。备份为 `BoundsBackup-20261012T161527233699`；重新打开编辑器后读取三系统的真实序列化边界，零错误零警告、脏包0。此项只修表现裁剪，不改变射程、服务器碰撞或伤害。

最新精修通过Monolith创建／保存、独立重载六系统零错误零警告并再次保存至脏包0。备份为 `VFXBackup-20261012T154309966832`。此前编辑器曾因未加载的已有Niagara模块保存触发“只加载部分”断言；模块原磁盘哈希与备份一致，修复作者的显式旧资产预加载后制作成功。预览只用Niagara场景，静态截图不证明GPU时间演化或实际游戏最终效果；最终视觉须在新版客户端检查。

当前任务不涉及后端代码修改、真实支付、生产发布、提交或推送。若新版独立专服与旧服共享分配条件，必须先明确路由隔离，否则不能宣称客户端已进入开发魔法服务器。

## 2026-10-24 快速恒速与VFX检查修正

用户要求提高魔法飞行速度。火球、冰箭、奥术飞弹、定向飞弹分别为6000、8000、7000、9000厘米/秒；保持5000厘米射程、服务器追踪/扫掠、原施法时长、连发间隔与伤害，不增加加速度。84个真实资产已通过Monolith保存及重载；原生GAS回归独立断言奥术弹0.05秒前进350厘米及原螺旋位移。改选目标夹具使用仍在50米内的远目标，避免旧近目标被新弹速提前命中后造成错误的存活断言。

旧开发资产检查仍要求Frostbolt占位Niagara路径，现与专用IceArrow定义合同一致，核对真实软引用及精修后的700厘米系统边界；不重定向现有定义、不删除边界检查。独立IceArrow资源门禁一并验证六系统及四个定义。

火球V3审核模块的InputMap曾被生成器设成隐藏、非必需输入。恢复暴露且必需后，刷新原八个FunctionCall，并按原GUID恢复生成/更新阶段的Age、Index、Mode输入绑定；不重建发射器、不改粒子数量、轨迹或生命周期。CustomHLSL的Index取模保持空格分隔，避免UE5.8分词错误。作者入口提前设置同一InputMap合同，防止重新制作时复发。四发射器保存、重载、强制编译均零错误零警告，并生成实际GPU HLSL。

本轮完整构建、原生回归、Client/Server Cook与真实进程身份统一记录在`Saved/Validation/VFXPlayerRelease20261024/`。原生编译与资源门禁不代替Cook GPU着色器、真实新手村/竞技场准入及双客户端画面验收；以该目录最终报告分层标记结果。

## 2026-10-25 火球与冰箭默认0.5秒施法

本次用户要求取代冰箭原2秒默认值。开发魔法与正式占位两条服务器GAS路径统一读取`UDivineBeastsDevelopmentMagicDefinition::DefaultCastDurationSeconds=0.5f`，将同一局部时长发布到既有复制Cast事实并启动首次释放Timer。火球原先只发布0.7秒反馈后立即发射，现与冰箭一样，在权威起手完成后才生成投射物。奥术首发/0.2秒连发、定向飞弹及V6独立技能时序保留，不机械覆盖其他技能。

首次释放复用既有BurstTimer与激活令牌，结束/取消统一清理；释放前检查当前Avatar、来源生命与代次、世界退出及冻结原目标，后续选中状态不能改写已校验的原目标。施法条继续消费Cast事件，不增加Widget轮询或新的表现播放器。运动、命中、伤害、成本和冷却合同不变。

源码合同`Tests/Architecture/TestMagicCastWindowContract.py`已观察两条路径失败后修正，通过6项检查（含反馈分叉、来源Avatar和清理故障注入）；它只证明当前源码合同。原生回归扩展为火球/冰箭×开发/正式四条起手测试，独立断言0.45秒无弹、0.55秒一枚、复制窗口0.5秒、取消与来源换代无晚发；另有8条参数回归覆盖来源死亡和ASC切换Avatar。当前共享Editor DLL被其他会话编辑器占用，本子任务没有启动构建、Cook、编辑器或客户端，也未运行这些新增原生用例；统一构建与原生执行证据由主任务后续记录。

本次默认时长是原生常量，已保存GA/Definition没有另一个施法时长字段，因此无需改写二进制资源来使服务器默认生效。两个既有作者脚本同步生成0.5秒说明，只在未来通过Monolith执行作者时进入真实Profile；本轮没有改写Profile、蒙太奇、地面法阵或其他VFX资产。客户端动作与现有冰箭2秒地面法阵能否正确跟随0.5秒Cast启停，仍需独立Monolith回读、客户端与人工视听验收；历史原生资产/2秒测试通过不适用于本次新默认。
