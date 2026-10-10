# 变更记录

## 2026-10-10｜竞技打击反馈授权Profile释放及编辑器实测

- `DBAArena/DivineBeastsArenaClient`修复同World重复初始化可能清除浮字Widget租约、Arena已就绪时平台数据服务晚到后Profile加载永久跳过问题。新增纯策略`DivineBeastsHitFeedbackGrantPolicy`并接入实际OwnerOnly授权缓存：技能集合无变化不抖动加载，技能撤销/英雄切换/角色重生清理旧Profile租约，增加UE自动化测试源码。
- 独立`clang-cl`成功检查更新后的竞技反馈组合根和授权测试，并实际编译授权测试目标文件；专项静态33项通过。此前真实UHT生成结果有效，但当前完整客户端/服务器构建尚无成功退出码。
- Monolith MCP连接真实工程核对后发现新`UGamePlatformHitFeedbackProfile`和项目`UDivineBeastsCombatFeedbackCatalog`反射类尚未在编辑器加载；在DBAUIPack_Core中临时建立的浮字Widget因编辑器会话在蓝图编译保存前断开，磁盘无目标.uasset，不记资产交付。重新启动编辑器遭`FailedDueToEngineChange`（多个客户端模块缺失或不兼容）；未保存/覆盖其他任务资产。
- 实际运行已注册`DivineBeasts.UI.Combat.PlayerStatusSnapshot`测试得到1项失败、2条属性集创建断言；独立编辑器Python构造属性集可行，不代表原生测试通过。完整Editor/Client/Server链接、真实Profile与Niagara/SFX/WBP、Automation、Cook及多人联机保持未通过。



## 2026-10-10｜生肖技能可回滚启动效果授予与单文件构建验证

- `DivineBeastsAbilitiesRuntime`（神兽联盟双端技能运行模块）增加 `IsStartupEffectReversible`（启动GameplayEffect可撤销预检），拒绝瞬时、周期、执行计算、堆叠及未审核附加效果组件；利用UE5.8公开FindComponent（查找组件）API维持引擎封装。授权顺序收敛为完整预检→先应用可回滚GE→再授权GAS技能Spec，失败仅撤销本组件真实句柄，保留平台原生能力机制。
- 新增独立UE自动化合同 `DivineBeasts.Abilities.GrantTransaction.ReversibleStartupEffect`，覆盖无世界的正反向样例；旧版技能装配测试不再作为新逻辑通过的依据。
- 真实锁定UE5.8 `DivineBeastsArenaEditor Win64 Development`（Windows编辑器开发构建）分别对技能授权组件及授权事务测试源码执行 `-SingleFile` 定向编译，**两个UBT任务均返回Result: Succeeded、进程退出码0**；未完成整模块DLL链接、重新加载编辑器、Client/Server/Cook及真实联机。三层授权静态门禁41/41通过，头文件引用扫描632处无缺失。
- 同一UE5.8目标补充验证 `DivineBeastsConfiguredGameplayAbility.cpp`（服务器技能命中/伤害基类源码）与 `DivineBeastsPlayerStatusViewModelTests.cpp`（UI状态快照测试源码）的定向单文件编译，均返回 `Succeeded`（成功）、退出码0。由此四个针对性C++文件已获得独立编译证据；完整DLL和自动化重新加载仍未完成。
- 详细测试范围、未完成发布门禁及后续接线顺序见 `Docs/Implementation/十二生肖技能数据驱动实施记录_20261009.md`（生肖技能执行台账）。



## 2026-10-09｜生肖技能开发资产数据一致性、网络执行策略与审计增强

