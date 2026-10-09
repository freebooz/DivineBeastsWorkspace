# 《神兽联盟》十二生肖技能数据资产、服务端授权与技能栏自动初始化实施计划

版本：V1.0（分阶段实施中；未完成 UE 与内容资产验收）
编制日期：2026-10-09
适用工作空间：DivineBeastsWorkspace（神兽联盟工作空间）
唯一正式工程：Game/DivineBeastsArena.uproject（虚幻引擎主工程，UE5.8）
依据：AGENTS.md V1.4.3、Docs/Architecture/解决方案总体规划.md、Docs/Architecture/游戏端插件三层架构实施规划.md、Game/Plugins/插件开发规范.md、现有技能/角色/UI源码与2026-09-30技能VFX和战斗公式计划。
范围：**执行计划与验收设计**；本文件创建不等于已创建 C++ 功能、DataAsset（数据资产）、Widget Blueprint（界面蓝图）或通过引擎构建。
执行记录：`Docs/Implementation/十二生肖技能数据驱动实施记录_20261009.md`（本次真实代码、配置、静态门禁和未完成的正式资产/引擎验证事实）。文档更新不将尚未通过的阶段标记为完成。

## 一、目标与不可变边界

1. 为十二生肖建立稳定技能身份、技能集、项目玩法参数、等级数值与纯客户端显示资源的**单一归属**；技能改名、换图标不改变服务器 AbilityId（技能编号）。
2. 形成最小真实闭环：Server HeroDefinition（服务器英雄定义）→ AbilitySet（技能授权集）→ GAS AbilitySpec（技能授权实例）→ Combat（权威战斗）→ Owner Client Replication（拥有者客户端复制）→ Ability UI ViewModel（技能栏视图模型）→ Widget（视觉技能栏）。
3. 复用 GamePlatformData（数据基础）、GamePlatformAbilitySystem（平台 GAS 基础）、GamePlatformCombat（平台战斗）、GamePlatformInput（平台输入）、GamePlatformUI（平台 UI 槽位）；不创建第二套资产管理器、ASC（技能系统组件）、战斗结算器或输入系统。
4. 严格保持 DivineBeasts（项目层）→ MobaCommon（可选 MOBA 层）→ GamePlatform（基础层）的单向依赖。非竞技 OpenWorld（常驻世界）与 Village（新手村）不得强制依赖 DBAArena（项目竞技）或 MobaPresentation（MOBA 表现）。
5. **禁止恢复**已取消的 Element（旧五行玩法）、克制、破元、共鸣、阵营或王印规则。气势 Momentum（允许保留的项目 GAS 属性）继续作为合法技能成本/收益。未来若另有经批准的玩法变更，必须单独进行设计审批、兼容及回归，不能以本任务名义恢复旧规则。
6. 保留五个项目代码插件 DBAGameplay、DBAWorlds、DBAClient、DBAServer、DBAArena；不新增 `DivineBeastsAbilities.uplugin` 或无职责空插件。若技能代码隔离确有必要，**仅在现有 DBAGameplay 插件内**新增 `DivineBeastsAbilitiesRuntime`（神兽联盟技能运行模块），须先完成构建、依赖、宿主及长期维护影响审核；不是自动建空模块。
7. 专用服务器不引入 Texture（纹理图标）、Widget（界面控件）、Niagara（粒子视觉特效）、SFX（纯客户端音效）资源或 ClientOnly（仅客户端）模块；客户端不得独立决定伤害、命中、成本、解锁或技能授权。
8. 项目全部新增/修改源码、结构体、类、公开函数、字段、路径、配置、测试与文档均提供具体中文说明，遵循稳定身份、完整错误路径、有限资源生命周期与真实构建验收。
9. 本计划不为 12 英雄预先虚构正式 AbilityId（技能编号）、技能名称、伤害数值或二进制 `.uasset`；每个真实技能必须由策划确认并由 UE 编辑器生成资产。

## 二、当前事实和技术缺口（实施前仍须复测）

