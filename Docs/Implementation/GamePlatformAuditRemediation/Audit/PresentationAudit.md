# 表现层及内容插件只读审查

审查日期：2026-10-09。基线：`main`，`8a12bbe1d1d02db835b92207646138fabe782b50`。开始及结束源码状态检查均无未提交源码改动。本报告仅写入 Saved；未修改源码、配置、正式文档或资产，未调用 Monolith、启动编辑器、生成资产、构建或发布。

读取根 AGENTS.md、祖先及局部规则搜索、解决方案总体规划、插件开发规范 P09—P25、三层架构实施规划及设计基线整合规格/实施记录。实际没有发现范围内局部 AGENTS/override。using-superpowers 明确要求受委派子代理忽略该技能。父代理报告 Monolith 当前编辑器未运行，因此资产只核文件与历史 Manifest，不冒充当前 UE 资产回读。

## 判定尺度及范围

成熟度只描述本次可证事实：D0=设计/注册壳；D1=运行机制源码存在、集成或真实资源尚不完整；D2=存在项目消费者和真实原型资产，但仍有验收/正确性缺口。没有用文件数换算完善度百分比，没有 CPU/GPU/内存实测，也没有认定任何插件生产就绪。

全部范围的描述文件、Build.cs、模块注册文件、文件清单均已读取。运行源码详读范围逐行列在下表；表外小类型/测试通过文件清单、声明及搜索核对，不声称每个文件均完成全路径深审。

| 插件/模块范围 | 源码详读及证据范围 | 职责、成熟度与规范结论 | 源码性能证据与待测项 |
| --- | --- | --- | --- |
| GamePlatformCamera | 唯一 Build.cs、Private/GamePlatformCameraClient.cpp、README、ImplementationSpecification | D0。0.1.0；唯一 ClientOnly 模块只依赖 Core、仅3行注册。方案A已批准但实现未开始。没有服务、模式栈、租约、测试、实际消费者；不得称相机功能已完成。 | 无运行机制，无法评价相机算法性能。当前预览相机在 Presentation/项目预览实现中，不能记为 Camera 插件集成。 |
| GamePlatformPresentation | Core 的 Context/Catalog/Request 类型、Catalog.cpp；ClientSubsystem.h/.cpp 全文；PreviewStage.cpp 全文；RegistryTests | D1。中立请求、LocalPlayer注册、上下文贡献、目录和预览舞台成立，平台未反向引用项目。P13排序/歧义仍有 F01/F02。Public接口和字段中文说明不完整。 | Contributor注册时排序，Submit无需复制排序；目录每次双层全扫，上限64片段×512项；没有语义索引/解析缓存。是热点候选，不是实测瓶颈。预览Actor无Tick。 |
| GamePlatformSFX | 所有13个 .h/.cpp，尤其 WorldSubsystem、桥、Definition、Policy和策略测试 | D1。0.2.0；ClientOnly且Client/Editor白名单、游戏世界/非Dedicated/非Commandlet过滤、Data Definition租约、AudioFinished清理成立。P2 F06/F07；终态接口不足F08。产品Target/项目组合未启用SFX，项目无真实音效Definition/Sound。 | 无Tick；每声音独立AcquireDefinition，完成时线性遍历最多256名义实例找Component，始终Spawn/Destroy而非自建池；重度5v5需Audio Insights/并发虚拟化验证。 |
| GamePlatformSurface | State、Settings、WorldSubsystem全文；AssetContract、EditorLibrary及Commandlet/测试声明；Content文件检查 | D1。状态验证/裁剪/事件/MPC桥完整，Client+Editor端侧正确；F09默认资源链缺失，F10同步加载。没有Content目录，MPC、9个必需母材质/函数均只有路径合同；无项目C++消费者。 | 无Tick；状态变化写固定8参数，重复状态去重。首次绑定在游戏线程LoadSynchronous，缺MPC时每次更新会重试加载。没有Shader/材质/设备成本证据。 |
| GamePlatformUI | Manager.h/.cpp、UIScreen、ActivatableWidgetBase、ViewModelBase、WorldUIService全文；Loading/Feedback/Notification服务与Adaptive、层栈、Definition验证/测试声明 | D2。CommonUI层栈、LocalPlayer、ViewModel事件及普通软资源句柄可用，存在真实项目UI消费者。F11失活过早撤销资源需求/重入时序；不将Widget直接HTTP或业务Tick指控到平台。 | 世界投影按需30Hz遍历最多128Widget，有池复用；通知/反馈低频事件定时器；池按类线性查找。无实际设备/复杂Widget布局性能测量。 |
| GamePlatformVFX | WorldSubsystem.h/.cpp全文；InstanceRegistry/Record、NiagaraExecutor、Pooling/CompositeRunner、Definition/CompositeDefinition、CatalogRegistry/Resolver全文；Provider/桥、设置、参数Schema及测试/Editor校验声明 | D1。0.2.0；私有世界执行、World共享Definition缓存、Data租约、实例反向映射、Niagara原生池、取消/子定时器所有权已存在。真实Niagara/Definition/ReviewMap为0，SourceArt=6 PNG+shader源；P13兼容解析排序F03、复合依赖F04、未用回退F05。VFX→Core描述漏依赖由父代理实际架构门禁另证。 | 标准Play直接DefinitionId，热路径复用World缓存，不重复语义解析；兼容Resolver全目录扫描但有1024结果缓存；结束按Component map定位；LRU淘汰线性扫缓存。池策略有实际AutoRelease，但需真实Niagara取消/复用/GC留存验证。没有Insights、GPU、设备预算数据。 |
| MobaPresentation | ClientSubsystem.h/.cpp、FactAdapters、RequestBuilder、SemanticRegistry全文；Types/Tags/事件、两模块注册、6个测试声明 | D1。17个中立语义、Combat/Arena事实适配、预测去重、世界退出解绑可见，无Niagara执行或项目反向引用。F12类型化上下文丢失、F13迟到Pawn漏绑定。 | 无Tick；每个Fact重新取Contributor Keys并排序；去重超过2048时RemoveAt(0)搬移；LatestAvatarGeneration长世界会话随实体增长，暂无数量/淘汰界限。需5v5高频事实和迟到复制联调。 |
| DBAClient：DivineBeastsPresentationRuntime/Client | Runtime Catalog/ContentPack/Context/Facts/HeroVFXProfile；ClientSubsystem全篇；ContextContributor；CharacterAppearanceCatalog/Component/WorldSubsystem、PreviewSubsystem全文；Profile及4个Runtime测试声明 | 表现包协调D1、角色外观/预览D2。公共实现不依赖MOBA，外观使用平台薄软加载入口、请求代次取消、真实角色Ready事件；预览隔离旧卸载流送关卡并恢复原相机/Pawn可见性。F14激活伪事务及F15默认Definition不可执行；Client无Private/Tests，复杂生命周期仅有历史Manifest片段。 | 外观每角色最多约5秒20次状态查找重试；BeginPlay只扫一次角色；预览激活事件扫Actor选择舞台；每原型材质槽生成MID。无永久Tick，但首次大量角色加载/GC需测。 |

