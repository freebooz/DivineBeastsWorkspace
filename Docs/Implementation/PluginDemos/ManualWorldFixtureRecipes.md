根冷读修正（2026-10-28）：引擎现有 Default SupportedAgent 为35cm半径/144cm高，Recast重开会采用该配置；仅将本开发 DA_DBA_ManualNavAgent 对齐35/144，保留全局正式导航配置。历史42/192配方已被本条取代；具体证据 World.Cold.NavMismatch.json 和 World.NavFixture.AgentFix.Authored.json，最终新Editor冷读仍待。

# 逐插件测试世界夹具配方与当前状态

根已通过直接Monolith制作并保存：World等7正向、PCG运行图/配置/MeshSet/空间源、3个刻意负向资源、真实Hub导航和独立流送关卡。原生资产校验见Saved/Validation/PluginManualUI20261027相应Authored报告；冷读、实际生成/清理、Realized图、专用World宿主和人工签收仍分别待完成。以下为源交接时的完整配方，历史“未制作”仅代表原子任务交接边界，不覆盖上列根证据。

# T05 World 原生资源与专用开发宿主配方

状态：源API核读配方，未创建任何UE资产、未Bake、未原生重载。根使用Monolith执行，先核当前工程和目标包脏状态；已有资源先读和备份，不覆盖其他任务脏包。源资产以稳定职责名保存；不创建Review/V2竞争资源。Fixtures唯一General扫描沿根现有设置，不加第二资产管理器。

共同宿主：保留现有Hub Floor `(0,0,-80)` scale `(50,50,1)`、手工PlayerStart `(0,-750,160)`全部属性和当前Camera `(0,-1500,560)` pitch=-16/yaw=90；不复制生产Experience/ServerActive。仍用Engine.GameModeBase + `/Script/DBAWorldsRuntime.DivineBeastsWorldPlayerController`。World/PCG初始化需要新增**专用开发宿主装配代码/蓝图实例**，这部分本镜像未假装已存在；不能仅加资产后宣布成功。

World定义由宿主在显式 `-DBAPluginDemo -GamePlatformManualFixture -FoundationWorld` 且**本专用Standalone开发世界**开始时调用 `IGamePlatformWorldService::Get(World)->InitializeDevelopment(WorldDefinitionId, FGamePlatformVersion{0,1,0})`。0.1.0是本夹具声明版本，不是客户端/服务端二进制兼容证明。真实服务内部AcquireDefinition和递归Region租约，宿主不得手动Init第二份World/Data子系统，也不能用InitializeBoundWorld伪造SessionProjection。宿主持有整World生命周期；完整退出/Travel销毁该World时原服务Stop释放其Definition/Region租约。**没有公开重置初始化API，不能在正式世界中临时初始化后销毁Host声称完整归还。** 若专用Host需要同World中卸载初始化，属于尚缺的独立所有权扩展，必须另交实现，不能只写配方。

所需世界定义资产：

| 资产/实际类 | 必填字段与合法值 |
| --- | --- |
| `/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualWorld`，`/Script/GamePlatformWorld.GamePlatformWorldDefinition` | LogicalId=`world.demo.hub@1`（Namespace=`world.demo`, Name=`hub`, LogicalVersion=1）；DataVersion SchemaVersion=1/ContentRevision=1；MapIdentity=`/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub.L_DBA_PluginDemoHub`；DefaultExperienceId无效/空；Regions及RequiredDefinitions均含 `GamePlatformDefinition:world.demo.region@1`；ReadinessTimeoutSeconds=3600为夹具等待上限，非Ready证明。 |
| 同目录`DA_DBA_ManualRegion`，`/Script/GamePlatformWorld.GamePlatformRegionDefinition` | LogicalId=`world.demo.region@1`；DataVersion=1/1；ParentRegionId空；RegionTypeTag=`Manual.World.Region`；BoundsPolicy=AxisAlignedBox；ActivationPolicy=AlwaysRegistered；GameplayTags空；RequiredDefinitions空。 |