- 通过 UE5.8 Monolith 原生 AssetRegistry（资源注册索引）与 Editor Python（编辑器脚本）核对十二生肖全部60份开发 `UDivineBeastsAbilityDefinition`（技能逻辑资产）、60行 `DT_DBA_Zodiac_DevBalance`（统一等级数值表）、12份开发 `AbilityUIProfile`（客户端显示配置）和60张图标：真实资产归属、类型、逻辑ID、等级行全部无误；与 `ZodiacAbilityDevelopmentDraft_20261009.json`（开发数值草案）逐字段对比60行数值为0差异。
- 子鼠开发GAS技能蓝图 `GA_DBA_Rat_DevPrimary`（子鼠开发普攻）改用 `ServerOnly`（服务器独占执行）网络策略并由 Monolith 真实编译、保存、回读，避免把伤害权威交给客户端预测路径。编辑器已加载的旧 C++ 反射尚未包含 `AuthorityTraceForwardAndApplyDamage`（服务器前向射线伤害）接口，因此没有伪造蓝图执行连接，不把该示例声称为真实命中/伤害完成。
- 增强现有 `Tools/Unreal/Abilities/ValidateZodiacDevelopmentAssets.py`（生肖开发资产引擎审计），新增真实60个 UI 技能编号与60个玩法技能定义的全量相等检查、技能类型校验、子鼠开发蓝图服务器独占策略及 AbilityDefinitionId 校验；Monolith执行通过，Python语法检查通过。
- 修改蓝图后同步更正 `Docs/Implementation/ZodiacDevelopmentUEAssetEvidence_20261009.json`（76个开发资源SHA-256证据）中的子鼠技能蓝图摘要，文件级门禁 `--require-all-profiles --require-all-definitions --verify-hashes`（全资源及摘要）实测76/76通过。生产英雄默认技能集不使用这些开发ID，正式能力仍缺真实授权、双客户端联机、完整UE三端构建与Cook/Stage验收；详细证据见 `Docs/Implementation/十二生肖技能数据驱动实施记录_20261009.md`。



## 2026-10-09｜战斗打击反馈技能授权预热与局部顿帧调节

- 项目竞技ClientOnly组合根根据GameState设置通知及本地Pawn接管事件建立已授权技能订阅；从OwnerOnly授权快照预热当前HeroDefinitionId对应实际授予的有效技能Profile，按角色/世界切换释放旧资源和事件绑定，停止无差别预热十二生肖全部技能。
- GamePlatformAnimationClient增加本地开发控制台变量gp.Combat.HitstopOverrideFrames，-1按Profile、0关闭、3/6参考帧AB对比、最大10；保留独立时钟、根运动保护、GAS服务端权威。新增局部顿帧计算自动化测试源码与静态接入审计。
- 独立MSVC已发现并修复竞技反馈.cpp两处C4067预处理器#include拼接错误；三层接入静态22项通过，UE正式编译、反射/资产验证、Multi-PIE和专服联机仍待完成。


## 2026-10-09｜战斗反馈主资产租约与竞技客户端接线

- 现有平台`UGamePlatformHitFeedbackProfile`和项目`UDivineBeastsCombatFeedbackCatalog`升级为`UGamePlatformDefinitionBase`，通过GamePlatformData统一主资产身份、数据校验、异步租约；Profile的CameraShake/Overlay只在Client Bundle中加载。项目技能表改用ProfileDefinitionId，新增重复映射、非法主资产ID和参数数值校验及UE自动化测试源码。
- DBAArenaClient加入按LocalPlayer持有的实际异步加载组合根，按角色可信HeroDefinitionId＋权威SourceAbilityId为MobaPresentation提供每击独立的已加载Profile及VFX/SFX逻辑ID；无正式资产时退回平台默认表现，不构造假的Definition ID。世界退出释放Instance租约，异步回调核对世界代次。
- 平台视觉顿帧新增RootMotion角色与原本已经暂停Mesh的保守跳过策略，避免破坏网络根运动；不把它冒称为已实现真实击退积分冻结。插件和文档均保留三层依赖单向、服务端纯表现剥离原则。
- 静态检查已通过（614处头文件引用0缺失，477公开头、981类型、175条继承边）；正式Editor/Client/Server构建、UE自动化执行、资源资产和1v1—5v5联机未验收，仍不得宣称P0—P8完成。

- 后续修正：Catalog加载完成即按映射顺序预热最多64个不同Profile，真实竞技场GameState存在才装配资产；没有匹配当前英雄技能时严禁复用前一英雄的VFX/SFX定义。Client定向构建UHT生成13项通过，但C++动作因本机UBA持续不前进而主动停止，不属于已通过编译。


## 2026-10-09｜战斗反馈 P0—P8 追加开发与阶段性验证