## 16个内容插件逐名覆盖

全部包为纯内容插件，无 Modules、无C++注册入口；均 Registry登记、CanContainContent=true、FriendlyName=英文稳定身份。下表资产计数是磁盘真实文件数，不证明资源真假、父类、硬引用、编译、Cook或运行通过。共79个文件（76 uasset、3 umap）。

共同 H 风险：Common及12 Hero被 `.uproject` 全局启用；Hero描述无目标限制依赖DBAClient，而ServerTarget未禁用；DedicatedServer Pak规则也未排除Common/Hero。见F16，需要实际Server依赖与干净Cook/容器补证。各Hero当前只有外观Profile+Prototype MI，没有可见技能VFX、Niagara、英雄模型/动画完整交付；此结论针对文件清单，不能反推二进制内属性。

| 插件 | 读取资源清单与项目消费者 | 成熟度、设计/规范结论 | 性能和未验证证据 |
| --- | --- | --- | --- |
| DBAContentPack_Common | 40 uasset；Mannequins/DBA Mesh/Skeleton/Physics/Material/Function/Idle动画，Standard材质及纹理；AppearanceCatalog、PreviewSubsystem消费 | D2原型共享源；131402435字节源包。具有明确所有者；Common内DBA/Standard同名母材质并存应通过AssetRegistry确认是否仍有合法消费者。共同H风险。 | 源文件字节不等于Cook后内存/GPU成本；需骨架/纹理LOD/动画授权及引用、干净Cook去冗余验证。 |
| DBAHeroPack_Rat | 2 uasset：DA_Appearance_Zodiac_Rat + MI_Zodiac_Rat_Prototype；AppearanceCatalog按Hero后缀软路径消费 | D2子鼠外观原型；非完整英雄内容；共同H风险 | 仅原型颜色；真实Mesh/骨架兼容、引用、材质参数、LOD、运行成本未回读。 |
| DBAHeroPack_Ox | 2 uasset：DA_Appearance_Zodiac_Ox + MI_Zodiac_Ox_Prototype；同目录合同消费者 | D2丑牛外观原型；非完整英雄内容；共同H风险 | 同上，不能用描述中“占位资源”证明资产可运行。 |
| DBAHeroPack_Tiger | 2 uasset：DA_Appearance_Zodiac_Tiger + MI_Zodiac_Tiger_Prototype；同目录合同消费者 | D2寅虎外观原型；非完整英雄内容；共同H风险 | 同上；Manifest中的历史Tiger预览不能替代当前MainArena运行。 |
| DBAHeroPack_Rabbit | 2 uasset：DA_Appearance_Zodiac_Rabbit + MI_Zodiac_Rabbit_Prototype；同目录合同消费者 | D2卯兔外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Dragon | 2 uasset：DA_Appearance_Zodiac_Dragon + MI_Zodiac_Dragon_Prototype；同目录合同消费者 | D2辰龙外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Snake | 2 uasset：DA_Appearance_Zodiac_Snake + MI_Zodiac_Snake_Prototype；同目录合同消费者 | D2巳蛇外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Horse | 2 uasset：DA_Appearance_Zodiac_Horse + MI_Zodiac_Horse_Prototype；同目录合同消费者 | D2午马外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Goat | 2 uasset：DA_Appearance_Zodiac_Goat + MI_Zodiac_Goat_Prototype；同目录合同消费者 | D2未羊外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Monkey | 2 uasset：DA_Appearance_Zodiac_Monkey + MI_Zodiac_Monkey_Prototype；同目录合同消费者 | D2申猴外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Rooster | 2 uasset：DA_Appearance_Zodiac_Rooster + MI_Zodiac_Rooster_Prototype；同目录合同消费者 | D2酉鸡外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Dog | 2 uasset：DA_Appearance_Zodiac_Dog + MI_Zodiac_Dog_Prototype；同目录合同消费者 | D2戌狗外观原型；非完整英雄内容；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAHeroPack_Boar | 2 uasset：DA_Appearance_Zodiac_Boar + MI_Zodiac_Boar_Prototype；同目录合同消费者 | D2亥猪外观原型；Boar与当前消费者一致，未改旧身份；共同H风险 | 当前无独立技能特效资源；需AssetRegistry和跨端资源剥离验证。 |
| DBAUIPack_Core | 8 uasset：Root、Login/Create/Select、2 Choice、2 Texture；2 SourceArt PNG；Manifest全文；UIClientSubsystem/UIScreenCatalog消费 | D2。合法项目UI资产归属与英文FriendlyName；Manifest记录Monolith0.20.3、历史编译/保存/回读和明确限制。当前Editor未运行，未读取Widget树/动态父类/焦点。 | 历史LOGO/PIE/Cook条目只证明当时范围；小窗口、移动、可访问性、真实键鼠、最新硬引用及服务器容器均待证。 |
| DBAFrontEndPack | 3资产：2umap L_DBA_FrontEnd/CharacterStudio +1Backdrop材质；README、Client/EditorTarget、PreviewSubsystem消费 | D2前端原型。与权威世界分离；平台舞台C++消费者存在；目前README仍说没有Client启动映射而配置已提供Custom FrontEndClient，应同步说明。 | 流送切入/切出、不同LocalPlayer舞台所有权、灯光与曝光、Cook引用需当前UE回读；没有场景GPU实测。 |
| DBAWorldPack_Village | 4资产：L_Village_Start.umap + Main/Tutorial/Training 3 Definition；README、Target与DefaultGame扫描路径、项目世界/流程消费 | D1首批流程地图与世界Definition。README:9还把Main/Training写后续，实际两Definition已在盘；最终湖心三岛PCG/世界美术未完成，不能称三体验内容验收通过。 | 地图内部Actor、导航、碰撞、权威PCG结果、出生安全点未在UE读取；三角色联机/地图Cook与服务器Ready须另证。 |

