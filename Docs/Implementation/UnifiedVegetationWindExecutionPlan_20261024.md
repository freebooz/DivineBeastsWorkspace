# SSLM统一植物风场 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新手村全部植物共享世界空间主风向与阵风，保留树种、枝叶和实例间的自然运动差异，并通过真实双客户端验收。

**Architecture:** GamePlatformWeather拥有唯一服务器天气事实；既有WeatherClient把风快照投影到GamePlatformSurface的世界级MPC。ST9适配材质与普通植物WPO共同消费该MPC；DBAFoliagePack_Core只拥有植物材质与美术调校，Village包拥有天气预设及场景装配。不新增风场Subsystem、播放器、逐株Tick或插件。

**Tech Stack:** 锁定UE5.8.0、C++/UHT、原生天气复制Actor、Material Parameter Collection、SpeedTree9、WPO、ISM/HISM、直接Monolith MCP、现有Foundation构建/烘焙工具。

**Spec:** 本文件“设计合同”；依据本对话已给出的统一风场方案。状态为**执行计划，尚未实施**。本次只新增计划和目录登记。

## Global Constraints

- 遵循工作空间`AGENTS.md`：三层单向依赖、插件化、唯一资源所有者；禁止HWebCodex及绕行代理。原生资产只经直接Monolith创建、编译、保存、重载。
- 执行默认采用当前对话单执行者串行推进，不自动启动子代理。每阶段形成独立可审阅差异；不得把用户或其他工作已有的未提交修改纳入提交。
- 2026-10-24规划时仓库为`main / 60a5514a`且大量未提交修改；这不是未来执行基线。执行前重新读取实际状态、局部规则、源码和当前编辑器。
- 保护人工最新位置、旋转、非均匀缩放、PlayerStarts、地形、道路、水体、PCG种子及实例。数量采用执行前实时快照，不固定成历史107棵树、6株柳树或某次蕨类数量。
- 不改变原图RGB/Alpha、PBR、碰撞、导航、伤害、服务器移动、积雪和音乐；茅屋虽经SpeedTree导入，仍属刚性建筑，明确排除植物风接线。
- 所有CPU风状态更新归既有世界服务和生命周期事件；GPU计算持续摆动。不得给每株植物、Widget或ViewModel增加风轮询。
- 固定Win64/Development、`-NoSharedPCH -NoUBA -MaxParallelActions 8`；Editor另加`-UsePrecompiled -AdditionalPlugins DBAServer`。只在真实失败证明必要时调整并记录；不Clean/Rebuild或清理旧包。
- 新增反射字段后必须取得**完整Editor构建证据**，再制作资产和Cook；Client/Server成功不能证明Cook使用的Editor DLL最新。
- 发布窗口内冻结源码、配置、地图和资产输入；并行改动使证据失效时先协调保存窗口，禁止无变化重复烘焙、忽略指纹或关闭网络校验。
- 正式验收分开报告：静态审查、原生资产、编译、Cook/Stage、实际服务器身份、真实准入、GPU测量、人工视觉。计划文件和参数存在不等于完成。

## Review Focus

1. 同一植物随机Yaw、不同实例缩放后仍朝同一世界风向受力；HISM使用每实例变换，不能使用组Actor变换。由任务4、6验证。
2. 垂柳下垂叶片可能低于模型原点；不能用全模型高度遮罩把这些叶片错误固定。根部锚定且建筑完全静止。由任务1、4、6验证。
3. 359°→1°走最短2°，频率改变、连续打断天气过渡时相位不跳变。由任务2、3、6验证。
4. 迟加入、换World、MPC异步重绑和迟到回调不能污染新世界；降雨/积雪写入不能抹掉风。由任务2、3、8验证。
5. PCG再生成、LOD及材质覆盖不能漏接统一风；装饰组件及新材质不得泄漏进Dedicated Server。由任务5、7验证。

## 设计合同

### 已核实的现状