- 计划：新增 `Docs/Implementation/CombatFeedbackWorkOrders_20261009.md`（三层插件工单和实际验收门禁），旧 `CombatFeedbackExecutionPlan_20261009.md`保留为初始计划，历史声称尚无网络事实等描述以本项最新源码检查为准。
- GamePlatformCombat（平台权威战斗）增加最小化Unreliable已确认表现RPC、事件GUID/技能ID/角色代次和位置投影，并经World范围只读总线交给表现层；不新增伤害RPC或修改GAS公式。Unreliable丢包只影响可选视觉，不能据此证明双客户端联网已经通过。
- GamePlatformAnimationClient（客户端动画模块）新增受击网格Overlay短闪白，保存原材质并限时恢复；GamePlatformCameraClient（客户端镜头模块）实现LocalPlayer CameraShake服务和玩家震动关闭倍率，均需真实资源与客户端评审。
- MobaPresentation（MOBA层）从网络事实总线接收已确认事件，按技能ID采用Skill/Light默认分类，利用统一平台表现Provider分别提交VFX/SFX逻辑请求。独立GUID防止VFX/SFX互相吞并；同一事实各客户端本地去重。
- GamePlatformPresentationCore可配置Profile增加Overlay/CameraShake软引用；DBAClient新增GamePlatformSFX客户端装配声明，现有英雄技能反馈目录仍需真实DataAsset、资产预加载/租约和DBAArena组合根接入。无新.uplugin、无伪造.uasset。
- 新增 `GamePlatform.Combat.Feedback.NetworkContract` 和 `GamePlatform.Camera.HitFeedback.UserScale` 自动化测试源码。静态检查和UE真实Client/Server构建、Automation、Cook、Multi-PIE、1v1—5v5的结论分开记录，不能以源码/静态测试宣告完整P8完成。



## 2026-10-09｜战斗打击感三层配置和客户端局部顿帧

- GamePlatformPresentationCore增加可配置的HitFeedback Profile数据结构；GamePlatformAnimationClient增加本地玩家局部Mesh动画暂停，含60Hz参考帧、命中GUID去重、CoreTicker单调实时时钟、连续10帧窗口封顶和World清理，不暂停Gameplay/全局时间。
- MobaPresentationRuntime增加轻/重/技能/格挡/挥空和暴击/连击反馈策略、0/3/6帧自动化测试源码，MobaPresentationClient消费已到达的Combat事实触发局部视觉顿帧，不新建MOBA特效播放器。
- DBAClient/DivineBeastsPresentationRuntime增加英雄技能映射DataAsset类，按Hero＋Ability键查询平台Profile软引用和视觉/音效逻辑ID；无真实 .uasset。
- 尚未完成真实复制命中通道、闪白、镜头及独立九层视觉资源、专用服务器Cook和联机验证；基础输入缓冲已写入源码但尚未实机验证。保存既有GAS未提交改动，具体门禁见 `Docs/Implementation/CombatFeedbackExecutionPlan_20261009.md`。

- 新增 `GamePlatformInputClient/Buffer/GamePlatformActionInputBuffer` 的真实有限离散输入缓冲（容量/超时/绑定代次/顺序/去重）、自动化测试源码；`DivineBeastsInputClient`订阅本地视觉顿帧自然恢复后，将缓冲输入交由原GAS接收/合法性判断。未开始权威网络事实、真实击退及闪白/相机等完整表现验收。


保留已有工程变更记录；不根据历史聊天补造不存在的提交或验收记录。

## 2026-10-09｜十二生肖技能定义、权威授予与技能栏视图骨架

- 专项后续增量：修复可信英雄身份切换时旧技能授权可能残留的问题；平台 GAS 对原生技能列表复制完成发出中立事件；项目 UI 改为匹配当前英雄与角色代次、真实 GAS AbilitySpec、CanActivateAbility（能力可激活性）以及 GameplayEffect（效果）和气势/控制事件，冷却遮罩按真实剩余/总持续时间投影，不做常驻业务 Tick。服务器授权前按级数核对 GameplayEffect 气势负值与时长是否等于 DataTable 配置，并在 GAS Commit 成功后才允许服务端造成伤害。新增 12 英雄×5枚共60张 PNG 图源校验脚本，结果通过，但**不等于引擎 Texture2D 导入或真实游戏 UI 已验收**。