- 已发现十二个 `DA_Hero_Zodiac_*`（十二生肖英雄定义）文件，英雄定义包含身份、生肖标签、气势等，但没有经核验的英雄默认技能集字段。
- `UGamePlatformGameplayAbility`（通用技能基类）、`UGamePlatformAbilitySetDefinition`（通用技能授权定义）、`UGamePlatformAbilitySystemComponent`（平台 GAS 组件）、`FGamePlatformCombatSpec`（战斗规格）和伤害公式已存在源码。
- `UGamePlatformAbilitySetDefinition::ValidateDefinition`（技能集合验证）约束同一个 AbilitySet 中 AbilityId、AbilityClass（技能类）、非空 InputTag（输入标签）唯一；首次样板须遵守，不以同一技能类重复申领多个槽位绕过校验。
- 输入配置存在主攻击及 `Slot1` 至 `Slot4` 四个主动技能输入标签；项目输入事件适配已有实现，但实际 Spawn、授予真实 Ability、正式输入资产和网络联调尚待确认。
- `UGamePlatformSlotWidget`（通用槽位）与 `UGamePlatformSlotBarWidget`（通用槽位条）已存在；`UDivineBeastsAbilityBarPanel::ApplyAbilitySlots`（技能槽状态提交）已存在；当前没有审实的一条从英雄技能集到技能栏自动装配的完整链。
- 静态目录中未发现命名为生产十二生肖技能、技能图标的真实二进制资产，也未发现批量正式技能平衡数据表。目录和文件名核查不代替编辑器 AssetRegistry（资产注册表）扫描及资产内容值验证。
- `WBP_DBA_UI_AbilityBar`（技能栏蓝图）、`WBP_DBA_UI_AbilitySlot`（技能单格蓝图）等仍列为待 Monolith MCP（虚幻界面工具连接器）制作，不将 JSON 规格视作真实蓝图。
- 当前工作树存在其他任务的未提交修改，特别是 DBAClient/DivineBeastsUIClient（神兽联盟客户端 UI 代码）及相关文档；开始改动前必须对目标文件逐项检查 `git status`（版本状态）与 `git diff`（差异），保留其他任务成果，不进行重置、覆盖、自动提交或推送。

## 三、数据资产模型（设计先行，防止双重真源）

### A. 英雄定义：`UDivineBeastsHeroDefinition`（生肖英雄定义）

- 在既有服务器安全的英雄定义中增加**稳定逻辑引用** `DefaultAbilitySetId`（默认技能集合编号），不加入硬引用 AbilityClass（技能类）、UI Texture（图标）、VFX/SFX（特效/音效）或具体客户端控件。
- 保持原有 `DefinitionId`（英雄编号）、`GetPrimaryAssetId`（主资产身份）和角色出生链的兼容性；未配置新字段时明确失败或按批准的开发策略处理，禁止 Shipping（正式发行构建）静默给予假技能。
- 在服务器组合端负责按 `HeroDefinitionId`（英雄编号）查到技能集并调用 GAS 授权流程；角色身份模块只记录身份和默认集合 ID，不负责 GiveAbility（实际技能授予）。

### B. 技能集合：`UGamePlatformAbilitySetDefinition`（平台能力集定义）

- **优先复用已存在类**：`FGamePlatformAbilityGrant`（单项技能授权）保存 `AbilityId`（稳定技能编号）、`AbilityClass`（GAS 技能实现类软引用）、`AbilityLevel`（等级）、`InputTag`（输入标签）及来源元数据。
- 12 个英雄先各规划一个默认集合；Passive（被动）按真实授权策略配置，不假设被动技能一定绑定按键；Primary（普通攻击）与四个主动槽位采用现有平台能力输入标签体系。
- 如果多个英雄共用某个技能，实现类可跨英雄复用；同一个集合内遵守“同技能类不可重复”现行门禁。首次不改变该验证规则；需要改变时单独审批与测试。
- 保持 `GamePlatformDefinition`（现有平台主资产类型）及 `FGamePlatformId`（平台逻辑身份）兼容，禁止新建并行主资产管理/缓存系统。

### C. 项目技能定义：`UDivineBeastsAbilityDefinition`（拟建生肖技能玩法定义）

