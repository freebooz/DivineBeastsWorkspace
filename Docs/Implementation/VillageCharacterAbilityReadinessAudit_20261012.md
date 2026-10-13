# 《SSLM》登录到Village及角色技能装载逻辑审查（2026-10-12）

> 范围：DivineBeastsWorkspace／Game/DivineBeastsArena.uproject，源码与现有配置只读审查。新一轮双客户端、专用服务器、真实引擎资产的联机验收尚未执行。当前不修改正式Hero资产、不把开发技能自动切入发行环境。

## 一、实际调用链

1. 项目客户端唯一ApplicationFlow完成账号认证、后端角色创建或选择、体验解析、世界分配、Session转移和WorldReady；体验映射区分初始Village与已完成引导后的OpenWorld。客户端不能自行指定服务器权威Hero。
2. 项目Server的DivineBeastsWorldCharacterAdmission（服务器角色准入）先凭真实验证过的PlayerId读取PlayerData所选CharacterId及归属，再从Roster取Active HeroDefinitionId，并结合原生PlayerState生命周期绑定平台生成的Pawn。
3. UDivineBeastsCharacterComponent（项目角色身份组件）接收服务器AuthorityBindTrustedContext，借GamePlatformData异步加载英雄定义、确认世界/AvatarGeneration和HeroID后广播ReadinessChanged。
4. 可玩ADivineBeastsGameplayCharacter默认子对象包含唯一ASC及UDivineBeastsAbilityLoadoutComponent。Loadout监听身份Ready，从Hero.DefaultAbilitySetId读取技能集身份，借GamePlatformData加载AbilitySet/Gameplay Bundle，检查技能类、定义、消耗与冷却，成功则由服务器GAS GiveAbility授予，失败禁止伪造技能。
5. 技能LoadoutState只向拥有者复制；DivineBeastsAbilityBarPanel绑定当前Pawn的Loadout。AbilityBarViewModel按HeroId/AvatarGeneration核对真实ASC AbilitySpec与授权快照，刷新五个展示槽和冷却，不从图标虚构可用技能。

主要证据源码：
- Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsApplicationFlowClient/Private/DivineBeastsApplicationFlowSubsystem.cpp
- Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private/Characters/DivineBeastsWorldCharacterAdmission.cpp
- Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsCharactersRuntime/Private/Components/DivineBeastsCharacterComponent.cpp
- Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp
- Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/Panels/Combat/DivineBeastsAbilityBarPanel.cpp

## 二、明确问题

### 阻断项A：正式技能资产与Hero默认集合未完成发布资格

- 英雄类UDivineBeastsHeroDefinition.DefaultAbilitySetId默认NAME_None；当前开发技能审计脚本要求12个正式英雄的该字段维持None，以保护未经批准的开发技能集不能进生产。
- Game/Content/Development/DivineBeasts/Abilities中明确有12套开发AbilitySet及候选技能定义、蓝图、图标；它们不代表正式SkillSet已批准。
- 已执行真实Git资产目录审计：`Game/Plugins/DivineBeasts/DBAGameplay/Content/Definitions/`存在12份 `DA_Hero_Zodiac_*.uasset`（正式生肖英雄定义），预定正式目录 `Game/Plugins/DivineBeasts/DBAGameplay/Content/Abilities/`内已跟踪技能资产为 **0**；`Game/Content/Development/DivineBeasts/Abilities/`存在12份 `DA_DBA_*_DevAbilitySet.uasset`（开发技能集合）。此结果仅证明Git资产路径，不代替UE读取二进制资产的DefaultAbilitySetId实际值。
- Game/Config/DefaultGame.ini规定bAllowDevelopmentAbilitySets=false；只有VillageDevelopmentServer和VillageDevelopmentClient专用CustomConfig再配合启动参数DBADevelopmentSkills才可能在非Shipping/Test使用开发集合。
- 普通正式Village若Hero.DefaultAbilitySetId仍为空，Loadout会发布bReady=false与空槽位，不会自动授予开发技能。因此不能把真实角色已生成等同于技能已加载。

### 阻断项B：服务器和客户端进入Active门禁只确认英雄资源，不要求技能已真正授予