- 在已有 DBAGameplay（项目双端玩法插件）中新设 `DivineBeastsAbilitiesRuntime`（技能运行模块），实现 `UDivineBeastsAbilityDefinition`（玩法主资产定义）、`FDivineBeastsAbilityBalanceRow`（每级伤害、冷却、气势等数值配置）、`UDivineBeastsAbilityLoadoutComponent`（按真实服务器英雄定义的默认 AbilitySet 申请、授予、撤销、OwnerOnly 复制），并保留既有平台 GAS/Combat/Data 为唯一通用能力真源。
- 新增 `ADivineBeastsGameplayCharacter`（项目可玩角色：ASC、角色身份、权威战斗、技能装配组合），竞技服务器出生逻辑改用该类。`DBAClient/DivineBeastsUIClient`（项目客户端 UI）新增 `UDivineBeastsAbilityUIProfile`（图标与名称）及 `UDivineBeastsAbilityBarViewModel`（授权槽位只读投影），原技能栏面板增量绑定本地玩家 Pawn 切换事件。
- 增量更新项目三层插件声明、双端技能定义扫描和仅客户端技能表现资源扫描；新增专项静态审计、单测源码及中文插件实施说明。根 AGENTS.md（项目规则）的旧五行/克制/破元/共鸣禁令保持有效，未制造正式技能资产 ID、`.uasset`、图标或 Widget 蓝图。
- 本批初始静态审计17/17通过（后续继续扩至29项），头文件扫描最初566处0缺失，继承边界扫描470公开头/963类型/175边通过；整体基线仍存在与 DBAWorlds/GamePlatformVFX 相关的16项既有依赖声明问题。之前因误查不完整的 D: 盘 UE 安装目录而未定位可执行文件；现已确认锁定引擎位于 `F:/UnrealEngine-5.8.0-release`，真实技能模块构建正在专项验证。UE 联机、正式技能资产及客户端/专服 Cook/Stage 仍未验收，详见 `Docs/Implementation/十二生肖技能数据驱动实施记录_20261009.md`。

## 2026-09-29｜登录后角色选择/创建三维前端预览

- 新增第三层纯内容插件 `DBAFrontEndPack（神兽联盟前端三维场景内容包）`，仅 Client/Editor Target 启用，Server Target 不启用；不纳入 `DBAWorlds`、WorldDefinition、ServerRole、Session Admission 或 World Assignment。
- UE5.8 已真实生成 `/DBAFrontEndPack/Maps/L_DBA_FrontEnd` 与 `/DBAFrontEndPack/Maps/L_DBA_CharacterStudio` 两张 `.umap`；编辑器回读确认 CharacterStudio 包含 `CharacterPreviewStage` 和 Key/Fill/Rim 三盏预览灯，FrontEnd 包含 `FrontEndCamera`。
- `GamePlatformPresentationClient` 新增跨项目 `AGamePlatformCharacterPreviewStage`，无 Tick、无复制，只接收已加载 Mesh/Material/AnimInstance 并提供角色旋转和镜头距离控制；平台层不出现 DivineBeasts/生肖身份。
- `DivineBeastsPresentationClient` 新增 `UDivineBeastsCharacterPreviewSubsystem`，按需流送 CharacterStudio，复用 `FDivineBeastsCharacterAppearanceCatalog` 与现有 12 个 `DA_Appearance_Zodiac_*`；异步请求使用 RequestGeneration 防止旧资源覆盖新预览。
- `DivineBeastsUIClient` 增加角色预览轻量接口并按 `CharacterEntry/CreateCharacter/ValidateSelection` ViewState 自动启停预览；预览动作不提交业务选择、不改变 ApplicationFlow。平台表现与项目表现模块 UE5.8 定向编译/链接成功；UI 两个本轮修改源文件的进一步单文件编译当前被同 Runner 另一 UBT 进程互斥锁暂时阻断，完整 UI 模块另有既有测试冲突标记与 Inventory 编译错误，不归因于本次前端预览实现。

## 2026-09-29｜新增 GamePlatformSurface 通用环境表面材质插件