- 柳树Atlas、BostonFern和WesternSwordFern叶材质均继承`/SpeedTreeImporter/SpeedTree9/SpeedTreeMaster`；被检查实例的Shared/Branch/Ripple摆动与速度参数均为0，WindDirection为默认+X。
- `FGamePlatformWeatherState`仅有`WindIntensity`，现有Surface参数合同也没有风向或风快照。
- 原型树作者脚本只连接BaseColor/Roughness/Metallic。单纯放一个WindDirectionalSource不能补齐上述材质、权重和共享数据链。
- 本机ST9导入器把枝条1数据放在UV1/UV2、Lightmap放在UV3；枝条2可占UV4/UV5。必须保留这些通道及顶点色合同。参数为0不等于源文件没有风权重。

### 唯一数据与坐标合同

1. 延续`FGamePlatformWeatherState::WindIntensity`（0..1），新增`WindYawDegrees`（有限数且0≤值<360）、`WindGustStrength`（0..1）、`WindGustFrequencyHz`（0..2Hz）。0°表示风**吹向**世界+X，90°吹向+Y；风向不是气象学“来自”方向。
2. 量化状态保留原`Wind`字段；新增`WindYaw`为uint16整圈编码、`WindGust`为uint8、`WindGustMilliHz`为uint16。Yaw循环量化最大角误差0.003°；频率最大误差0.0005Hz。旧字段不改名，不手改生成代码。
3. 既有快照增加`WindSeed`（uint16，首版默认2026，世界激活后保持）及`WindPhaseAtStartCycles`（0≤值<1周期）。增加纯值方法`float FGamePlatformWeatherSnapshot::SampleWindPhaseCycles(float ServerTimeSeconds) const`，返回积分与起始相位相加后取模的[0,1)相位。
4. 改频率时保持相位：令t=max(0,Now-StartedAt)、T=TransitionSeconds，过渡中积分为`f0*t+(f1-f0)*t*t/(2*T)`；过渡后接`(f0+f1)*T/2+f1*(t-T)`；T=0时使用f1*t。新快照的起始相位先采样旧快照，不归零。频率0冻结相位；强度0使所有植物风WPO归零。
5. 全部植物共用主风向和阵风传播方向；株间只改变响应系数与局部枝叶相位，不随机主风向。材质内部若使用局部坐标，按每实例变换转换方向；最终位移为世界空间。
6. 初次Village样板候选：晴天主风Yaw=0°、Intensity=0.25、GustStrength=0.20、GustFrequency=0.10Hz。这些是待原生视觉审核的起点，不是已批准的最终美术参数；其他天气保留其既有强度，仅补齐方向/阵风配置。

### 材质投影与归属

- 在`GamePlatformSurfaceTypes.h`增加客户端值结构`FGamePlatformSurfaceWindTransition`，保存From/To的Yaw、Intensity、GustStrength、GustFrequencyHz，以及StartedAtServerSeconds、TransitionSeconds、PhaseAtStartCycles、ServerTimeOffsetSeconds、WindSeed；通过`FGamePlatformSurfaceEnvironmentState::Wind`携带。该结构不引用Weather模块类型。
- 既有`IGamePlatformSurfaceService::ApplyEnvironmentState(const FGamePlatformSurfaceEnvironmentState&)`签名不变。WeatherClient增加私有`ApplyWindSnapshot(const FGamePlatformWeatherSnapshot&)`，只在有效快照/首绑定/清空时提交风端点；普通雨雪插值继续合并当前Surface快照，保留Wind字段。
- 扩展现有`/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal`，不创建第二套全局风MPC。追加3个Vector：`GlobalWindFrom=(Yaw,Intensity,GustStrength,FrequencyHz)`、`GlobalWindTo`、`GlobalWindTiming=(StartedAt,T,PhaseAtStart,ServerTimeOffset)`，以及1个Scalar：`GlobalWindSeed`。现有8项Surface标量名称和值保持。
- 平台通用函数归`/GamePlatformSurface/MaterialFunctions/Vegetation/`：`MF_GP_GlobalWindState`求方向、强度、相位；`MF_GP_DirectionalVegetationWind`供普通植物生成定向WPO。时间采用遵守世界暂停的材质World Time；有效天气快照及既有`RefreshPresentationAfterContentActivation`入口用WeatherClient的`GetEstimatedServerTimeSeconds`重新配对时间偏移。保持既有暂停规则；两客户端共享风状态与时间基准，局部枝叶运动不要求像素级同步。
- ST9项目适配归`/DBAFoliagePack_Core/Foliage/Materials/Wind/`：`M_DBA_Foliage_ST9_Master`、`MF_DBA_Foliage_ST9Wind`、`MF_DBA_Foliage_ST9WindMotion`。从引擎对应资产派生所需改动分支，保留其他原生函数引用；不修改引擎SpeedTreeMaster。
- ST9保留枝条锚点和UV权重；普通原型树才使用按真实根部制作的简化弯曲遮罩。树种响应由共享材质实例参数管理，不另建运行时Profile服务。
- 先验证ST9原图、权重、原生WPO和静态风开关。有效权重下恢复树种风曲线；缺失权重时记录具体资产阻塞并补源/制作明确遮罩，不把整树平移当成枝叶风完成。
- 静态网格、FoliageType、Landscape Grass、ISM/HISM、Blueprint组件和LOD/Nanite材质覆盖都进入接线清单。当前资产不在本任务中批量转换Nanite、骨骼植物或更换模型。

