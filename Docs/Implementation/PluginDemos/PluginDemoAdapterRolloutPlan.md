# SSLM 逐插件演示接入实施计划（方案 A）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> 用户已选择Native；按 `superpowers:executing-plans` 执行。当前为计划文档，所有实施步骤未执行。

**Goal:** 将每个真实插件已审计的功能接入独立菜单、可复用场景及人工核验，不把目录登记当功能交付。

**Architecture:** 消费核心计划已定义的目录、案例适配、资源租约和证据合同；项目层组合真实公开能力。涉及竞技的接线只在竞技项目客户端，平台最小配置使用中立数据。

**Tech Stack:** UE5.8、项目既有C++与定义资产、Monolith、现有服务器测试配置、核心计划的Python/PowerShell门禁。

**Spec:** [已通过规格](PluginDemoCenterSpecification.md)；依赖[核心闭环计划](PluginDemoCenterImplementationPlan.md)的T01—T13。日期2026-10-13；计划待审阅。

## Global Constraints（全局约束）

- 唯一正式工程 `Game/DivineBeastsArena.uproject`；依赖方向 `DivineBeasts → MobaCommon → GamePlatform`，项目可以直接依赖平台。
- 当前明确限制仍为不构建。未加载新反射类时不制作依赖它的蓝图，不启动自动编译，不执行Cook/Stage。
- 所有项目UI资产必须经Monolith；新增资源归真实所有者；内容包不是第四逻辑层。
- 禁止 Monolith 0.23.0 的 `capture_scene_preview` 的 `widget` 路径；按规格验证原稿到RHI链路。
- 自动化不能代签人工通过；原始失败/取消、历史结论、版本和证据不得被覆盖。
- 不引入生产账号/密钥/支付，不直接修改权威背包余额；复位仅限案例所有权或业务合法补偿。
- 不重建缺失业务功能来让演示成功；缺能力登记到所属插件整改项，菜单持续显示阻塞。
- Native串行修改共享目录、适配注册、Build.cs、Config和Monolith资产；保留并行任务修改，不自动提交或推送。

## Review Focus（重点复核风险）

1. README宣称有功能而调用链尚缺：逐项记录源码/资源证据，未实现不得进入可运行集合；R01/R06门禁。
2. 一个组合场景同时涉及多个插件：逐插件独立检查，不自动传递通过结论；R01/R03用例。
3. 调整真实库存/权益后误“复位”：隔离账号/命令、合法补偿，不能覆盖持久业务状态；R04用例。
4. 竞技模块加载顺序与关闭MOBA：基础演示仍可运行，注册注销按实例/代次；R05用例。
5. 内容包可见但资源缺失/授权不明：真实加载与消费者证据、资产许可和所有权分别核对；R06用例。

---

## 1. 接入前提与功能级工作单

核心T01—T08代码合同稳定后可进行只读审计与源码接线；核心T09—T13真实样板闭环之前，不标记任一批“可运行交付”。缺新构建时允许提交源码级成果说明，但不运行旧DLL代验。

每项独立功能必须在现有 `Docs/Implementation/PluginDemos/PluginDemoManifest.json` 的v2源中形成工作单，包含：稳定Feature/Scenario/ReviewCase ID、真实源码路径与公开方法签名、所属插件和端侧、资产身份/消费者、正常/失败/取消/清理输入与预期、参数范围来源、文件责任、复位边界和证据类型。`FeatureCoverage.md`、`AssetReuseMap.md`只从其导出。

接口消费：核心计划T04的 `IDivineBeastsDemoScenarioAdapter::Prepare/Execute/Stop`、`FDivineBeastsDemoRunToken`、注册完整句柄；目录/证据分别消费T03/T08，所有适配结果进入同一状态机。不存在公开接口时不得访问Private头或复制实现：登记阻塞，提出原插件最小稳定接口增量并独立审查。

每项功能接线遵循相同的可审查步骤：