## 有依据的发现

以下为源码可复核问题。没有把缺少性能数据判为实测瓶颈。P1级跨插件漏依赖由主代理运行门禁另列；本组确定行为缺陷主要为P2，文档/注释为P3。

### F01 / P2 平台表现目录先比作用域，父语义可覆盖精确语义

文件：`Game/Plugins/GamePlatform/Presentation/GamePlatformPresentation/Source/GamePlatformPresentationClient/Private/GamePlatformPresentationClientSubsystem.cpp`，130先比Scope、134后比SemanticRank；365直接允许所有父标签，无显式允许开关。可复现配置：Project条目精确Semantic=Moba.Combat.Hit，ContentPack条目父Semantic=Moba.Combat；二者上下文合格，后者Scope3压过前者Scope2。P13要求精确语义第一、当前语义无合格结果且显式允许才逐级父回退。现有CatalogResolution测试只分别覆盖同语义Scope和单父回退，不覆盖二者冲突。

### F02 / P2 跨目录同EntryId绕过歧义

同文件392只在BestEntry.EntryId != Entry.EntryId时标Ambiguous；RegisterCatalogFragment只保证FragmentId唯一，Fragment.IsValid只保证片段内部EntryId唯一。两个不同Fragment可各用EntryId=Default、同排序键、不同DefinitionId，结果保留第一项，受注册顺序影响。应比较完整目录/条目身份，同键多结果不可用EntryId同名豁免；P13要求完全同键多结果失败并显示Pack/Catalog/Entry。