- 作为项目层对 `UGamePlatformDefinitionBase`（平台定义基类）的**必要数据扩展**，只承载项目专属技能规则：类型、目标规则、射程、数值行 ID、能力生效参数及所需的效果逻辑 ID。
- **身份使用继承的 `LogicalId`（逻辑编号）**，与 AbilitySet 内 `AbilityId` 严格一一对应；不在定义类重复设置另一套 AbilityId 真源。
- AbilityClass 与 InputTag 的拥有者是 AbilitySet 授权配置，项目技能定义不能再复制它们；真实 GAS 冷却及 GameplayEffect（玩法效果）运行状态的拥有者是 ASC，不落入静态定义。
- 数据资产只保存 server-safe（服务器安全）字段和稳定逻辑引用，不出现图片、声音、Niagara、Widget、AssetPath（客户端资源路径）等表现依赖。
- 若 P0 审查发现已有等价现行 Definition 真实满足全部契约，应复用现有定义而非新造同义类型。

### D. 数值表：`DT_DBA_AbilityBalance`（拟建技能平衡数据表）

- 项目层声明 `FDivineBeastsAbilityBalanceRow`（技能等级数值行结构），包含技能编号/级数、`BaseDamage`（基础伤害）、`AttackPowerCoefficient`（攻击加成系数）、`AbilityPowerCoefficient`（技能强度加成系数）、`DamageType`（伤害类型）、`bCanCritical`（是否暴击）、`CooldownSeconds`（冷却秒数）、`MomentumCost`（气势消耗）、`CastRangeCm`（施法距离厘米）、`AreaRadiusCm`（作用半径厘米）和按需合法的治疗/控制等字段。
- 先做 DataTable（数据表）；连续级数规律确实需要才使用 CurveTable（曲线数据表）。仅保留业务真实使用字段，不要求每个技能填入无意义的全部字段。
- 每技能、每等级有唯一行身份，数值有限且范围合法；列单位、允许区间、缺失含义、级数上限、版本和回退/失败语义全部中文说明。
- 服务端根据真实 `AbilityLevel`（技能等级）加载唯一对应行，并使用现有 Combat（战斗插件）计算结果。客户端只可依据同版本配置显示说明/预估值，不可提交服务器接受的伤害量。
- 数值平衡表改动应记录修订号和服务器/客户端内容对账策略，避免客户端 tooltip（提示说明）与服务端实际计算不一致。

### E. 客户端技能表现配置：`UDivineBeastsAbilityUIDefinition`（拟建技能界面表现定义）

- 放在 `DBAClient/DivineBeastsUIClient`（项目 UI 客户端模块）或经 P0 边界检查确认的现有客户端表现模块中，只拥有 `DisplayNameKey`（名称本地化键）、`DescriptionKey`（描述键）、`Icon`（图标软纹理引用）、提示/无障碍信息；不复制伤害和运行冷却真值。
- 通过 AbilityId → 表现定义映射查找 UI 文本、`TSoftObjectPtr<UTexture2D>`（纹理软引用），并由既有项目 Presentation Catalog（表现目录）映射 VFX/SFX/皮肤，不为图标重新实现第二套 VFX/SFX 注册表。
- 具体英雄图标与技能表现归 `DBAHeroPack_<Hero>`（对应生肖英雄内容包），公共栏背景/槽位蓝图归 `DBAUIPack_Core`（公共 UI 内容包）。
- 发布包中每个可显示正式技能有可验证的实际图标或明确的缺省降级策略；图标丢失仅影响显示，不影响游戏权威执行。

### F. 状态与作用域

- 服务器 `AbilitySpec/GameplayEffect/AttributeSet`（技能授权实例/玩法效果/属性集）拥有“已授权、等级、冷却、消耗、状态、当前气势”实时权威真值。
- UI `ViewModel`（视图模型）仅合成该事实和静态展示资源，遵循 `LocalPlayer`（本地玩家）作用域，并携带当前 `HeroDefinitionId`（英雄编号）、`AvatarGeneration`（角色实例代次）与 `Revision`（快照递增版本）；客户端 UI 无法自己授予技能。
- `SlotId`（槽位编号）、`AbilityId`（技能编号）、`InputTag`（输入标签）、`GameplayAbilitySpecHandle`（授权实例句柄）分别承担展示、身份、输入和运行实例四种职责，不互相冒充；映射必须唯一、可审核、可重建。

## 四、分阶段执行任务与门禁

### P0 真实现状基线、冲突检查与数据策划批准

责任：现有代码插件与 Docs（文档）；只读审查优先。

