# BirdFlock Niagara（鸟群GPU粒子）现行制作合同

2026-10-11 P4–P7续接版。平台通用Niagara模块、基础系统、环境EffectType与三种项目变体已由真实UE5.8/Monolith生成并保存。三种WorldDefinition、统一场景蓝图和正式Presentation/Data加载链路已实装；本文件只列当前真实参数与资产，不再沿用早期拆分为六个空模块的设想。

独立P4资产重载、三物种Niagara编译零错误/零警告、15组直接GPU数量以及15组正式PIE链路GPU数量均已通过。完整构建/资源烘焙/长稳与人工验收状态见`Docs/Implementation/BirdFlockP4P7Execution_20261011.md`；不能以这些自动检查代替人工审美确认。

2026-10-15间距修复状态：下述稳定编队算法、增量资产作者入口和纯位置回归已更新源码；本轮真实Monolith更新、编译保存、独立重载、GPU读回、Cook和人工观察须由主流程重新执行。前段2026-10-11证据不能作为本轮间距修复已生效的证明。

## 一、方案边界

- 平台通用层 `GamePlatformVFX` 保持无具体物种引用：运行时参数合同、飞行数学模块、Niagara通用 GPU emitter 子图、Niagara性能和编译校验资产。不能在平台层硬引用任何 `DBAPresentationPack_Core` 资源。
- 项目层 `DBAPresentationPack_Core` 的 Seagull（海鸥）、Swallow（燕子）、Eagle（雄鹰）各有1个轻量 Niagara 系统变体，只配置项目 StaticMesh、对应 MI、基础速度、不同振翅频率与数量上限。
- `GamePlatformPresentation` 仍负责语义映射；`GamePlatformVFXWorldSubsystem` 只通过已有 `DefinitionId` 和Data Lease启动/取消真实 VFX；不得另外维护第二个表现请求路由器。
- 正式项目场景只使用Niagara后端，旧ISM Actor保留为历史原型和回归夹具，不创建可选执行器/平行资源身份。专用服务器不使用任何鸟群视觉后端。

## 二、通用 Niagara 图谱

实际插件资源目录 `/GamePlatformVFX/Environment/BirdFlock/Niagara`：`NM_GP_BirdFlockMotion（通用鸟群GPU运动模块）` 和 `NS_GP_BirdFlock_Base（中立系统）`；环境性能类型位于兄弟目录`EffectTypes/NET_GP_BirdFlock_Ambient`。模块一次计算稳定个体相位、三种有界轨迹、风扰动、有限高度、模型朝向与轻微侧倾；模型/羽色由项目变体决定。中立系统只引用Engine基本网格，不反向依赖项目。

当前 GPU 发射器 Stack（堆栈）合同：

```text
Emitter Spawn
  Set GPU Sim / Persistent Id Optional（GPU模拟/稳定个体身份可选）
  Set Fixed Bounds（显式固定包围盒）
Emitter Update
  Maintain Ambient Budget（环境预算）
Particle Spawn
  Initialize Particle（粒子初始化）
  Spawn Burst Instantaneous = User.BirdCount（一次生成可配置鸟数）
  Individual random initialization（稳定个体偏移与相位）
Particle Update
  NM_GP_BirdFlockMotion（Mode轨迹、风扰动、有限高度、姿态、尺寸统一计算）
Render
  Mesh Renderer（项目物种网格）
  Material: MI_DBA_Bird_{Species}（项目材质）
```

不得为了每只鸟创建独立 Niagara System、SkeletalMeshComponent、Tick Actor 或长生命周期物理刚体。鸟群是环境视觉，不应启用逐鸟碰撞和服务器位置复制。

## 三、必须实现的用户参数协议

通过现有 `FGamePlatformVFXParameterSchema` 的 `Rules` 为每种项目定义配置以下稳定属性：