## 文件与资产映射

以下缩写只用于任务引用，均为工作空间相对路径，不是新增目录：

| 标识 | 精确路径/职责 |
| --- | --- |
| W | `Game/Plugins/GamePlatform/World/GamePlatformWeather/Source` |
| S | `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Source` |
| F | `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFoliagePack_Core` |
| V | `Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village` |
| WX类型 | W下`GamePlatformWeatherRuntime/Public/Types/GamePlatformWeatherTypes.h`及`Private/Types/GamePlatformWeatherTypes.cpp` |
| WX权威 | W下`GamePlatformWeatherRuntime/Private/Subsystems/GamePlatformWeatherWorldSubsystem.cpp` |
| WX客户端 | W下`GamePlatformWeatherClient/Public/Subsystems/GamePlatformWeatherClientWorldSubsystem.h`及`Private/Subsystems/GamePlatformWeatherClientWorldSubsystem.cpp` |
| Surface值/名称 | S下`GamePlatformSurfaceClient/Public/Types/GamePlatformSurfaceTypes.h`、`GamePlatformSurfaceParameterNames.h`及对应Private/Types实现 |
| Surface绑定 | S下`GamePlatformSurfaceClient/Private/Subsystems/GamePlatformSurfaceWorldSubsystem.h/.cpp` |
| 平台原生资产 | `Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/ParameterCollections/MPC_GP_SurfaceGlobal.uasset`及`Content/MaterialFunctions/Vegetation/` |
| 植物原生资产 | F下`Content/Foliage/Materials/Wind/`、`Content/Foliage/Materials/M_DBA_Foliage_PrototypeTree.uasset`及现有柳树/蕨类MI |
| Village装配 | V下`Content/Weather/Definitions/DA_DBA_Weather_*.uasset`、正式地图实际引用的原型树MI；保留原地图身份 |
| 作者工具（新增） | `Tools/Unreal/Weather/AuthorUnifiedVegetationWindMonolith.py`，仅在Monolith内执行原生资产装配、保存和证据导出 |
| 原生审计（新增） | `Tests/Assets/ValidateUnifiedVegetationWindMonolith.py`，只读核对接线、权重、覆盖、重载和场景保护 |
| 发布审计（新增） | `Tests/Assets/ValidateUnifiedVegetationWindContainer.py`，读取真实Client/Server容器，不根据磁盘源文件判入包 |
| 隔离审核地图（新增） | `/DBAWorldPack_Village/Development/Wind/Maps/L_DBA_VegetationWind_Review`；明确从发行Client/Server Cook排除 |
| 证据 | `Saved/Validation/UnifiedVegetationWind20261024/<RunId>/`；保存输入清单、阶段状态、截图、视频、GPU数据和运行身份，不存凭据 |

## Task 1：建立当前资产基线与最小审核夹具

**Files:** 新增作者/原生审计工具；更新`Docs/Architecture/解决方案总体目录规划说明_V1.3.0.md`；正式资产此时只读。

