# 创建页角色与技能介绍（2026-10-12）

## 范围与权威边界

本增量仅实现本地创建页介绍：角色正文来自已有项目 `HeroDefinition`，五项技能名称/说明来自独立客户端介绍资产。它不创建业务角色、不授予技能、不改变 `DefaultAbilitySetId`，也不把前端文案变成服务器权威规则。`AbilityDefinition`、数值表与授权集合不新增介绍读取入口。

全部当前候选必须逐项显示“开发候选，未作为正式技能批准”。现阶段文案陈述已存在的开发验证限制，不自行发明位移、控制、治疗、护盾、被动或平衡。原数值草案仅作为候选身份/名称/图标溯源，不能整体复制进纯介绍JSON或客户端资源闭包。未来正式技能批准需要独立数据迁移与审核，不能直接关闭草案标记。

平台层 `GamePlatformData` 继续唯一拥有资源加载和跨实例租约；项目 `DBAClient/DivineBeastsUIClient` 拥有VM、软映射与文本投影，真实公共页面/Profile归第三层 `DBAUIPack_Core`。沿用既有平台基类和项目扩展，不新增MOBA依赖、插件或第二个加载器。

## 字段、文件和稳定路径

- `DBAGameplay/Source/DivineBeastsCharactersRuntime/Public/Definitions/DivineBeastsHeroDefinition.h`：新增可本地化 `FText Introduction`。CDO不写默认介绍；旧定义可为空，但创建介绍VM明确拒绝空白正文。
- `Tools/Unreal/Characters/ZodiacPlaceholderCharacters.json`：十二份 `IntroductionZh` 作者正文。它不是英雄协议身份真源，不声明正式技能批准。
- `Tools/Unreal/Characters/GenerateZodiacPrototypeCharacters.py::ensure_hero_definition`：先核对非空正文，再写 `introduction = unreal.Text(IntroductionZh)`，不修改默认技能集合。批量入口先核对十二份正文；生成后复核正文等于作者源，防未来重建清空介绍。执行真实资产操作仍须由main授权的Monolith/编辑器流程负责，本侧未运行生成器。
- `Public/Definitions/DivineBeastsCharacterCreationProfile.h` 与 `Private/Definitions/DivineBeastsCharacterCreationProfile.cpp`：`UDivineBeastsCharacterCreationProfile` 派生既有 `UDivineBeastsAbilityUIProfile`；继承 `HeroDefinitionId`、`Entries`，新增 `bDescriptionsAreDraft=true`。`GetPrimaryAssetId()` 返回类型 `DivineBeastsCharacterCreationProfile`、名称HeroId，不复用战斗 `DivineBeastsAbilityUIProfile` 主资产身份。
- `Public/Private/Settings/DivineBeastsUISettings.*`：`UDivineBeastsUISettings` 是 `Config=Game` 的配置对象；`CharacterCreationProfiles` 为 `TMap<FName,FSoftObjectPath>`，读取函数 `TryGetCharacterCreationProfilePath` 不加载资产。实际映射位于 `Game/Config/DefaultGame.ini` 的 `[/Script/DivineBeastsUIClient.DivineBeastsUISettings]`。
- 创建Profile包名为 `/DBAUIPack_Core/UI/Profiles/CharacterCreation/DA_DBA_CharacterCreation_<Rat..Boar>`，配置写对应 `包名.对象名`；禁止 `/Game/Development` Profile、技能集合或子对象路径。运行时还核对真实类型、HeroId和独立PrimaryAssetId，不能仅凭文件名当作有效介绍。
- `Tools/Unreal/Characters/CharacterCreationProfiles.json`：十二份Profile/六十项纯介绍作者数据，只存五槽身份、中文名称、说明和本英雄UI图标软路径；顺序为 `BasicAttack`、`Passive`、`Active01`、`Active02`、`Ultimate`。
- 既有 `Public/Private/Definitions/DivineBeastsAbilityUIProfile.*`：公共字段校验新增 `Description` 空/纯空白拒绝，与名称/图标门禁共同执行。Creation专用校验另外要求恰好五项、同英雄身份、开发ID的草案声明，以及 `/DBAHeroPack_<Hero>/UI/Abilities/` 图标边界。

## 调用链与生命周期