- 在`Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/`新增正式平台插件，采用`GamePlatformSurfaceClient（ClientOnly）＋GamePlatformSurfaceEditor（Editor）`双模块，不建立Runtime／Server空模块；Client与Editor Target显式启用，Server Target不启用。
- Client实现跨游戏环境表面状态、八个稳定MPC参数、每世界事件驱动MPC桥接、C++服务、Blueprint入口、有限值／范围校验和生命周期清理；Editor实现真实核心MPC首次生成／只读校验Commandlet、材质资产合同和自动化测试源码。母材质及雪／苔藓／湿润／积水等Material Function只建立真实制作合同，不用文本或空uasset伪造完成。
- 锁定边界：GamePlatformSurface不拥有天气权威、PCG生成、Niagara、水体物理或神兽联盟专属内容；项目纹理和`MI_DBA_*`材质实例归`DBAWorldPack_*`。Surface表现失败不得改变服务器玩法，Dedicated Server不得链接或Cook纯表面表现资产。
- 正式基线更新为平台层39个＋MOBA层GamePlatformArena 1个，共40个GamePlatform稳定身份；加MobaPresentation和5个DBA代码插件后为46个代码／机制插件＋内容N。GamePlatformOpenWorld继续退休；当前46与2026-09-27历史46成员不同。
- 同步AGENTS、插件规范、插件主清单、总体规划／目录、核心要求、三层规划、P0历史补充、内容包／DBAWorlds边界和DesignBaselineAudit；实际编译、Automation、MPC生成、Cook／Stage与材质人工视觉验收结果按本轮后续真实执行证据记录。

## 2026-09-29｜十二生肖角色占位资源与角色插件闭环

- 十二生肖采用“稳定 HeroDefinitionId + Server-safe Hero Definition + 客户端 Appearance Profile + 独立 HeroPack”的可替换架构；开发期统一复用 UE5.8 Manny/Quinn，每个生肖使用独立识别色，正式模型替换时不修改后端协议、CharacterId、HeroDefinitionId、GAS 身份或存档键。
- 公共 `DBAContentPack_Common` 只保留一套 Manny/Quinn、Skeleton、PhysicsAsset 和基础材质/纹理；未导入旧工程 Control Rig、Mover 示例和动画蓝图，`Game/Content` 无版本控制重复 Mannequin 副本。12 个 Hero Definition、12 个 Appearance Profile、12 个原型颜色材质已落盘并纳入版本库。
- 角色运行状态改为原子复制 Hero/生肖/SpawnGeneration/AvatarGeneration/DefinitionVersion/ContentRevision，CharacterId 保持 OwnerOnly；新增平台角色状态只读接口，修复异步 Definition 旧请求覆盖新角色的竞态，并以 ContentRevision 做客户端/服务器就绪一致性门禁。
- `DivineBeastsCharactersRuntime` 注册统一 Character Initializer，只初始化平台 Spawn Operation 已创建的 ACharacter，不在项目层旁路 SpawnActor/Possess；Server Target 显式启用 DBAServer/DBAArena，OpenWorld/Village/MainArena 继续共用 Dedicated Server Target。
- AssetManager 已扫描 `/DBAGameplay/Definitions`；Client/Editor Target 显式启用公共角色包和 12 个 HeroPack，Dedicated Server 不携带这些客户端表现资源。原型生成脚本支持自动工作区/引擎探测、旧项目仅首次导入时使用，并提供 `-ValidateOnly`；当前验证结果为公共 Manny/Quinn 依赖完整、Definition/Profile/Material = `12/12/12`。
- 新增平台 `FGamePlatformCharacterInitializationExecutor`，统一解析且强制唯一 `GamePlatform.CharacterInitializer`；MainArena 新增项目 LifecycleAdapter，只使用标准 `RestartPlayerAtPlayerStart` 创建/控制基础 `ACharacter`，再调用 Executor 完成项目初始化，源码无直接 `SpawnActor/Possess`。12 个 Server-safe Hero Definition 在 Assignment 阶段异步预热，全部参赛者 `CharacterReady` 后比赛才进入 `InProgress`，复活复用同一链路。
- UE5.8 Server 定向构建实际编译并通过 LifecycleAdapter、Arena GameMode、ProjectExtension、InitializationExecutor；Editor 定向构建通过 `GamePlatformCharacter` 与 `DivineBeastsCharactersRuntime`。角色 Automation 启动被既有 `GamePlatformGameplay` 空实现阻断：该模块单独链接确认存在 `AGamePlatformGameModeBase/GameState/PlayerController/PlayerState` 等 9 个未解析符号；这是独立平台基础设施欠账，MainArena 当前不依赖它，本轮不在角色任务中重写整套通用准入状态机。

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