**Interfaces:** 产生`Baseline.json`：每个植物组件的Actor/组件路径、Mesh/全部LOD材质、实例变换哈希、碰撞/专服加载属性、World身份、原生风参数和源文件哈希。作者工具暴露`inspect()`、`author_platform()`、`author_st9()`、`author_review()`、`bind_village()`、`save_owned()`；审计工具暴露`audit(stage: str)`。

- [ ] 读取当前分支、未提交修改、编辑器工程/World、正在运行的客户端/专服及并行编辑状态；为后续正式资产保存约定短暂独占窗口。
- [ ] 导出所有实际植物与手工场景基线；单独记录PlayerStarts、地形、道路、水体、茅屋与PCG实例；备份本轮将修改的真实包。
- [ ] 原生检查柳树/两种蕨类的UV1/2、可选UV4/5、顶点色、SpeedTree风静态开关和零参数；输出“有效/缺失/无法确认”，不以有UV通道等同有效权重。
- [ ] 在独立审核地图建立一株柳树、两种蕨类、原型树及Yaw=0/90/180/270°的StaticMesh与HISM夹具；加入均匀/非均匀缩放。不得重新生成正式森林。
- [ ] 运行`audit('Baseline')`，预期报告当前未接共享风合同的具体项，同时场景保护通过；标记本阶段差异可审阅，不提交混有用户改动的包。

## Task 2：扩展服务器天气风合同与复制快照

**Files:** 修改WX类型、WX权威；扩展W下`GamePlatformWeatherRuntime/Private/Tests/GamePlatformWeatherStateTests.cpp`。

**Interfaces:** 保留`ActivateWeather`、`SetWeather`等既有调用签名；输出设计合同中的新字段、量化编码和`SampleWindPhaseCycles(float) const`，沿用一个天气Replicator与Revision机制。

- [ ] 先增加`GamePlatform.Weather.Runtime.WindDirectionAndPhase`失败测试：0/90/180/270°方向、359→1°中点=0°；非法NaN/Inf/范围输入拒绝。
- [ ] 增加量化误差与相位测试：起始相位0、f0=0.1、f1=0.2、T=10时，t=5返回0.625；t=15积分增量2.5周期、取模返回0.5；中途替换快照的起始相位等于旧快照当时相位。新增字段前测试应失败。
- [ ] 最小实现编码/解码、最短弧插值、相位积分与PublishTransition锚定；Seed在同一世界连续快照间保持，不以客户端随机数生成权威风。
- [ ] 测试迟加入采样、零时长、0强度、0频率、连续取消、乱序Revision及旧Replicator退出；保留原雨雪量化/调度重入测试。
- [ ] 用完整Editor增量构建后的原生Automation运行`GamePlatform.Weather.Runtime`，结果必须实际执行且0失败；记录新增UE复制字段的版本影响，禁止热混旧端。

## Task 3：把风快照接入现有Surface世界级MPC

**Files:** 修改WX客户端、Surface值/名称、Surface绑定；新增S下`GamePlatformSurfaceClient/Private/Tests/GamePlatformSurfaceWindTests.cpp`；更新既有Surface绑定/状态测试。

**Interfaces:** 新增`FGamePlatformSurfaceWindTransition`及环境值`Wind`；保留`ApplyEnvironmentState`签名；新增WeatherClient私有`ApplyWindSnapshot(const FGamePlatformWeatherSnapshot&)`。Surface仍不依赖WeatherRuntime或项目层。

- [ ] 先增加`GamePlatform.Surface.Wind.StateAndWorldIsolation`：两个World使用同一MPC资产但拥有不同参数实例；同值不重复写入；非法数值整包拒绝。新字段/向量绑定缺失时测试失败。
- [ ] 实现端点与时间数据合并、3个Vector和1个Scalar的原子校验/写入；扩展参数计数，保留原8项标量和雨雪/苔藓/温度结果。
- [ ] 实现快照首次投影、取消清空、服务器时间偏移、恢复/重同步处理；MPC异步回调沿用Data租约和BindingGeneration，不新建资产管理器。
- [ ] 增加`GamePlatform.Surface.Wind.BindingAndWeatherMerge`：MPC晚到、旧代回调、换World、恢复重绑；雨雪过渡不重置Wind端点，风更新不抹掉积雪。
- [ ] 原生Automation执行`GamePlatform.Surface`及受影响WeatherClient测试；审查无逐株Tick/定时器、无静态跨World风缓存。失败只修受影响链路。