1. 创建页的本地选择事件调用 `UDivineBeastsCharacterCreateViewModel::SelectHeroOverview(HeroId)`；相同英雄的Loading/Ready刷新不重复申请。
2. VM先增加独立请求代次、清空旧正文并释放自己的旧需求，再发布Loading。同步观察者可能切英雄/关闭页面；Loading返回后重新核对页面与请求双代次，不替旧意图继续申请。
3. 生产Outer是UI子系统→`ULocalPlayer`→Engine，不假定Outer链包含GI。优先 `GetTypedOuter<ULocalPlayer>()->GetGameInstance()`；仅测试保留 `GetTypedOuter<UGameInstance>()` 回退，不依赖未明确的GetWorld转发。
4. HeroCatalog只解析已有HeroDefinition软路径，Settings提供CreationProfile软路径；VM用一次 `IGamePlatformDataService::AcquireResources` 普通资源批次加载两者，寿命为Instance。显式GI作为Data合法作用域调用者，VM弱引用及双代次守完成回调。
5. 完成检查当前页面/请求/英雄、终态门闩、中央租约身份及成功状态，再验证真实作者内容。全部成功后一次性复制到只读 `FDivineBeastsHeroOverview`，绝不发布半份介绍；错误清正文、释放自身需求并显示明确错误。
6. 快照包含 `HeroDefinitionId`、`Introduction`、`Abilities`、格式化后的 `SkillIntroductionText`、`StatusText`、`bDescriptionsAreDraft`、`PageState`、`ErrorCode`、`PageGeneration`、`RequestGeneration`。独立原生事件访问器是 **`OnHeroOverviewChanged()`**，并非Blueprint动态属性；使用返回委托的 `AddUObject/AddLambda` 与精确句柄解绑。
7. `ClearHeroOverview`、`OnPageEnded`、`BeginDestroy` 主动撤销本VM租约；释放前先撤本地所有权，不能误释放后继请求。成功租约保留到换英雄/失活/销毁，其他消费者需求不受影响。不透明私有上下文采用 `TSharedPtr` 兼容UHT生成单元；只有VM持有，回调不捕获共享上下文，没有循环所有权。
8. Widget只投影事件快照的三段纯文本，不加载资产、不发送HTTP、不持有服务器业务权威，也不Tick轮询介绍。精确绑定原VM/页面代次；事件双代次拒绝同一广播栈中的旧英雄文本。
9. 页面刷新期间收到同步新VM初刷/状态变化时记录pending；旧轮次退出后重新读取当前VM和页面代次补刷。每次调用至多两轮同步投影，更多重入合并为单个游戏线程弱页面任务，避免无限递归；任务不捕获旧VM/正文，页面解绑代次及活动资格拒绝旧任务。没有业务Tick或无条件调度。

## 页面资产合同

动态绑定名固定为 `HeroOverviewStatusText`、`HeroIntroductionText`、`SkillIntroductionText`；旧Blueprint可 `BindWidgetOptional`，新资产门禁必须实际存在，不能以可选绑定掩盖新资产缺节点。

main在右 `CreationInfoPanel` 内维护独立 `HeroOverviewScroll`（ScrollBox）→`HeroOverviewBody`（VerticalBox），包括上述动态文本及 `HeroIntroductionHeading`、`SkillIntroductionHeading`。正文五技能只作可滚动纯文本，标题/固定字号14和16由Monolith资产管理；保留500×540面板、返回按钮和中部旋转区域，不缩放整页。本侧不创建/修改任何二进制UI、地图、落叶或VFX资产。

## 回归与依赖门禁

- UE前缀：`DivineBeasts.UI.Characters.HeroOverview`。既有相邻 `Private/Tests/DivineBeastsAbilityBarViewModelTests.cpp` 覆盖真实十二份英雄FText与Description空/纯空白；新 `Private/Tests/DivineBeastsHeroOverviewViewModelTests.cpp` 覆盖独立Profile身份/草案校验、真实中央租约切英雄/失活/迟到取消、无Data明确失败。
- 同步换VM P2 case：`DivineBeasts.UI.Characters.HeroOverview.ScreenSwapDuringLoadingRefreshesIncoming`。使用真实原生创建页和输入监听，在激活后触发Loading观察者同步 `InitializeActivatableViewModel` 换VM；需旧刷新退出后新英雄/页面代次重新投影。不用激活结尾或旧VM第二监听偶然补刷掩盖问题。最终夹具只广播一次控件OnTextChanged，不先SetText；main实际RED后才修改生产guard，未修改测试使之通过，GREEN由main统一执行。
- `Tools/Unreal/Characters/CharacterCreationIntroductionSource.py`：只读检查纯介绍字段白名单、十二英雄正文、五槽顺序、候选名称/身份/图标、逐项未批准声明、独立Profile类型以及客户端TMap软映射一致。数值/授权字段混入、跨英雄、开发Profile路径或重复映射必须拒绝。
- `Tests/Assets/TestCharacterCreationIntroductionSources.py`：实际源正向检查和独立突变负向测试；另运行正式生成器函数配字段记录替身，证明重建会写作者Introduction、缺正文在资产操作前失败。该替身不冒充UE保存、资产反射或Cook验收。
- 离线入口：`python -B Tools/Unreal/Characters/CharacterCreationIntroductionSource.py`；`python -B Tests/Assets/TestCharacterCreationIntroductionSources.py`。实际运行结果见下方最终集成验证，离线成功不表示真实引擎资产完成。