1. 读取真实实现和调用方，在工作单锁定签名、文件和输入期望；没有读取的功能维持未审计。
2. 在所有者模块 `Private/Tests` 先写命名为 `FeatureId.Normal/Failure/Cancel/Cleanup` 的适用回归及明确断言；不适用项写原因并人工审核，不能用缺实现当不适用。
3. 记录红测；若需C++构建而被禁止，则写明未运行原因，不制造红/绿结果。
4. 通过核心适配合同调用真实API或配置已有适配器；纯数据差异不新增类，所有权不同才拆文件。
5. 执行允许的静态门禁，待匹配二进制和资产后做真实案例/人工检查，四维状态分别更新。

R01—R06分别可交付菜单/源码级阶段，不强制一起解锁所有插件。以下完整身份分批表只是任务归属，不是实现证据；具体工作单必须先于对应代码改动生成，避免在未读源码时提前虚构接口。

## 2. 插件分批责任表

| 批次 | 真实代码插件身份 | 现有源码根范围 |
| --- | --- | --- |
| R01 | GamePlatformPresentation、GamePlatformVFX、GamePlatformSFX、GamePlatformAnimation、GamePlatformCamera、GamePlatformSurface、GamePlatformWeather、GamePlatformPCG | `Game/Plugins/GamePlatform/Presentation`、`Gameplay/GamePlatformAnimation`、`World/GamePlatformWeather`、`World/GamePlatformPCG` |
| R02 | GamePlatformUI、GamePlatformInput、GamePlatformSettings、GamePlatformLoading、GamePlatformLocalization、GamePlatformCommerceUI、DBAClient | `Game/Plugins/GamePlatform/Presentation/GamePlatformUI`、`Application`、`PlayerServices/GamePlatformCommerceUI`、`Game/Plugins/DivineBeasts/DBAClient` |
| R03 | GamePlatformGameplay、GamePlatformCharacter、GamePlatformAbilitySystem、GamePlatformCombat、GamePlatformAI、GamePlatformInteraction、GamePlatformNavigation、GamePlatformQuest、GamePlatformWorld、GamePlatformLobby、GamePlatformVillage、DBAGameplay、DBAWorlds | `Game/Plugins/GamePlatform/Gameplay`、`World`、`GameModes`、`Game/Plugins/DivineBeasts/DBAGameplay`、`DBAWorlds` |
| R04 | GamePlatformCore、GamePlatformData、GamePlatformSave、GamePlatformApplicationFlow、GamePlatformInventory、GamePlatformEquipment、GamePlatformProgression、GamePlatformEntitlement、GamePlatformLiveOps、GamePlatformOnline、GamePlatformSession、GamePlatformServer、GamePlatformDebug、GamePlatformTelemetry、GamePlatformDeveloperTools | `Game/Plugins/GamePlatform/Foundation`、`Application`、`PlayerServices`、`OnlineServices`、`Diagnostics` |
| R05 | GamePlatformArena、MobaPresentation、DBAArena、DBAServer | `Game/Plugins/MobaCommon`、`Game/Plugins/DivineBeasts/DBAArena`、`DBAServer` |

五批合计47代码身份；每批只修改工作单明确的文件。公共案例适配放已有Demo模块的 `Private/Scenarios`；平台可复用逻辑留平台自身。只有真实测试职责需要才新增原插件文件，不为每个插件机械创建提供者。

### R01：表现、声音、环境与编辑器制作

**Files:** 工作单锁定的正式Public API及对应Demo适配；复用已存在BirdFlock/Weather/MossSnow/Snow/Fauna/Terrain/PCG候选地图，AssetReuseMap记录真实包路径；更新编目源、Monolith清单和所属插件README/ManualReview。

**Interfaces:** 以正式表现/音效/相机/天气/Surface/PCG接口为输入，统一输出核心T04操作结果。VFX十行为/十七分类来自现行规范，不新增执行器。纯编辑器制作与客户端/权威结果各用独立运行方式。