## Task 4：制作统一材质函数并完成ST9样板

**Files:** 新增平台Vegetation函数、ST9适配母材质/函数；仅修改样板所用柳树/蕨类MI；扩展作者和原生审计工具。

**Interfaces:** `MF_GP_GlobalWindState`输出DirectionWS、Intensity/Gust包络、连续Phase；`MF_GP_DirectionalVegetationWind`接受每实例位置/锚点、植物权重与位移上限，输出世界WPO。ST9适配保留原BaseColor、NormalRoughness、Subsurface及原生枝条/叶片分层。

- [ ] 用Monolith扩展真实MPC并编译、保存、重载；首次`audit('PlatformContract')`必须发现尚缺的材质连接，完成函数后转为通过。
- [ ] 原生导出SpeedTreeMaster/Wind/WindMotion图，再派生设计合同中的三个项目资产；只替换风输入与需要共享时间的分支，不编辑引擎资产，不覆盖UV/顶点色。
- [ ] 为柳树与两种蕨类恢复非零且可审核的树种响应；先证明Native ST9权重可用。树干轻摆、枝条分层、叶尖响应；垂柳低垂叶片不套统一高度截断。
- [ ] 根部保持固定；每实例世界风方向不随Yaw改变；株间随机仅影响局部相位/柔度。禁止只把SimpleGrassWind当作主方向风。
- [ ] 在隔离夹具分别验证强度0/0.25/1、四个风向、359→1°、10秒过渡及中途打断。保存原生视频和固定相机前后截图；样板通过后才能进入全部植物接线。

## Task 5：接入原型树林及全部实际植物入口

**Files:** 修改既有共享原型树材质、正式引用的原型树MI、ST9植物MI；更新必要的Foliage包依赖、Village天气预设及现有导入/PCG作者入口。茅屋材质排除。

**Interfaces:** `audit('Coverage')`以任务1的实际组件清单为输入，输出每个Mesh/LOD/材质覆盖的统一风来源；树种MI不得自行持有独立可写全局方向。

- [ ] 先写覆盖失败检查：漏接任何植物槽、LOD或Nanite覆盖材质即失败；把含“Grass”的茅屋材质列入明确排除清单。
- [ ] 原型树接入平台定向WPO，按真实根部和树冠响应设计简化遮罩；共享植物母材质成为唯一可复用实现，保持现有资产路径及原配色。
- [ ] 通过Monolith逐项接线现有植物、FoliageType和Blueprint/HISM材质；不拆成独立Actor、不修改散布密度、位置、种子和碰撞。
- [ ] 更新现有Village天气预设的风字段；核对当前GameMode的InitialWeatherPresetId/调度入口，以现有Data租约启用预设，不另建风控制器，也不改雨雪启停选择。
- [ ] 显式检查PCG再生成前后材质合同与实例变换哈希；核对纯装饰组件的专服加载标记/现有类剥离机制仍有效；`audit('Coverage')`零漏接、刚性对象无风位移。

## Task 6：原生GPU、生命周期与质量降级验收

**Files:** 扩展原生审计工具与隔离审核地图；必要的风质量开关归既有材质/伸缩入口，不增预算服务。

**Interfaces:** 输出`NativeAssets.json`、`DirectionMatrix.json`、`Lifecycle.json`、`QualityAB.csv`及原生PNG/视频；请求已受理、参数相等或Actor坐标不动不能代替GPU风动画证据。