1. 读取全部实际生效 `AGENTS.md`（工程规则）、`Game/Plugins/插件开发规范.md`（插件规范），清点当前 branch/head/dirty（分支/提交/工作树变更）与并行任务，列出不得覆盖的目标文件。
2. 用 UE5.8 Editor（虚幻编辑器）AssetRegistry（资产注册表）复核 12 个 Hero Definition 的主资产 ID、实际字段、引用和 Cook（资源烘焙）范围；不得因文件存在即声称资产值正确。
3. 再核实 GAS `GiveAbility`（实际授予）、`AbilitySet`（集合）、`ASC`（能力组件）和角色 `Spawn`（生成）装配边界；对照项目输入 `Primary/Slot1~4`（主攻击/四技能槽）与 Widget 层既有状态字段。
4. 核查 DBAGameplay/DBAClient/DBAServer/DBAArena 的模块依赖图；确定新增 `DivineBeastsAbilitiesRuntime`（技能运行模块）的最小必需依赖，避免循环或多余模块。核查主资产逻辑 ID 注册与类别扫描。
5. 产出《技能主数据词典》《已存在/缺失/待验证清单》《业务技能策划审批表》《资产归属与唯一真源矩阵》，逐项列出正式技能名称/中文说明、技能序号、类型、等级、成本、基础参数、按键/槽位、动画/VFX/SFX、图标和状态，不猜测真实内容。
6. 正式配置不得包含已取消的五行、克制、共鸣和破元规则；当已有历史文档表述冲突，以 AGENTS.md 现行禁令为准并登记“废止旧规则”决策。

P0 验收：事实/未验证分开记录；12 个现存英雄定义目录经核查；资产 ID 与所有者设计通过；清晰批准首个子鼠样板具体技能的稳定 ID、名称和数值；无工作树覆盖。

### P1 技能逻辑契约、数据校验及模块边界

责任：`DBAGameplay`（项目玩法）、`GamePlatformData/AbilitySystem`（仅必要的通用能力）。

1. 先明确已存在 `UGamePlatformAbilitySetDefinition`（技能授权集）与 `UGamePlatformDefinitionBase`（主数据定义）的 API（接口）和资产扫描行为，不重建通用 Definition 基类。
2. 对 `UDivineBeastsHeroDefinition`（英雄定义）按兼容方式增加 `DefaultAbilitySetId`（默认能力集编号），只保存逻辑引用。
3. 在确认必要性后，在现有 DBAGameplay 内创建非空 `DivineBeastsAbilitiesRuntime`（技能运行模块）：项目 Ability Definition（技能定义）、BalanceRow（数值结构）、项目技能行为基类与真正消费者；更新 `DBAGameplay.uplugin`（插件描述）和 `*.Build.cs`（模块构建规则）。不得创建新项目插件或让角色定义层反向依赖技能模块。
4. 技能行为继承 `UGamePlatformGameplayAbility`（平台技能基类），客户端预测遵循 GAS 原生能力，服务器构造 CombatSpec（战斗规格），不写第二套伤害公式或 ASC。
5. 为 AbilitySet/AbilityDefinition/Balance 增加 ID 唯一性、等级范围、非有限数值、重复 InputTag、技能类重复、循环依赖、缺失行、版本不匹配、无权限等**失败即拒绝**校验；对旧英雄定义和已有资产保证加载兼容。
6. 按现有主资产类型 `GamePlatformDefinition`（平台定义主资产类别）规划扫描与类型约束；实际新类型若继承该基类，应使用既有 `GetPrimaryAssetId`（主资产 ID）机制，不未经审查创建新主资产类别。
7. 新模块的公共头文件只暴露稳定契约；Grant 逻辑、解析器、缓存、策略等放 Private（私有实现目录）。代码、Build.cs 和测试同时补充中文注释。

P1 验收：目标依赖无环、插件数不增加、类型身份/序列化兼容、非法资产被拒绝、定向 UE Editor+Client+Server 模块编译记录真实退出码；模块尚未写出时不得标记通过。

### P2 单英雄“子鼠”权威授权与实际技能闭环

责任：`DBAGameplay/DivineBeastsAbilitiesRuntime`（技能定义与行为）、`DBAServer`（公共服务器装配）、`DBAArena`（可选竞技装配）。