### F03 / P2 VFX兼容Resolver先比具体度，违反同语义Scope优先

文件：`Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/Source/GamePlatformVFXClient/Private/Resolution/GamePlatformVFXCatalogRegistry.cpp`，30-32按ContextTier→Specificity→Scope；170又把RequiredContextTags.Num加入Specificity。同精确语义下，低层更多标签条目能压过高层包默认，且具体度没有按六个已验证等值项计算。标准Gameplay当前绕过此兼容Resolver，但公开低层Catalog及StartupCatalogs仍可调用，不可宣称两套规则一致。

### F04 / P2 Composite Steps可绕过数据依赖无环/预加载门禁

文件：`.../GamePlatformVFXClient/Public/Definitions/GamePlatformVFXCompositeDefinition.h:14` Step仅FName；`.../Private/Definitions/GamePlatformVFXCompositeDefinition.cpp:29` 至51只查直接自引用；Editor `.../GamePlatformVFXCompositeValidator.cpp:39` 声称间接环由Data阻断。实际Data唯一边源是 `Game/Plugins/GamePlatform/Foundation/GamePlatformData/Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.cpp:240` 的Definition.RequiredDefinitions；不存在Steps与RequiredDefinitions一致性校验。A.Steps=B、B.Steps=A、二者RequiredDefinitions空均能通过定义校验，运行会产生多代子请求到深度截断，而不是加载/注册时拒绝环；缺子Definition也是运行后才部分失败。MaxChildren只限制每个节点Steps，未证明根树总子数预算。

### F05 / P2 配置FallbackDefinition没有运行消费者

文件：`.../GamePlatformVFXClient/Public/Definitions/GamePlatformVFXDefinition.h:81` 宣告FallbackDefinition；全文搜索运行层无读取此字段。WorldSubsystem HandleCachedDefinitionLoaded:375失败直接Cleanup；NiagaraExecutor:21资源不存在直接nullptr，ExecuteLoadedDefinition:1154失败退出。可选变体或基础Niagara缺失无法按定义回退，关键预警可能完全消失。当前无真实Definition，所以尚未触发实际游戏事件；这是公开合同和未来内容接入的实质缺口。

### F06 / P2 SFX Pending与Active预算可以突破名义上限

文件：`Game/Plugins/GamePlatform/Presentation/GamePlatformSFX/Source/GamePlatformSFXClient/Private/Subsystems/GamePlatformSFXWorldSubsystem.cpp:108` 分别限制Pending128和Active256。255 Active时可以接受128 Pending；完成回调326-494未重查总额，最多升到383 Active。真实超限路径由源码即可确定；未将383声称实测。应按预留实例的总量或每类独立且明确的预算计数，补批量完成回归。

### F07 / P2 SFX起播后才建立完成监听，极短音源时序需真实复核

同文件400/412/434先SpawnSound*，482才AddUObject AudioFinished。如果声音立即结束/起播尾部/并发拒绝在这段窗口终止，平台不会收到终态，Active记录和租约没有定时/失败清扫备援。文件源码证明监听晚于起播，是否能在锁定引擎产生该窗口仍需极短SoundWave/MetaSound实测；本机安装目录没有AudioComponent.cpp，不能伪称引擎内部时序已证。SFX TestingAndEvidence.md已主动列该未完成测试。