- [ ] GUI D3D12下从保存包独立重载，验证StaticMesh与HISM四种Yaw、随机实例及缩放的主受力方向一致；使用相同位移函数的浮点调试渲染或明确可测的正交截图审查根部和方向。
- [ ] 根部位移容差0.1cm、方向/时间纯数值按任务2误差合同；强度0时风WPO为0。保存站立根部、垂柳低垂叶片和茅屋静止对照，不以CPU Bounds推算GPU位移。
- [ ] 通过首快照、迟加入、过渡打断、隐藏/恢复、换World、MPC重绑及重复绑定回归；一次World关闭后旧订阅和在途回调不能恢复旧风。
- [ ] 质量档首版：Epic/High保留分层枝叶；Medium减次级枝条细节；Low保留低成本主弯曲/阵风，减少叶片微颤。远距采用平滑衰减并核对实际组件WPO设置、边界和阴影，不先指定未经测量的全场距离。
- [ ] 在当前真实数量下做风开/关、四档、固定镜头AB，记录GT/RT/GPU、阴影成本、P95/P99和原始采样；柳树约40万三角面/单LOD单独登记。不得挪用技能VFX预算或宣称免费/达标；未有批准门限时只报告实测。

## Task 7：完整构建、双端Cook与产物门禁

**Files:** 复用`Build/Game/BuildFoundation.ps1`、两个Cook入口及`FoundationCookEvidence.psm1`；新增容器审计；为隔离审核目录更新现有客户端NeverCook和服务器排除真源/受管INI。

**Interfaces:** 输出同一冻结输入的Editor/Client/Server完整目标结果、Cook关联Editor证据、真实容器闭包与独立版本清单。新风复制字段不允许与旧端混用。

- [ ] 冻结最终源码、配置、地图和资产清单；比较任务1场景保护基线，采用当前人工编辑，不回写历史位置。新增文件/资产在同次变更中登记总体目录。
- [ ] 按下方已存在入口运行完整Editor、Client/Server增量构建；Editor反射变化后受控保存/重启编辑器再独立加载，保护未保存手工工作。记录真实receipt、DLL和进程结果。
- [ ] 先验证UI颜色报告，再双端Cook/Stage；显式传本次完整Editor构建结果。仅当既有Zen确实不可用时使用SkipZenStore；磁盘不足选独占输出方案，不删除旧包。
- [ ] 容器审计必须证明Client含本次MPC/函数/母材质/植物MI的真实ExportBundleData；Server保留天气权威类型及必要碰撞，但不含Surface风材质、植物装饰和审核地图。PCG再生成后同样验证剥离。
- [ ] 复用既有真实打包GA/CDO序列化回归，保留原UI/声音容器门禁；编译与Cook输入任何变化都重新判定受影响结果，不给旧结果补签。

执行时在工作空间根目录使用PowerShell 7；每个RunId新建且把最终路径写入本轮证据，不把示例路径当成已存在产物：

```powershell
$windEditorRun = [guid]::NewGuid()
& ./Build/Game/BuildFoundation.ps1 -EngineRoot 'D:/UnrealEngine-5.8.0-release' -Editor -UsePrecompiled -AdditionalPlugins DBAServer -NoSharedPCH -NoUBA -MaxParallelActions 8 -RunId $windEditorRun
if ($LASTEXITCODE -ne 0) { throw 'Editor build failed; stop.' }
$windEditorResult = Join-Path (Get-Location).Path "Saved/Validation/FoundationM0/$windEditorRun/Build/result.json"
# 确认完整Editor结果、刷新并重载原生资产后，冻结最终发布输入。
$windGameRun = [guid]::NewGuid()
& ./Build/Game/BuildFoundation.ps1 -EngineRoot 'D:/UnrealEngine-5.8.0-release' -Client -Server -NoSharedPCH -NoUBA -MaxParallelActions 8 -RunId $windGameRun
if ($LASTEXITCODE -ne 0) { throw 'Client/Server build failed; stop.' }
& ./Build/Game/CookFrontEndClient.ps1 -EngineRoot 'D:/UnrealEngine-5.8.0-release' -Cook -CustomConfig VillageDevelopmentClient -EditorBuildResult $windEditorResult -RunId ([guid]::NewGuid())
if ($LASTEXITCODE -ne 0) { throw 'Client Cook/Stage failed; stop.' }
& ./Build/Game/CookFoundation.ps1 -EngineRoot 'D:/UnrealEngine-5.8.0-release' -Cook -Target Server -CustomConfig VillageDevelopmentServer -Maps @('/DBAWorldPack_Village/Maps/L_Village_Start','/DBAWorldPack_MainArena/Maps/L_DBA_MainArena') -Pak -SkipDebugSymbols -EditorBuildResult $windEditorResult -RunId ([guid]::NewGuid())
if ($LASTEXITCODE -ne 0) { throw 'Server Cook/Stage failed; stop.' }
```