1. 由人工策划先批准一个**真实**子鼠主动技能的 AbilityId、技能名称、数值和目标规则，再在真实 UE 编辑器中生成数据资产（不是写入文本 `.uasset`）。
2. 创建子鼠真实 `AbilitySet`（技能集合）、`AbilityDefinition`（技能逻辑定义）、`BalanceRow`（数值行）和继承平台技能基类的真实 Ability（技能实现）；按当前同类唯一门禁使用合法的技能类。
3. 核对 AbilitySet 真正可由服务器授予。若现有 GamePlatformAbilitySystem 只有定义验证而缺少通用授权执行，应审查后优先完善现有平台 GAS 授权合同；否则在 DBAServer（项目服务器组合端）做最小装配，不复制授予账本或 ASC。
4. 角色就绪后由服务器确定可信 `HeroDefinitionId`（英雄编号）并授予默认集合，支持幂等、失败回滚、真实 SpecHandle（授权句柄）持有与按授权来源撤销；装备等额外技能不得被本功能误删。
5. 服务端真实使用 GAS 资源成本/冷却 GameplayEffect、现有气势属性以及 GamePlatformCombat（通用战斗）结算，不接受客户端直接传入 Damage（伤害）、命中结果、等级或非法 AbilityId。
6. OpenWorld（常驻世界）、Village（新手村）应使用相同通用授权服务，竞技 MainArena（主竞技场）通过独立组合端调用；通用角色链不得导入竞技插件。
7. 验证死亡/重生、切换角色、断线重连、重复进世界及版本不匹配时撤销与重授予流程。异步数据加载必须检查 Hero/Avatar/World 的代次。

P2 验收：单个子鼠真实技能能够在专用服务器权威完成“加载→授予→输入→成本→冷却→伤害→复制”，非法触发被拒绝，重生不重复发放；无图标/特效时权威逻辑仍可验证。

### P3 技能栏数据适配、自动装配与事件驱动状态

责任：`DBAClient/DivineBeastsUIClient`（项目 UI 客户端），复用 `GamePlatformUIClient`（平台通用 UI）。

1. 新增有实际消费者的 `UDivineBeastsAbilityBarViewModel`（技能栏视图模型）和 `DivineBeastsAbilityUIAdapter`（技能 UI 适配器），复用现有 `UDivineBeastsAbilityBarPanel`（项目技能栏面板）、`FGamePlatformUISlotState`（通用槽位状态）。
2. 数据源以“服务端已授予并复制的技能集合”为准；静态技能定义只用于合法 ID 对账、名称/图标查找和 Tooltip（技能提示信息）。不能只根据英雄静态列表宣称技能已可使用。
3. 根据 `HeroDefinitionId+AbilityId`（英雄与技能编号）异步请求合法 UI 表现资源，由 GamePlatformData（统一数据服务）的作用域/租约机制管理；不要在 Widget 中同步 `LoadSynchronous`（同步加载）。
4. 使用已有 `SlotId/ContentId/Icon/Count/OverlayProgress/bEnabled/bPending`（槽位身份/内容/图标/次数/覆盖/可用/待确认）映射；项目特有等级、剩余秒数、耗能原因、解锁说明留在项目 ViewModel/Tooltip，不为生肖私有字段改造平台通用槽位基类。
5. 订阅授权新增/移除、ASC 复制、等级变化、GameplayEffect 冷却、AttributeSet 气势变化、状态控制、英雄切换、设备切换及界面激活/失活事件；无常驻 Widget Tick（逐帧业务轮询）。冷却视觉过渡允许基于权威起止时刻本地插值，不要求每帧网络消息。
6. 对可播放技能实行有效/冷却/耗能不足/被控制/尚未授权/资源待加载/禁用/失败等状态区分，避免未知技能显示为可释放。缺失图标展示明确中性回退但不假定技能存在。
7. 断线重连、跨世界、死亡重生、分屏多本地玩家时必须按 `LocalPlayer/AvatarGeneration/Revision`（本地玩家/角色代次/状态修订）取消旧订阅与迟到异步回调；画面不得残留上一个英雄的技能。