宿主按页选择拥有明确的区域Provider策略。`GamePlatformWorld.regions`案例保持此Region没有外部Provider，由本Run `RegisterRegion`（本RunAnchor、真实World ContextGeneration、盒[-400,-400,-100]..[400,400,300]）注册/撤销，正常与重复冲突都可实测；其它World/PCG页由宿主以它自己的明确Actor/句柄提供[-2000,-2000,-500]..[2000,2000,500]区域，避免PCG中心与Bounds落出区域。**同一Region不能同时由宿主和本Run注册。** 页模式切换应在专用World重建/明确Host交接时处理；在根尚未提供这套Host前，这些是可补宿主阻塞，不是API缺实现。初始化加载未完成时读取bDefinitionLoaded、bMapIdentityMatched和ContextGeneration，不能延时若干秒就标Ready；初始快照/区域事件/显式操作更新UI。

流送页：保存独立小关卡 `/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoStream`，真实UWorld，包含一个可见Engine Cube，放在右侧观察区。以传统Streaming Level登记到**专用开发Hub副本/当前开发Hub**的关卡列表，不改正式地图；初始不要求载入和可见。参数是AssetPath `.L_DBA_PluginDemoStream`，适配器取其完整包名，Loaded/Activated、Timeout秒实际进入RequestStreaming；Snapshot调用GetStreamingState，Cancel只撤自己的需求，不声称卸掉其它使用者需要的资源。只有Request受理不等于可见/Ready。

天气/光雾：`DA_DBA_ManualWeather`，类`/Script/GamePlatformWeatherRuntime.GamePlatformWeatherPresetDefinition`；LogicalId=`weather.demo.rain@1`，DataVersion=1/1，RequiredDefinitions空；Weather.State.Type=LightRain，RainIntensity=.4、Wetness=.4、PuddleAmount=.2、SnowIntensity=SnowAmount=0、TemperatureCelsius=20、FogIntensity=.1、WindIntensity=0；WindYaw=0/GustStrength=0/GustFrequencyHz=0；Weather MinHoldSeconds=MaxHoldSeconds=3、TransitionSeconds=1、Weight=1。调用ValidateDefinition成功后保存/重载。此资产只有数值；可见雨雪/可听输出需复用根已有Presentation目录/真实Weather语义提供者，禁止另建播放器。

天气页只接受**未激活/无调度**Runtime + Surface六天气字段为默认(0,0,0,0,0,20)且Wind四向量/Seed默认、无NativeWindSource的开发World；已有非默认表面或原生风源不接管。ProjectionScope检查自身上次写入，检测到外部后继字段/风后停止覆盖该组，清理保留后继。AtmosphereScope只接管未启用光雾Profile，恢复原禁用Profile；临时Fog归既有Atmosphere对象，候选构造同步Stop以配置代次拒绝旧回写。RuntimeScope精确撤自己创建的Replicator/计划，客户端空快照取消既有表现。不能把此窄能力写成任意生产天气快照/调度恢复已实现。

水域：sampling/presence由适配器创建本Run `/Script/GamePlatformWaterRuntime.GamePlatformWaterRegion`，Transient，RegionId=`Manual.Water.<RunGuid>`；Box水平半径300cm、Z半径Depth至少200cm；Actor原点(0,0,0)、Yaw参数；SurfaceOffsetCm=100、ShallowDepthCm=60、FlowSpeed=20、Priority=20。真实子系统Register/Query/Unregister；presence在自有Actor上创建真实WaterInteractorComponent，FootOffset/Interval参数，订阅动态事件，组件自身合法权威采样调度不是UI轮询。sampling可测(0,0,90)浅水、(0,0,0)深水、区外/旋转窄边；不写WaterState。

visual页还需专用Host自有的真实WaterRegion覆盖(0,0,100)，以及可见原生WaterBody/水面材质；现有Floor不自动变成水。`DA_DBA_ManualWater`类`/Script/GamePlatformWaterClient.GamePlatformWaterPresentationPreset`：RippleSemanticTag=`Presentation.Water.Ripple.VFX`、SplashSemanticTag=`Presentation.Water.Splash.VFX`、MaxRippleSpawnsPerSecond=18、MaxViewDistanceCm=6000、MinimalTimeBetweenSamePointSeconds=.13、bEnableSplash=true。两标签必须真实注册并经唯一Presentation→VFX Catalog指向已加载的可用Definition/系统；根先复用现有可用资源，不重复新建基础播放器。预设有效/RequestLocalRipple true仍是受理，须固定镜头动态视频和GPU核验。Clear归还配置/预算/质量，已受理Instant表现由原VFX有限寿命管理，不能宣称立即逐粒子撤回。