### F08 / P2 Queued之后终态对调用者不可区分

SFX服务公开接口只有Play/Stop/IsActive/更新/Diagnostics（IGamePlatformSFXService.h:20起），FailPending:520仅日志后释放；VFX服务公开接口同样无终态查询/完成委托（GamePlatformVFXService.h:22起），Definition失败统一Cleanup。调用方只能看到Queued→不活动，无法区分完成、取消、失败、内容撤销和世界销毁。现有失败路径真实清理，不是固定成功，但不满足P16的可诊断终态契约。Presentation Provider也忽略Play受理结果：VFXProvider:71、SFXBridge:75返回true，平台可能记录Submitted而实际加载未受理。

### F09 / P2 Surface被产品启用但默认材质资源链不存在

Settings.cpp:7默认指向`/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`；ClientTarget显式EnableSurface。实际范围没有Content目录或任何MPC/Material/Function，AssetContract.cpp:39起仅列9必需路径，EditorLibrary只生成MPC而不生成材质。默认Initialize只会记录绑定失败，状态更新不产生可见表面；应将现状明确为机制切片，真实链路补齐后验收。README25已诚实声明资产未制作，不算伪报。

### F10 / P2 Surface首次/失败后每次更新同步加载

SurfaceWorldSubsystem.cpp:158 在Initialize或Apply/Refresh时LoadSynchronous；失配且BoundCollection为空会每次重试。冷加载会阻塞游戏线程，缺失资源时增加重复查询。固定8参数事件桥成本清楚；这里只指出阻塞点，未测时延，后续应使用Data普通软资源入口异步持有句柄或明确预热合同。另83只看BoundInstance有效即返回Unchanged，之前ParameterContractMismatch未保持失败状态；重复输入会从失配变成“无变化”，需错误重试回归。

### F11 / P2 UI失活即撤销资源及关闭通知早于完整解绑

UIManagerSubsystem.cpp:765 AddWidget可能激活/失活页面，784才绑定Deactivated、790才转移ActiveScreenLeases；页面初始化事件中立刻关闭可漏掉清理后留下lease。818在任何Deactivate即移除lease，CommonUI覆盖但仍留栈的旧页会失去PreloadAssets需求，返回重激活没有加载路径。UIScreen.cpp:43先广播PlatformDeactivated，44才调用父类；ActivatableWidgetBase.cpp:82才EndPage。关闭观察者重入打开相同VM时，旧EndPage可结束新代次。现有项目主页面每次建立新VM降低重入概率，不能据此认为公开平台合同安全。需要真实栈覆盖/返回、初始化自闭、GC后软预载及回调重入测试。

### F12 / P2 MOBA到平台的类型化上下文丢失

MobaPresentationRequestBuilder.cpp:7-34从Fact复制空间及通用Request字段，没有给Request.Context赋任何值；MOBAContext已有HeroDefinitionId/AbilityId/ArenaModeId/AvatarGeneration（Types.h74/75/67/92），FromAbilityFact:189实际填入AbilityId。到平台目录Hero/Ability/Arena约束全部失效，项目默认ContextContributor可能补成本地英雄而非事实源英雄，旁观其他英雄时选择错映射。旧单测只查通用Request.WorldGeneration，不查类型化Context。

### F13 / P2 MOBA自动绑定漏迟到Possess及重生

MobaPresentationClientSubsystem.cpp:29/91/103为RefreshBindings唯一内部调用，分别初始化/世界重置/PostLoadMap；144读当前Controller/Pawn/Combat，缺失就不订阅。没有PlayerControllerChanged/PawnChanged/组件就绪订阅，项目范围搜索无外部RefreshBindings调用。同地图后续Possess、网络迟到Pawn、重生换Pawn都不会绑定新的Combat，无法自动适配Hit/Heal/Death。公开AdaptAbility/Status也没有实际游戏接线消费者，不能把函数存在算集成。

### F14 / P2 项目内容激活只广播预载，没有原子完成/回滚