P3 验收：不用为每个生肖编写固定技能栏；同一技能槽 UI 在真实服务器授权变化后自动生成/更新/删除；单张缺失图标不阻断合法技能，未知或未授权技能不能触发。

### P4 真实技能栏 Widget 蓝图与图标

责任：`DBAUIPack_Core`（公共项目界面内容包）、`DBAHeroPack_*`（英雄独有内容包）。

1. 使用现有 `GamePlatformSlotWidget`（通用槽位控件）、`GamePlatformSlotBarWidget`（通用槽位条）与 `DivineBeastsAbilityBarPanel`（技能面板）的继承及组合，不重复编写另一套技能栏基类。
2. 由 **Monolith MCP**（界面资产工具连接器）在确认已经打开 `Game/DivineBeastsArena.uproject`（正式 UE5.8 编辑器工程）且工具可用后，创建/完善 `WBP_DBA_UI_AbilitySlot`（技能单格）、`WBP_DBA_UI_AbilityBar`（技能栏）以及需要的公共 SlotBar（通用槽位条）真实蓝图。
3. 子鼠视觉资产只在批准技能后生产：中文技能名称、单格图标、冷却遮罩、快捷键提示、等级与气势不足状态；图标归 DBAHeroPack_Rat（子鼠内容包），公共框架/主题归 DBAUIPack_Core（公共界面包）。
4. UI 图标源文件可以按资产制作流程维护可溯源素材，真正引用的 `UTexture2D`（引擎纹理）必须经正式 UE 工具导入，不能通过文件复制/改后缀冒充二进制引擎资产；神兽联盟 UI 视觉资产创建/调整统一走 Monolith 相关 UI 能力。
5. 单次 Monolith 操作必须记录工具状态、原生父类、Widget Tree（控件树）、控件绑定、编译、保存、Editor 重载回读、显示结果；同步 `MonolithGenerationManifest.json`（界面生成审计清单）与 UI 中文清单。
6. 验证键鼠、手柄及触屏视觉模式、焦点导航、不同宽高比、安全区、缩放、无障碍对比与技能提示；UI 命令通过现有输入服务，不直接触发无授权的 GAS 指令。

P4 验收：真实 `.uasset` 文件存在且内容经 UE 编辑器/Monolith 加载、编译、保存、重载验证；初次获得子鼠技能时可见正确图标与冷却状态；仅有 C++ 类型或 JSON 规格不得判定已交付。

### P5 技能动画、VFX/SFX 表现映射

责任：`DBAClient/DivineBeastsPresentationClient`（项目表现适配）、`MobaPresentation`（可选竞技语义）、现有 `GamePlatformVFX/GamePlatformSFX`（机制）、`DBAHeroPack_*`（资源）。

1. 仅针对 P2 真正落地的 AbilityId（技能编号）创建明确生命周期表现映射：CastStart（施法开始）、Release（释放）、Impact（命中）、End/Cancel（结束/取消），按真实能力需要增加 Projectile、Warning 等语义。
2. 复用现有 `Hero VFX Profile`（英雄视觉特效配置）、`Presentation Catalog`（表现目录）、平台 VFX/SFX 机制；MOBA 项目使用中立 MobaPresentation（MOBA 表现）事实，非竞技直接通过平台中立表现入口。
3. 技能伤害、命中范围、权威位移与控制持续时间**只能来自玩法/GAS/Combat**，不得来自 Niagara 粒子半径/动画播放长度；特效缺失不取消已生效的权威伤害。
4. 按内容包和 `AbilityId/Phase/SkinId`（技能编号/阶段/皮肤编号）解析皮肤差异，不改动真实技能 ID，不产生同一个事件的重复 VFX/SFX 播放。
5. 预加载只面向已选择的真实英雄/技能，资源使用软引用和有限租约；退出世界、换角色、关闭客户端或取消施法时释放自身句柄，不卸载其他使用者资源。

P5 验收：子鼠样板在动画/音效/特效可用时正常展示，缺失或取消能回退且权威战斗不受影响，Server Cook 不带纯客户端表现资源。

### P6 扩展 12 生肖与内容治理

责任：`DBAGameplay`（统一玩法数据）、`DBAHeroPack_*`（每英雄专有美术）、`DBAClient`（统一 UI 适配）。