每条返回非零立即停止下一条；Editor测试通过并完成资产保存后如源码/配置变化，重新获得完整Editor证据。以上命令不代替原生Automation执行、MCP资产操作或版本/产物审计。

## Task 8：实际部署与两个客户端人工签收

**Files:** 复用`Deploy/Local/StartVillageDevelopment.ps1`、`Deploy/Local/RunMainArenaPool.ps1`及`Build/Game/StartLocalTestClients.ps1`；本计划实施完成后补结果与证据链接。

**Interfaces:** 输出`RuntimeCompatibility.json`、两个客户端PID/启动时间/可执行路径/日志、真实准入日志和人工验收表。不要复用本计划日期以前的PID或静态BuildVersion字符串代替映像证明。

- [ ] 核实真实Village、MainArena实例的PID、映像、启动时间、当前玩家和池监控归属；保留无关服务与数据。仅在可安全排空时更新本轮专服。
- [ ] 核对新运行服务器确实来自本轮Stage、双端共享源码/复制合同及Cook输入一致；地图和必要资源属于同一冻结版本，再启动两个可见客户端，保留测试账号按钮并由用户人工登录。
- [ ] 两客户端真实进入新手村，确认无NetChecksumMismatch、复制解析或Cooked序列化错误；记录两边相同Revision、风向/强度/阵风时间基准。
- [ ] 人工观察当前场景全部柳树、蕨类、原型树林：统一主方向、不同摆幅；四向切换、零风、重新进入世界、一个客户端迟加入；近/远景无根部漂移、穿插加剧、剔除闪烁或阴影异常。
- [ ] 完成结果分层：仅在真实通过后勾选原生资产/构建/Cook/准入/GPU/人工项目；任何缺权重植物、未通过性能档或人工未签收项逐项保留，不写“全部完成”。

## 回退与范围控制

- 源码按本阶段实际差异回退；材质恢复任务1记录的父链/参数；只恢复本任务拥有的风改动，不用旧整图覆盖用户后续人工编辑。
- 协议字段变更时双端一起退回同一套完整构建与Cook；不可只回退一个客户端或服务器。
- 服务器风渲染剥离失败先修引用/装配合同，不关闭NeverCook、网络或Editor构建门禁。
- 默认不扩展Niagara落叶/雨雪运动、布料、湖面、Nanite骨骼植物、LOD重制或其他地图的美术。后续接入者复用同一风合同，不再建立独立风向真源。
- 阶段执行顺序固定为1→2→3→4→5→6→7→8。样板、原生重载或阶段门禁失败时停在对应阶段修复，不先批量覆盖全部植物。

## 计划自审记录

- 已覆盖统一世界风向、ST9与普通WPO适配、根部/垂柳、逐实例旋转、服务器快照、事件投影、World清理、质量、Cook剥离和双客户端人工验收。
- 五项Review Focus均已映射到具体任务及失败/边界检查；方法、类型和MPC参数名前后对应。
- 执行建议为当前对话串行实施，原因是共享天气合同、同一MPC和原生资产保存存在顺序依赖；不默认并行写共享编辑器。
- 本文件不表示已开始实施，不以计划批准代替后续人工视觉签收。

参考：Epic官方[Material Parameter Collections](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-material-parameter-collections-in-unreal-engine)、[WPO材质函数](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-position-offset-material-functions-in-unreal-engine)、[Nanite/WPO限制](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-virtualized-geometry-in-unreal-engine)。具体实现以本机锁定UE5.8源码和原生重载结果为准。