DivineBeastsPresentationClientSubsystem.cpp:189先向平台发布Catalog，205调用RequestLogicalPreload，215-217即加入Active并成功返回；249-282仅保存ID及Broadcast，整个Game/Plugins无OnLogicalPreloadRequested订阅者、无Data调用、无完成回执。bRequiredPreload不会阻止发布；即使资源缺失或没人处理事件也处于“已激活”。Partial failure不可能回滚、Deactivation也只是广播Cancel。与P14/P15要求完整预检成功后发布、失败撤销租约冲突。内容Registry是工程清单，不自动代替此运行合同。

### F15 / P2 项目默认VFX映射不符合当前Definition逻辑ID并没有对应资产

DivineBeastsPresentationProjectCatalog.cpp:23/38填写`Presentation.DBA.World.Interaction.Committed.Default`、`Presentation.DBA.Village.Guidance.Ready.Default`，缺少当前GamePlatformId规定的`@version`；WorldSubsystem.cpp:38以TryParse转换后才AcquireDefinition。这些ID没有真实Definition/内容包文件、DBAPresentationPack_Core也未登记。所以合法WorldInteraction/VillageFeedback能被Catalog解析及Provider记录Submitted，但VFX最终拒绝/缺定义。应把默认映射视为规划合同，交付对应合法Definition后才开放成功路径。

### F16 / P2 Common/Hero的服务器禁止装配未在项目启用层贯彻

Game/DivineBeastsArena.uproject:19-31全局启用Common/12Hero且无TargetAllowList；每个Hero.uplugin:14无目标限制依赖DBAClient。Server.Target仅Enable DBAServer/DBAArena/Village，没有Disable这些全局启用项；Custom/DedicatedServer/DefaultPakFileRules.ini:8-12排除DBAClient/UI/FrontEnd/VFX/Surface，但没有Common/Hero。说明文档“Server Target不启用英雄美术包”与描述层不符。源码可证明未排除声明，不能凭此声称实际已Cook/链接；应对同一HEAD求Server闭包及干净Cook/最终容器确认。

### F17 / P3 中文职责/API说明及内容文档未同步

PresentationClientSubsystem.h:38-65、Context/Catalog大部分公开字段，Moba Types.h:50-55及74-95、DBA ContentPack.h:26-35、VFX/SFX多公开字段均仅英文标识或一句类型注释，没有参数/单位/空值/所有权/失败/线程说明。Camera Build.cs和模块入口没有职责头。符合命名扫描不等于AGENTS中文人工说明条款已满足。Village README:9仍把Main/Training写后续，实际已交付；FrontEnd README:33保留旧启动说明；正式历史文档计数可能过时但不自动回写历史证据。应逐受影响文件补说明并做人工审核，不为一致性擅自改稳定身份。

## 必须补的真实运行证据

1. 同一HEAD的Editor/Client/Server构建和干净Cook/Stage，AssetRegistry父类/硬引用/Primary ID/Bundle数据；Server必须同时审Common/Hero/UI/VFX/Surface及最小权威动画。
2. Presentation精确/父语义、同名跨目录、Scope优先、资格、目录失败原子回滚和注销后的迟到加载，不仅当前3个类型测试。
3. SFX极短完成、255Active+128Pending、Owner销毁/并发抢占/虚拟化、取消纠正、双LocalPlayer；Queued终态；5v5 Audio Insights。
4. VFX真实Niagara/EffectType/Definition/评审地图、AutoRelease池反复自然完成/取消、Data多世界租约、Composite间接环/总子数/失败传播、关键预警缺资源回退及迟到瞬时事件时效。
5. Surface真实MPC及Material/Function链、项目世界消费者、Shader编译与不同材质复杂度/设备GPU成本；避免仅生成MPC认为完整材质方案交付。
6. UI通过正确Editor及Monolith读取/编译/保存/重载树、父类、焦点/导航/可访问性；Modal覆盖再返回、GC软预载、重入、世界切换、小窗口/移动/真实鼠标；密码瞬态与事件驱动源码由UI主组另审。
7. MOBA迟到GameState/Pawn/Combat、重生/旅行、英雄/技能Context映射、其他玩家表现、预测确认撤销，五种竞技人数高密度表现。
8. 79个内容资产引用、骨架、材质、动画/源图来源、碰撞/导航/出生安全点/权威PCG结果及多客户端Village真实准入。历史Manifest不能作为本轮运行通过。

本组未执行任何UE运行检查或性能捕获。主代理本轮静态门禁的结果应在总报告引用其真实日志，不能归为本子组执行；本报告新增项只是源码审查与磁盘清单。