- WorldCharacterAdmission::IsPawnReadyForActivation目前只检查CharacterIdentity.IsCharacterReady；既未检查Loadout存在，也未检查OwnerOnly授权状态bReady、HeroId/AvatarGeneration与真实GAS AbilitySpec。
- WorldCharacterAdmission::ObserveCharacterInitialization在英雄定义Ready时清除服务器10秒初始化定时器，未等待另一个异步AbilitySet授予完成。
- WorldPlayerController::AreLocalResourcesPrepared只读取Experience Prepared；RefreshLocalWorldFacts报告客户端准备仅检查Identity Ready，不要求当前Pawn对应的Loadout授权快照。
- ApplicationFlow::TryCompleteWorldReady要求Session/Loading就绪，但GameplayData本地事实来自上述Experience资源，因此WorldReady并不证明五项技能就绪。
- AbilitySet未批准、缺资源或加载失败时可能已进入Active，技能栏仍为空；不应作为正式完成验收。

### 高风险C：异步失败与重试状态不充分

- Loadout在授予失败时发布bReady=false并撤销自己授予的技能/效果，这是合理的事务隔离。
- 但是Hero Identity不一定再次广播Ready，且客户端PreparationToken只允许每个Token发送一次；如果强行把服务端Active改成等待技能而不补Loadout授权事件订阅，可能制造永久等待或超时。
- 因此必须同步设计服务器Loadout Ready观察、客户端Loadout快照事件驱动重新准备、失败原因、有限重试、超时、旧Pawn/新Avatar代次防串号。不得只添加单点布尔检查。

### 待真实UE验证D：Village PawnClass及完整联机流

- 平台GameMode默认PawnClass为空，依照Village Experience/PawnDefinition按准入受控生成，必须真实用AssetRegistry核查该DataAsset的PawnClass是否确为ADivineBeastsGameplayCharacter派生类，不能只从C++构造函数推测。
- 角色创建与选择最新审查记录只证明前端UI构建、Cook及登录界面运行，未证明“实际选角→Village Dedicated Server→技能授权→HUD显示”完成。
- WorldReady的角色绑定、技能HUD根布局与真实资产Cook仍需重新打开编辑器和两客户端专服测试。

## 三、推荐的正确实现顺序

1. 先建立审批后的12英雄正式AbilitySet及必要的AbilityDefinition、能力类、数据表、UIProfile。让Hero.DefaultAbilitySetId具有明确的正式逻辑ID，不能通过移除开发标志或只改启动参数冒充发布。
2. 新手村应在服务器验证并绑定所选英雄Pawn、英雄Definition Ready后异步装载AbilitySet；授权成功才发布Loadout.bReady及与当前AvatarGeneration绑定的技能槽。第一次进入、重连、复活、切角色都走同一套，不单独在Village地图蓝图直接调用GiveAbility。
3. 在正式资产和角色类经UE验收后，调整项目服务器Active检查为Identity Ready + Loadout Ready + 当前Hero/Avatar代次一致，客户端准备对应订阅OwnerOnly Loadout状态和ASC Spec，必要时允许有限补偿重试。
4. 延长或重新界定服务端角色初始化等待窗口，以覆盖Hero定义与AbilitySet双异步阶段；技能缺失时发布可观察的失败码，不可静默进入空技能栏状态。客户端技能加载中展示不可使用的占位槽位，不伪造授权。
5. 进行真实Editor/Client/Server编译、Village专服+两客户端端到端登录、角色创建/选择、至少两种不同生肖的技能数量/图标/快捷键、Server实际GiveAbility、OwnerOnly复制、攻击/气势消耗、重连及切图回收、真实Cook和人工视觉检查。

## 四、工程归属

- GamePlatformAbilitySystem（平台GAS）：唯一技能授权和激活规则。
- DivineBeastsCharactersRuntime（项目角色）：可信身份与英雄定义。
- DivineBeastsAbilitiesRuntime（项目技能）：AbilitySet加载、授权、撤销与OwnerOnly状态。
- DBAServer（服务器项目层）：Server准入与实际技能Ready联动，不新建ASC。
- DBAWorldsRuntime（世界项目层）：当前本地Player/Pawn/Loadout事件驱动准备与复位。
- DivineBeastsUIClient（项目UI）：授权状态与图标显示，不向服务端授予技能。

## 五、总体结论与安全边界

架构方向和已有技能授权代码基本正确，但目前不能断言普通正式登录后进入Village一定拥有技能。最直接的阻断是正式DefaultAbilitySet未批准/未关联，其次是服务器Active和WorldReady未确认Loadout终态。修复必须先完成正式资源，再整体收紧双端门禁；否则可能导致所有进入Village的玩家被卡在准备阶段。

**本次是审查，不自动更改正式Hero默认技能身份或生产准入流程，也不宣称联机、Niagara或Cook验收通过。**