1. 通过 P2—P5 后再扩展十二生肖；初期策划参考“每英雄四个主动槽位＋一个被动技能，普通攻击单独配置”的规模约束，**不是已有生产资源事实，也不是未经审核自动建齐 60 个技能**。
2. 第二个样板优先选择需要投射/范围/多阶段表现的辰龙，验证技能系统不是专为子鼠硬编码；再按策划批准逐个补全其他生肖。
3. 每个正式技能交付一组互相引用且唯一的资料：身份/本地化名称/技能等级数值/目标与成本/真实技能类/输入授权/图标/必要动画/VFX/SFX/测试证据。
4. 共用通用规则、伤害公式、命中验证与槽位 UI，不让 GamePlatform 出现生肖类型，也不让 HeroPack 提供第二套技能系统；不为目录齐全而制作空资产。
5. 编辑器执行 AssetRegistry 和 DataValidation 检查 12 英雄唯一默认技能集、所有已批准技能参数范围和路径/身份，无二义性映射；打包内容由真实注册与目标过滤决定，不因文件夹存在就默认加载。

P6 验收：每个声明“已正式交付”的技能可追溯到策划批准、服务器实际授权、正确数值与真实客户端资源；未交付英雄或技能明确为缺项，不以回退图标冒充完成。

### P7 工程化验证、性能与交付门禁

1. **测试代码**：按实际生产模块放 `Private/Tests`（内部测试目录），测试合法/重复技能 ID、重复技能类、重复槽位/输入 Tag、缺失等级行、无效范围、NaN/Inf（非有限数值）、版本不匹配、未授权输入、取消与错误回退。
2. **服务端**：真实两个 Client（客户端）连接 Dedicated Server（专用服务器），验证子鼠样板授权/复制/伤害/冷却/气势/重生/断线重连/换英雄/跨图/旧 Avatar 回调拒绝，严禁只测试本地 UI 假数据；必要时使用可独立复现的 Server-only 自动测试。
3. **三角色**：验证 OpenWorld、Village、MainArena 三类服务器角色的已授权能力装配；竞技的 1v1～5v5 共用既有 MainArena，不为技能创建五套服务器 Target（构建目标）。
4. **构建目标**：分别运行 UE5.8 Editor（编辑器）、Win64 Client（64 位客户端）与 Win64 Server（64 位服务端）相关模块编译；报告真实可重放命令/退出码/失败文件，不把模块编译冒充完整运行。
5. **真实 Cook**：分别执行客户端与服务器干净 Cook（资源烘焙）和 Stage（暂存），检查 AssetRegistry（资产注册）、IoStore/Pak（打包容器）与依赖闭包；服务器不得包含 HeroPack 图标、Niagara 及 DBAUIPack_Core 等纯客户端资源，客户端必须具有主动技能的必要 UI 定义与图标或批准的合法回退。
6. **UI 验证**：Monolith 创建/修改的蓝图逐项记录父类/树结构/编译/保存/重载/截图/焦点/导航；验证不同分辨率及 PC/移动布局。Monolith 不可用时标记真实阻断，禁止改用伪 `.uasset`。
7. **性能**：UI 事件驱动，初始化资源异步并去重/可取消，基于 LocalPlayer 与 World 代次持有租约；对多技能连续切换、断线循环和战斗高频状态更新做 Unreal Insights（虚幻性能分析），量化帧耗时、资源峰值、重复请求和网络带宽；未经测试不宣称达到特定毫秒或内存阈值。
8. **架构门禁**：执行仓库已存在的 `ValidateProjectHeaders.ps1`（头文件引用校验）、`ValidateDesignBaseline.ps1`（总体设计基线）、`Tests/Architecture/InheritanceBoundaryAudit.psm1`（三层继承审计）及准确核查后的相应名称校验；区分历史工作树既有失败与本次引入的新失败。
9. **变更控制**：复核 `git diff`（变更清单）与中文注释、端侧依赖、资产授权来源、迁移兼容、错误路径；清楚区分“静态代码通过 / UE 编译通过 / 自动化测试通过 / 真实引擎资产交付 / 客户端运行通过 / 联机通过 / Cook/Stage 通过”七种不同证据。
10. **文档同步**：按实际实施更新 `DBAGameplay/README.md`（玩法插件说明）、`DBAClient/Docs/用户界面目录规划.md`（界面目录）、`DBAUIPack_Core/Docs/十大业务域Widget蓝图实施清单.md`（界面资产清单）、`Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`（总体目录）、`Docs/CHANGELOG.md`（修改历史）、各模块 API/验证/数据字典与真实 Monolith 资产清单。任何真实新增、移动或删除路径必须同步总体目录文档。