PCG有两个不同执行策略的夹具，不能用编辑器静态Profile冒充运行装饰生成：

1. `DA_DBA_ManualPCGRuntime`：类`/Script/GamePlatformPCG.GamePlatformPCGProfileDefinition`；LogicalId=`pcg.demo.cosmetic@1`；DataVersion=1/1；TemplateId=None、TemplateVersion=0、RequiredPCGSchemaMajor=1；RegionId=`world.demo.region@1`；ExecutionPolicy=RuntimeCosmetic、OutputUsage=Cosmetic；HalfExtent=(500,500,50)、Spacing=100、Density=1、UniformScale=.2、Seed=1、MinimumOutputs=1、MaximumOutputs=4096、TimeoutSeconds=30。GraphReference必须是真正Legacy四节点图，OutputMesh=`/Engine/BasicShapes/Cube.Cube`。可由已存在的原生CreateDevelopmentAssets产生图后，通过引擎原生复制到本夹具职责目录并审查；不得直接复制文本或改扩展名。四节点必须是PCGCreatePointsGridSettings.Out→PCGTransformPointsSettings.In→PCGDensityFilterSettings.In→PCGStaticMeshSpawnerSettings.In，Spawner.Out→Graph Output.Out；实际引擎Pin labels以`PCGPinConstants::DefaultInputLabel/DefaultOutputLabel`为准，冷读Edges两端指针，不能仅信AddEdge返回非空。Grid半径(500,500,0)、Cell(100,100,100)、OriginalComponent、CellCenter、CPU；Transform无位移/统一缩放/无Attribute旁路；DensityFilter排零；WeightedSelector一个Cube、权重1、NoCollision、无Overlap/Nav/Replication，组件类型ISM。ValidateProfile及ValidateApprovedGraph均成功，再通过真实RequestGeneration/SubscribeGeneration获得输出数量与指纹；Stop等待Cleaned才释放源租约。

2. `DA_DBA_ManualPCG`（空间/Inspection）：同类，LogicalId=`pcg.demo.spatial@1`；TemplateId=`TPL_ScatterSurface`、TemplateVersion=1、SchemaMajor=1；ExecutionPolicy=EditorGeneratedStatic、OutputUsage=Cosmetic；Region同上，其余数值如上；MeshSetDefinitionId=`GamePlatformDefinition:pcg.demo.meshset@1`且列在RequiredDefinitions；GraphReference绑定非模板Realized图，OutputMesh可空。MeshSet类`/Script/GamePlatformPCG.GamePlatformPCGMeshSetDefinition`，LogicalId=`pcg.demo.meshset@1`、DataVersion=1/1，Entries一项Mesh=EngineCube、Weight=1、bStaticCollision=false、bCastShadow=true。用原生`CreateDevelopmentRealizedGraphAsset`在其强制目录`/Game/Development/Foundation/PCG/Realized/PCG_DBA_ManualSpatial`首次创建；Profile、MeshSet及PCGGeneration资源租约都应先加载，生成器会验证唯一MeshSet/SchemaValidator/真实WeightedSpawner，保存失败不可继续。根通过Monolith调用现有反射EditorLibrary；不直接include它的Private头。

空间图的确切节点合同：已有`PCGDevelopmentGraph.cpp::CreateFoundationTemplateGraph(TPL_ScatterSurface)`产生CPU CreatePointsGrid→`/Script/GamePlatformPCG.GamePlatformPCGWriteSchemaDefaultsSettings`→**恰好一个**`/Script/GamePlatformPCG.GamePlatformPCGSpatialCarveSettings`→`/Script/PCG.PCGDensityFilterSettings`→`GamePlatformPCGProjectAlignSettings`→`GamePlatformPCGApplySpawnPolicySettings`→`GamePlatformPCGAssignMeshSetSettings`→`GamePlatformPCGValidateSchemaSettings`；Realized creator在Validator与Graph Output之间插入真正`/Script/PCG.PCGStaticMeshSpawnerSettings`。通用点边全部Out→In，Graph Output取`DefaultOutputPinProperties()[0].Label`，不要猜默认Input为In而把最后边接错。Grid输出本身自采样，无需伪造Graph Input数据。所有Settings enabled、debug=false、CPU，禁止未知节点/子图/GPU/参数旁路。