- [ ] 完成八插件功能工作单，读到资源/公开接口后再标已实现；没有真实声音的SFX项不得只用文件名代展示。
- [ ] 为各功能加入缺资源、停止、附着对象失效、叠加、世界退出断言；质量档和并发只报告批准条件下实测，不虚构达标预算。
- [ ] 接入真实适配与参数，已有材质库作者/审核UI按当前所有者纳入，保留其路由；新资源统一由引擎/Monolith生成。
- [ ] 分插件核验可见/可听效果、参数与复位；组合画面成功不自动签收所有提供者。PCG额外区分生成、碰撞/导航和Cook产物。

### R02：界面、输入与公共客户端

**Files:** 项目适配在 `DBAClient/Source/DivineBeastsDemoClient/Private/Scenarios`；业务VM/页面仍在 `DivineBeastsUIClient`；真实公共Widget归DBAUIPack_Core；各插件既有测试按工作单增量。

**Interfaces:** 消费原页面/VM事件与命令；目录显示正常、忙碌、禁用、失败、空数据与缺依赖。不把UI夹具当玩家数据，不在Widget内发送HTTP。

- [ ] 工作单逐项核对设置、角色信息、地图任务、技能书、Buff/Debuff计时、施法、小队、鼠标与目标等已有实现；未接服务和本地化模块壳如实登记。
- [ ] 写焦点冲突、输入设备切换、失活清空/解绑、服务失败、不同视口及敏感信息不留存断言；测试页面不保存密码或令牌。
- [ ] 将真实页面以受控上下文挂入UI展厅；仅演示自己的配置/会话，取消后恢复本案例修改的输入状态。
- [ ] 通过Monolith核对布局/父类/层栈并重载；按当前资源指纹做颜色验证与人工操作核验，不把旧报告复用为新页面通过。

### R03：玩法、世界与交互

**Files:** 复用 `Foundation/PluginReviews/Maps/L_GP_CombatReview`、`L_GP_InteractionAIReview`、`L_DBA_HeroShowcase` 等候选；项目案例适配不改正式默认GameMode；源码测试放工作单确定的所属模块Private/Tests。

**Interfaces:** 只通过已存在的角色/技能/GAS/任务/交互/导航/世界公开能力操作；状态用既有复制、事件与只读投影。

- [ ] 逐十三插件拆功能，确认实际角色/Experience/权威对象是否存在；大厅不增加独立服务器角色，Village教学/训练只是体验。
- [ ] 写无目标、超距、目标销毁、重复命令、资源不足、冷却、取消、不可达、任务条件拒绝、旅行后旧对象失效等适用断言。
- [ ] 实现可控制角色、目标和交互/导航对象的最小真实场景；命中/伤害/任务结果归权威，不用视觉成功替代。
- [ ] 单机、客户端和联网检查分开记录，复位仅操作隔离场景；联动案例每个插件均有独立ReviewCase和证据。

### R04：基础、玩家服务、在线与诊断

**Files:** 所属插件公开接口及Demo私有服务适配；后端仅读取实际合同/测试环境说明，不顺手修改本轮并行背包授权实现；使用核心T08/T11报告和运行器。

**Interfaces:** 实际服务的请求、取消、结果和诊断状态；所有写请求通过受控测试端点。核心Data租约样板可作为起点，不复制新加载器。

- [ ] 完成十五插件功能工作单，明确哪些能离线检查、哪些依赖本地服务、哪些只有Editor；未配置服务时菜单显示具体前置条件。
- [ ] 写超时、重复请求、过期响应、取消、凭据脱敏、存档隔离、证据路径逃逸以及非法复位拒绝的用例；每项断言关联真实接口。
- [ ] 使用隔离测试数据调用真实服务；经济/库存变更复位只允许业务合法补偿，不删除流水、不直接覆盖余额，正式认证不随演示入口自动启动。
- [ ] 输出真实状态与失败原因；自动成功只能成为证据，不代签人工审核。后端未就绪不切换到未标记的内存模拟。