## 实施与证据边界

main已提供首轮三项真实RED，位置 `Saved/Validation/CharacterEntryFollowUp20261012/NativeRed.json`。首次生产Editor构建真实退出6；本侧按 `141221e2-0978-4b3c-aad9-d5ab4962301e/Build/Editor/UBT.log` 修复局部PageGeneration遮蔽、误用FString.ToString和UHT不完整UniquePtr删除，未降低编译器严格度。main随后通知Editor重编译 `604e04aa-dae4-4e42-bb92-4cafcbfc73de` 退出0；它不是新增P2测试、实际资产、完整UI套件、Cook或客户端验收结论。

本侧只读核对main运行的 `Saved/Validation/CharacterEntryFollowUp20261012/Native-fac4b24b-9b01-4d36-99ca-c331d9e147f7/Report/index.json`：完整UI报告52成功、1失败，失败为另项空列表焦点；HeroIntroduction、Description两项、CreationProfileIdentity、真实SwitchAndDeactivate租约与UninitializedData均记录Success。报告也包含P2 case的Success，但此时生产guard未修，早版夹具的旧VM双监听可能掩盖缺陷。该结果不能代替当前“单一输入事件”夹具的实际RED；须重新构建并运行P2后再修生产。

最终稳定夹具构建 `1cf9a449` 退出0。实际核对 `Saved/Validation/CharacterEntryFollowUp20261012/Native-45754abe-6c13-44c5-a760-ca85eece33d7/Report/index.json`，P2真实Fail：期望Hero.Zodiac.Ox却是None，期望Error却是Empty。收到此RED后，仅修改CharacterCreateScreen生产h/cpp：pending记录、有界两轮同步补刷、弱任务合并及失活寿命栅栏，并停止旧VM/页面代次投影；原回归测试源保持不变。最终构建、54项GREEN、RHI及Cook的实测证据分别记录，不预写通过。

本侧只在main原位通过apply_patch维护源码、作者源、测试及中文文档；未启动编辑器/Monolith，未构建，未Git写操作。main负责十二份真实CreationProfile及Hero.Introduction、生成清单、真实命名控件、FrontEnd依赖闭包/Cook、SourceToSlate颜色门禁和人工验收。依赖静态检查不能替代真实加载后的类型/注册身份门禁；正文FText仍需正式本地化收集与内容审核。

## 最终集成验证（2026-10-12）

最终Editor构建`30207b8f-1d8a-42d4-8373-617acfd13dcb`和Client构建`c554ecb0-f711-44fd-b190-6643bb5d69be`均退出0。UI原生报告`83e4ba6e-4408-41bc-8617-5cc3eccf21bf`实际54条均Success，包括保留最终RED夹具的同步换VM回归及真实Data租约生命周期；53条无警告、PlayerStatusSnapshot一条有GameplayCue扫描路径回退警告，不把带警告通过漏计或当作全日志无警告。以前两份报告也是54条，摘要计数不含带警告项。

离线介绍源回归11项与Architecture Pester 80项通过。独立重开正式编辑器后，经Monolith编译保存两页0错误/0警告，读取十二份英雄介绍及十二份独立Profile的六十项正文，与源JSON逐项一致。`Saved/Validation/CharacterEntryFollowUp20261012/ReloadAudit.json`保留结果；真实RHI颜色发布门禁`25289496-e5fd-479c-9c22-ced922f676b5`通过。以上不代替最终客户端视觉、鼠标操作或新手村联机验收；完整发布结果继续记录于`Docs/Implementation/CharacterEntryRevision_20261011.md`。