Carve实际字段：SubjectPriority=0（页参数范围0..100）、ExcludeThreshold=.5、Masks初始空。DensityFilter必须LowerBound=.001/UpperBound=1、bInvertFilter=false、bNormalizeOutputDensity=false、bKeepZeroDensityPoints=false；Carve只把排除点Density设0，缺这个真实过滤边会让WeightedSpawner仍生成排除点。页操作在**本Run瞬态克隆**图写Masks，源Graph/Profile不会被修改或保存。真实Director注册真实ExclusionActor（Transient、SourceId由OnConstruction、DomainId=ManualLock、CarvePriority=100、Strength=1，Bounds默认150cm半径，位置/半径来自页参数）；CollectSpatialMasks须返回一个闭合四顶点盒面来源。Register/Unregister结果和Actor归属都保留，清理失败不丢对象/租约。

空间正向验收：原生保存重载后检查唯一Carve、DensityFilter与全部真实Edge；加载Profile的PCGGeneration束递归持有Graph/MeshSet/Mesh；ValidateDefinition、ValidateApprovedGraph、InspectProfileSource均成功。以相同Seed在根专用编辑器Bake宿主生成无Mask基线，再在盒[-150,150]²注入Mask后**真实Generate完成委托**，独立枚举PCG受管ISM，排除区内实例数必须0且区外仍有实例；比较真实输出指纹，清理全部输出后保存/重开/复核。`ConfigureSpatialMasks`只完成克隆图注入，**本适配未提供永久Bake发布执行器**；根现有PCG构建/Bake入口、清单与重开证据仍是必做项，不能用参数写入成功签收Bake。Bake、临时、Stage绝对路径均在当前工作空间。

导航：`DA_DBA_ManualNavAgent`类`/Script/GamePlatformNavigation.GamePlatformNavigationAgentProfile`，ProfileId=`Manual.Humanoid`、AgentRadius=42、AgentHeight=192、StepHeight=45、MaxSlope=44、SupportedNavDataId=None、DefaultFilterId=`Navigation.Filter.Default`、InvokerPolicy=RegisterWhenActive、TileGenerationRadius=3000、TileRemovalRadius=5000、DefaultPartialPathPolicy=RejectPartial、Version=1。适配通过Data普通资源租约加载，仅在无同IDProfile时注册；精确对象撤销，不删除其他Profile或其Invoker。根在专用开发Map添加真实NavMeshBoundsVolume覆盖Floor和±200cm查询点，以UE导航构建流程生成真实RecastNavMesh；固定Floor碰撞必须实际阻挡导航，不能通过伪造路径或更改NavDataMissing错误补成功。Invoker正向需要项目Settings允许Invoker，并使用支持runtime generation的真实NavData。

SmartLink页创建真实`AGamePlatformNavigationSmartLinkProxy`，NavLinkId=本Run、TraversalType=参数、SmartLinkComp两端±Distance/2且BothWays。真正AI遍历另需根可补宿主：真实`ACharacter`（Capsule/NavAgent与42/192相容）、真实`AAIController`与PathFollowingComponent、烘焙NavData及可达Link端点；沿Navigation真实FindPath/MoveTo进Link，真实OnSmartLinkReached/平台NotifyTraversalRequested后由项目Traversal handler完成并调用CompleteTraversal成功/失败。当前页没有MoveTo/跳跃执行器，不把CreateLink或Notify事件本身冒充遍历成功；不瞬移DefaultPawn伪造路径通过。