2026-10-11真实资产白名单为7项：`User.BirdCount`整数0–512；`User.FlockRadius`浮点半径200–10000cm；`User.FlockHeight`浮点相对高度200–10000cm；`User.FlightSpeed`浮点速度10–5000cm/s；`User.WindInfluence`浮点扰动幅度0–2000cm；`User.FlockSeed`整数0–1000000；`User.FlightMode`整数0盘旋/1拉长闭合掠过/2八字。2026-10-15增量修复将加入第8项可选浮点字段`User.FlockMinimumSpacing`（最小中心距厘米，1–1000）；其定义、默认值与Niagara参数必须在同一轮真实更新，不能只修改HLSL后沿用旧资产。

世界锚点使用平台表现请求的SourceLocation及既有LWC放置流程，粒子在本地空间运动；不是额外User.FlockCenter参数。振翅频率/幅度来自物种材质实例，没有开放User.WingBeatHz或WindDirection协议，不在文档声称不存在的字段已可配置。

`User.BirdCount` 初始 Burst 配置不应由每帧SpawnRate连续生产，否则512上限可能演变为每秒512；动态数量变更时正确 Kill/Respawn或调整有效实例数，必须测试增减、0与World EndPlay取消。
`User.FlightMode` 是项目飞行模式参数而非类型；鸟种具体网格由Definition引用的Niagara资源变体决定，不能把项目鸟种硬编码为平台枚举。

## 四、项目资源与Definition

已保存的三个项目 Niagara 系统：
`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_Seagull`
`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_Swallow`
`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_Eagle`

项目中的 `DA_DBA_VFX_BirdFlock_{Species}` 必须继承现有 `UGamePlatformVFXWorldDefinition`，引用相应系统和 `NET_GP_BirdFlock_Ambient`，并注册对应 `GamePlatformData` 统一身份。例如：

- `presentation.dba.environment.birdflock.seagull@1`
- `presentation.dba.environment.birdflock.swallow@1`
- `presentation.dba.environment.birdflock.eagle@1`

`MaxLifetimeSeconds=0` 表示由World/Presentation生命周期明确取消而非无限留存；`bAutoDestroy=false`、`bEnableScalability=true`、`bAllowPooling=true` 在真实资产中检查。需要按当前平台Editor Validator要求验证Niagara EffectType一致性、FixedBounds及DataAssetSchema。

上述Niagara系统及WorldDefinition均已真实写盘/独立回读。三种Renderer当前引用`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Meshes/Flight/SM_DBA_Bird_{Species}`，不使用早期白色/错误尺寸网格。项目`Blueprints/BP_DBA_BirdFlockAmbient`内部只有一个正式项目场景组件，鸟种、数量和群ID可编辑。

## 五、分步落地与门禁

1. 增量修改平台机制后编译受影响Editor模块，核验编辑器打开正确主工程。已有未保存编辑工作不得强制退出。
2. 通过Monolith更新本任务自己的Niagara图、绑定与配置，按实际错误/警告计数判断编译，再保存真实资源。
3. 独立UE进程运行`ValidateBirdFlockP4Assets.py`，校验平台依赖方向、三种定义/系统/网格/材质及蓝图。
4. `RunBirdFlockGPUReview.py`使用受控单一模拟时钟与真实视口截图，验证三物种各0/1/32/128/512共15组。非PIE编辑器不可把自动Tick与手动AdvanceSimulation叠加，否则可能重复提交Burst；真实PIE按游戏世界时钟执行。
5. `RunBirdFlockIntegrationReview.py`在真实PIE验证蓝图自动生成三群、正式表现链路15组数量、错误输入不污染既有请求、停止和World清理。不能用脚本内构造的结果代替GPU读回。
6. `RunBirdFlockSoakReview.py`按10分钟稳定性窗口验证三群512只，无界性能数据和硬件无关FPS不属于该检查的结论。
7. 最后核查客户端Cook/Stage包含新Flight网格/蓝图/定义/Niagara而专用服务器不包含，人工验收关卡与最终场景后才关闭P7。

## 六、2026-10-15个体间距修复