P7 验收：各阶段证据、未执行项、真实阻断、回退方案和人工审核结论可复核；阻断未解除不能标成“全部完成”。

## 五、建议的目标归属（不是本轮创建结果）

`Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/`（拟新增到现有玩法插件的正式双端技能模块，须审查后创建）
- `Public/Definitions/`（公开技能逻辑定义与数值行结构）
- `Public/Abilities/`（平台 GAS 技能派生的必要公开基类）
- `Private/Abilities/`（项目技能执行与合法参数读取）
- `Private/Tests/`（真实 UE 编译内测试）

`Game/Plugins/DivineBeasts/DBAGameplay/Content/Abilities/`（拟新增权威服务器安全技能数据目录）
- `Definitions/`（正式 Ability Definition）
- `AbilitySets/`（正式 AbilitySet）
- `Balance/`（DataTable 技能数值表）

`Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/`（既有项目客户端模块）
- `Public/Definitions/`（技能界面表现定义公开类型）
- `Public/ViewModels/Combat/`（技能栏 ViewModel）
- `Private/Adapters/Combat/`（GAS-to-UI 单向投影适配）
- `Private/Tests/`（技能栏状态和作用域测试）

`Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Rat/Content/UI/Abilities/`（子鼠技能专有表现资产，按需创建）
- `Definitions/`（图标/中文名称客户端定义）
- `Icons/`（真实 Texture2D 图标）

`Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Content/UI/Combat/`（公共技能栏视觉蓝图，由 Monolith 创建）
- `WBP_DBA_UI_AbilitySlot`（项目技能单格）
- `WBP_DBA_UI_AbilityBar`（项目技能栏）

所有上列路径均为**待实施的新增位置**；只有真实消费者确定且完成对应引擎资产生产后才实际创建。`DBAServer`（项目服务端）和 `DBAArena`（可选竞技服务端）仅做组合与授权接线，不为相同能力创建新的项目插件。`DefaultGame.ini`（项目资产扫描配置）按实际路径增量配置 GamePlatformDefinition（平台定义）扫描、Bundle（资产分组）与双端 Cook；不推断软引用一定自动烘焙。

## 六、阶段顺序、停止条件与工作树保护

推荐顺序：`P0 → P1 → P2 → P3 → P4 → P5 → P6 → P7`。在 P2 尚无真实技能授权/伤害闭环前，不进入批量图标或 12 英雄一次性制作。仅当单英雄功能、双端授权、真实蓝图、资源 Cook 验证后，才进行全生肖规模化扩展。

每阶段结束输出“修改的真实文件与资产清单 / 实际命令与退出码 / 用例和实际结果 / 已知失败与是否环境阻断 / 未完成事项 / 回退方法 / 下一阶段入口”。遵循最小增量；不默认自动提交、推送、不覆盖当前 dirty（含未跟踪）文件。对已有文件必须先读原内容、必要时做完整版本差异对比，再做最小修改。

**本计划不授权恢复已取消玩法，不授权新建额外代码插件，也不授权绕过 Monolith 制作视觉蓝图。**

## 七、计划本身的验收

- [ ] 项目规范优先级、五插件与三层边界明确。
- [ ] 统一数据模型没有重复授权/数值/图标/运行态真源。
- [ ] 明确了 `GamePlatformAbilitySetDefinition`（现有集合）及其唯一性约束。
- [ ] 明确图标与 Widget 均是客户端资产，Gameplay 定义 Server-safe。
- [ ] 明确 UI 初始化读取服务器真实授权，而非静态英雄列表。
- [ ] 明确 P0 审查、P1 架构、P2 单英雄、P3 UI、P4 真实蓝图、P5 特效、P6 扩展、P7 验收以及各期退出门槛。
- [ ] 明确禁止五行/克制/破元/共鸣旧玩法。
- [ ] 未把设计稿/文档、计划步骤或开发占位资源记为实施通过。