交互：页通过中央Data加载Engine Sphere，创建本Run可见40cm球和真实Visibility射线碰撞、InteractableComponent；自有InteractorComponent添加到同LP真实PlayerController，拒绝接管已有组件。Focus由真实Camera/鼠标Trace；BeginFocusedInteraction仍走真实资格、距离/LOS/目标Revision/Generation与Server RPC链。Hub无ServerActive时正常应拒绝。根下一Gameplay子批的成功宿主须复用`AGamePlatformGameModeBase`（准入Authority、SubmitVerifiedAdmission、AcceptPreparation、既有Experience Begin/Factory准备）和真实GamePlatform玩家/化身Eligibility链，不能页内SetServerPlayerActive或伪造VerifiedAdmission。具有真实Active的PlayerState/AvatarGeneration后，同一页outcome的CommitKind可选Toggle/Consume/Harvest/External；Custom需要实际`IGamePlatformInteractable`处理器，普通Actor会真实拒绝。外部保留/消费/取消只对本Run权威目标，不能发放真实背包/经济权益。正向资格宿主当前属于可补宿主，不能标已完成。

Telemetry无需资产/端点/凭据。真实GI Telemetry的开发capture只允许空缓冲、无在途sink/flush/config、空用户会话/世界上下文；GetHealth外部调用期间封锁重入写入并重核生命周期/sink/context代次。保存精确原sink对象（含真正空指针）、原空Buffer/RateLimiter/Schema、Context/Seed/Enabled/Trace；原sink不Flush/Shutdown。临时同步计数sink只接同一平台执行器的固定合成事件/Schema指标，无网络、不存事件属性；公共非Scope记录/配置在开发租约期间拒绝，避免混入外部数据。End精确归还原对象；GI最终销毁才按正常生命周期Shutdown两个拥有对象。不能把此夹具称生产遥测网络投递/恢复验证。

DeveloperTools：`DA_DBA_ManualReview`类`/Script/GamePlatformDeveloperTools.GamePlatformReviewCase`，CaseId=`Manual.World.Pending`、Category=`Manual.World`、CaseVersion=1、FeatureId=`GamePlatformDeveloperTools.human_review`、ScenarioId=`GamePlatformDeveloperTools.human_review.Manual`，中文Description/Preconditions/Steps/ExpectedResult填写当前页合同，RequiredEvidence用真实`state_dump`/`screenshot`/`log`合同支持项；CreatePendingReport必须NotRun，不调用ApplyHumanDecision。没有真实运行/二进制索引时ValidateReviewEvidence应失败，根完成证据后通过正式独立导入流程绑定，不导入人工Passed。

`DA_DBA_ManualPerformance`类`/Script/GamePlatformDeveloperTools.GamePlatformPerformanceProfileAsset`，Profile.ScenarioId=`Manual.World.Bounded`、Map=实际Hub、ServerRole空、PlayerCount=1、BotCount=0、Duration=5、Warmup=1、Metrics至少`GameThreadMs`；数值是夹具输入，非预算。**缺实现**：尚未发现注册此Manual.World场景的真实性能Executor。适配只调用真实Registry/Runner，未注册明确失败；不得安装返回固定数组或睡眠计时的假Executor制造P95。真实Executor若后续实现，须按引擎实测/可取消场景和受控宿主独立交付。资产/规则报告仅写ProjectSaved/Validation本Run目录；审查是静态/合同，不代表GPU或人工验收。


## Fix1 负向资源与测试前置（不能计入正向可发布资产）

根已报告并保存：World/Region/Weather/Water/NavAgent/Review/Performance七资源；Runtime PCG四节点图/Profile、MeshSet、Spatial源Profile；真实NavBounds/Recast及流送子关卡。对应根证据为T05.World.Fixtures.Authored.json、T05.World.PCGFixtures.Authored.json、HubNavigation.Authored.json、HubStreaming.Authored.json。本文只读了报告，未调用Monolith；冷重载/本候选原生编译运行仍须根核验，保存/Validate并非本Fix1 native通过。

新增负向回归使用下列**真实UE资产**，根已报告Monolith保存至T05.World.PCGNegativeFixtures.Authored.json；负向不Cook、不计正向全通过。两Profile均为`/Script/GamePlatformPCG.GamePlatformPCGProfileDefinition`，Fixtures唯一General扫描继续复用，不加资产管理器。