### R05：MOBA、项目竞技与服务器集成

**Files:** 竞技适配只在 `Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaClient/Private/Demo`，测试放该模块Private/Tests；必要私有依赖只在非Shipping/Test开发配置增加 `DivineBeastsDemoClient`，保持Server目标不引入公共客户端。服务器侧沿用DBAServer与既有三角色测试配置。

**Interfaces:** 由竞技客户端组合根调用核心T04注册/注销；关闭MOBA时公共目录无需竞技类型。服务器操作通过已有受控测试/准入接口，不由DemoClient直接包含服务器私有头。

- [ ] 审查四插件实际接口，按定义枚举1v1、2v2、3v3、4v4、5v5；登记三角色与必要地图/服务缺口，未经批准不补造竞技规则。
- [ ] 写单人直接匹配、组队不可拆分、错误准入拒绝、重连、结算重复、过期返回目的地及关闭MOBA仍能用公共中心的用例。
- [ ] 非Shipping/Test条件下接入中立竞技语义和项目场景；实例结束注销完整注册句柄，依赖闭包按Client/Server/Editor分别检查。
- [ ] 在隔离服务上运行多客户端核验；单机地图不能代替网络权威。证据分别记录三角色/五模式，不合并成“竞技已通过”。

### R06：全部内容包与覆盖收口

**Files:** 真实内容所有者及其Monolith清单，源编目与导出的FeatureCoverage/AssetReuseMap；运行报告引用核心证据根，不复制资源或新建播放器。

**Interfaces:** 内容包来源为 `ContentPackRegistry.json`，加载/租约来自Data，场景调用现有消费者。18身份如下，执行时与实际集合核对：

| 类型 | 内容包稳定身份 |
| --- | --- |
| 十二英雄 | DBAHeroPack_Rat、DBAHeroPack_Ox、DBAHeroPack_Tiger、DBAHeroPack_Rabbit、DBAHeroPack_Dragon、DBAHeroPack_Snake、DBAHeroPack_Horse、DBAHeroPack_Goat、DBAHeroPack_Monkey、DBAHeroPack_Rooster、DBAHeroPack_Dog、DBAHeroPack_Boar |
| 公共与前端 | DBAContentPack_Common、DBAFrontEndPack |
| 公共表现 | DBAUIPack_Core、DBAPresentationPack_Core、DBASFXPack_Core |
| 世界 | DBAWorldPack_Village |

- [ ] 逐包核对主资产/软引用/实际消费者、许可和编辑器保存状态；角色头像不能冒充角色模型或技能完成。
- [ ] 写缺内容、重复主资产身份、撤销包、多世界租约、缺可选皮肤回退以及服务器纯表现排除的适用断言。
- [ ] 为每包建立独立画廊入口与真实使用案例；复用既有资产，不把项目资源拷到平台，未实现项明确显示。
- [ ] 汇总所有65身份与真实审计功能数、案例覆盖和当前版本人工记录；交回核心T14执行总体验证，仍未满足项逐条保留。

## 3. 接入完成判据与执行交接

每项功能只有在“源码审计依据＋真实操作/失败/取消/清理检查＋当前版本运行证据＋人工结论”满足时才能按对应维度完成。适配器源文件写完、资源文件存在、构建通过、单机能显示都不能互相替代。

回退先撤销本次目录激活和适配注册，结束本次实例并释放本次租约，再按引擎记录恢复资产/路由；不卸载其他世界资源、不改变原业务事务历史。若发现需要新增生产权限、改变玩法合同或扩大到插件业务补全，先报影响并请求方向，不在演示任务内默默完成。

计划审阅后按Native先执行核心计划；接入批次遵循本文件功能工作单门禁。未审查的插件接口不在本计划虚构具体函数名，实施前工作单必须记录真实签名与确切文件，否则该功能仍为未审计、不可进入接线阶段。