原运动模块将`Particles.MaterialRandom`作为相位、半径与角速度的共同随机源，没有任何最小距离约束；独立角速度还会让初始分开的个体在后续追近。八字路线存在两个分支交汇，增大分离权重或只检查出生点均不能修复当前已发布0/1/2路径。高级Spline/Boids发布资格仍保持原门禁。

修复直接从现有ParameterMap读`Particles.UniqueID`与`User.BirdCount`，保留原8个模块输入和`Individual`的随机尺寸用途。单次Burst内连续编号模实际数量形成稳定槽位；共享角速度使槽位相位差在整个请求生命周期内不漂移，风扰动和上下起伏也由全群共享。仍使用原盘旋圆、拉长椭圆及八字公式，半径统一为0.9R（原随机半径均值约0.885R），保持原飞行区域、数量和路线形状，不创建逐鸟Actor、CPU更新或邻域扫描。

同层槽位先按真实中心距计算容量。圆/椭圆采用最小轴的弦长下界；八字每层只占半圈，并用`distance >= 0.65R * abs(sin(delta))`的保守下界计算容量。不同层的Z差固定至少一个中心距，交汇时也成立。单层直径不足时容量为1；不能将asin输入夹至1后误认容量为2。底层需要抬高时上移整个编队，不逐鸟钳Z而压掉层间距。

项目候选最小中心距为海鸥220cm、燕子140cm、雄鹰340cm，依据已有翼展155/97/255cm、最大1.11模型Scale、振翅幅度11/7/20cm及可见余量。真实更新入口还会读取已保存网格包围球与材质实例幅度，按`2*SphereRadius*1.11 + 2*WingAmplitude + 15cm`复核，再向上20cm取整，避免只信源规格而漏掉实际模型边界。默认Village海鸥32只仍为一层，燕子24只为两个相距140cm的半圈层；OpenWorld雄鹰5只仍为一层。高数量只增加保证间距所需的固定层数，必须复核其生态安全高度与实际GPU剔除。

待主流程确认正确主工程、锁定Monolith/UE身份并协调编辑器后，使用现有入口：

```text
<已核实的Python解释器> Tools/Unreal/VFX/BirdFlock/AuthorBirdFlockNiagaraMonolith.py --repair-spacing
```

入口预检三物种真实资源，留存旧模块/系统规格后，为现有基础系统及三变体新增参数，调用Monolith `set_custom_hlsl_text`更新原模块，在原@1定义增加可选规则、默认值与内容修订，再逐个编译保存。组件`BirdCount`、正式服务与Definition白名单均真实允许0..512；正式鸟群入口只覆盖Count，模式/半径/高度来自系统默认值，因此固定Bounds按各系统当前真实模式和512只计算，而不为默认盘旋鹰群无条件预留未请求的八字高度。候选参数的四系统均可保持原XY Bounds与Z -1000..12000cm；默认全相位中心高度为海鸥808..892cm、Village燕子458..682cm、雄鹰1358..1442cm。报告同时记录真实默认数量与512只包络、实际模式和最终Bounds，真实网格预检若提高间距则重新计算。模式、半径、高度或间距改动后必须重跑作者与安全体积审计；平台中立请求自行覆盖这些字段时须提供覆盖后Bounds，不能沿用这次默认模式的合同。三种轨迹、数量、逻辑身份、网格与材质引用均不重建。失败即停止并保留逐步证据，不自动重放写动作。

纯回归入口`Tests/Assets/TestBirdFlockReleasePolicy.py`逐句执行真实HLSL位置公式，验证默认群600秒完整相位及1小时/24小时、三模式的2/3/72/128/512只、2000cm风扰动与最小200cm半径/超直径间距。测试是CPU双精度数学证据，不能代替GPU浮点、UniqueID、真实User参数、固定Bounds、画面和Cook验证。真实资产门禁`ValidateBirdFlockNiagaraMonolith.py`要求保存后的模块HLSL与源码一致、三个系统包含新间距字段并且GPU编译状态最新；最终仍需对实际GPU位置和两窗口场景观察留证。
