# BirdFlock Niagara（鸟群GPU粒子）现行制作合同

2026-10-11 P4–P7续接版。平台通用Niagara模块、基础系统、环境EffectType与三种项目变体已由真实UE5.8/Monolith生成并保存。三种WorldDefinition、统一场景蓝图和正式Presentation/Data加载链路已实装；本文件只列当前真实参数与资产，不再沿用早期拆分为六个空模块的设想。

独立P4资产重载、三物种Niagara编译零错误/零警告、15组直接GPU数量以及15组正式PIE链路GPU数量均已通过。完整构建/资源烘焙/长稳与人工验收状态见`Docs/Implementation/BirdFlockP4P7Execution_20261011.md`；不能以这些自动检查代替人工审美确认。

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

当前白名单恰好7项：`User.BirdCount`整数0–512；`User.FlockRadius`浮点半径200–10000cm；`User.FlockHeight`浮点相对高度200–10000cm；`User.FlightSpeed`浮点速度10–5000cm/s；`User.WindInfluence`浮点扰动幅度0–2000cm；`User.FlockSeed`整数0–1000000；`User.FlightMode`整数0盘旋/1拉长闭合掠过/2八字。

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