1. `/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCGMissingDependency.DA_DBA_ManualPCGMissingDependency`。LogicalId=`pcg.demo.missing_dependency_fixture@1`；DataVersion=1/1；RegionId=`world.demo.region@1`；TemplateId=None/Version=0；SchemaMajor1；ExecutionPolicy=RuntimeCosmetic、OutputUsage=Cosmetic；GraphReference复用真实`/DBAUIPack_Core/Development/PluginDemos/Fixtures/PCG/PCG_DBA_ManualRuntime.PCG_DBA_ManualRuntime`，OutputMesh=EngineCube；HalfExtent(500,500,50)、Spacing100、Density1、UniformScale.2、Seed1、MinimumOutputs1、MaximumOutputs4096、Timeout30；RequiredDefinitions唯一一项`GamePlatformDefinition:pcg.demo.required_missing@1`，刻意不创建此依赖。LogicalId/PrimaryAssetId的ImportText需保留完整带引号的`@1`，按根作者已加的精确断言核对。

   普通AcquireResources(Profile)必须成功；测试再以外部资源租约加载其Graph/Mesh，验证实际常驻。此运行AcquireDefinition/PCGGeneration递归依赖应失败；第二次同路径Load必须以Demo.World.DefinitionNotReady阻止Apply，即便外部资源常驻。Reset只释放本运行失败/成功租约，随后合法ManualPCGRuntime必须取得自己的成功完整束才Apply。此测试不在运行中改共享Definition/补造缺失依赖；同路径失败后的资产修复/重新发布另走正式作者/新世界流程。

2. `/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCGInvalidGraph.DA_DBA_ManualPCGInvalidGraph`。LogicalId=`pcg.demo.invalid_graph_fixture@1`；其余合法数值/Region/策略/OutputMesh同上；RequiredDefinitions空；GraphReference为同目录`PCG_DBA_ManualInvalidGraph.PCG_DBA_ManualInvalidGraph`，原生`/Script/PCG.PCGGraph`、bIsTemplate=false，保留默认Input/Output，GetNodes=0、实际Edges=0，无假节点，不Generate。Data定义与PCGGeneration束应成功；ConfigureOwnedComponent的原生ValidateApprovedGraph应明确返回`UnsupportedGraphShape`，且组件归属保留到Stop。仅Python bool-out桥的None不作为此精确错误码证据。

正向重配使用此前`DA_DBA_ManualPCGRuntime`及真实四节点图。回归顺序Configure→重复拒绝（仍同一个组件/没有新Data需求）→Reset（Anchor组件为0、源束撤销、foreign仍在）→Configure（准确一个新组件）→Reset→切合法定义但非法图的负向Profile→真实配置失败→Stop（组件/克隆图Owner清理、全状态记录回到基线）。清理确认前保留组件/图和完整Data句柄。

交互目标：focus/session不含Charges参数，解码保留Charges=3。CreateTarget走真实Data EngineSphere、Standalone权威Actor/Interactable，正常只产生一个可用目标，重复必须拒绝且不增加数量，Reset后能重建。Outcome的显式Charges=0是负向输入，在产生目标前拒绝，不用它污染focus/session正常默认值。当前目标创建回归不声称已经完成ServerActive/真实交互提交或联机验收。

SmartLink：native Spawn回调触发Stop时，源运行保持ManualHost并返回SpawnInFlight，旧命令候选准确销毁后返回Cancelled；再Stop才成功。Native回归在同一真实World启动后继adapter，确认旧Stop重试不销毁后继Link或释放其完整Host Token。若候选销毁失败，RetiringLinks保留精确Actor供重试，不接管后继。

Telemetry instrumented回归只在空真实用户/世界上下文、空Buffer/无调度/无在途的独占WorldLive GI执行。测试friend先把空闲slot从精确外层原sink换为已经Start一次的Probe，外层原sink只暂停且不会Flush/Shutdown；同服务的capture再将这个Probe当原sink验证身份/0调用计数、pending/buffer拒绝、GetHealth重入、disabled基线恢复和真正空slot恢复。测试最后精确归还外层原sink/Enabled/Seed，不用ConfigureSink重启旧对象，不初始化/Deinitialize第二个GI。合成记录确实发生，累计诊断单调保持真实；不把它写成生产网络retry测试。
