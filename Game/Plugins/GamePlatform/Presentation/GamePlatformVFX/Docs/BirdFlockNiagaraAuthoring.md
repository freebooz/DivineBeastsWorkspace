# BirdFlock Niagara（鸟群GPU粒子）待实施节点合同

本文件是 **Niagara 实际制作图谱规范**，不是已存在的 `.uasset` 资产列表，也不是 GPU 效果完成证明。
本任务由于现有 UE Live Coding 会话限制及 UE5.8 Python 对 NiagaraEmitter/Renderer 图编辑接口暴露不足，尚未安全生成可验证的真实鸟群 Niagara System。后续可在编辑器解锁后由 Monolith 编辑器能力或 C++ GamePlatformVFXEditor 资产作者实现，并需要实际UE版本反射验证。

## 一、方案边界

- 平台通用层 `GamePlatformVFX` 保持无具体物种引用：运行时参数合同、飞行数学模块、Niagara通用 GPU emitter 子图、Niagara性能和编译校验资产。不能在平台层硬引用任何 `DBAPresentationPack_Core` 资源。
- 项目层 `DBAPresentationPack_Core` 的 Seagull（海鸥）、Swallow（燕子）、Eagle（雄鹰）各有1个轻量 Niagara 系统变体，只配置项目 StaticMesh、对应 MI、基础速度、不同振翅频率与数量上限。
- `GamePlatformPresentation` 仍负责语义映射；`GamePlatformVFXWorldSubsystem` 只通过已有 `DefinitionId` 和Data Lease启动/取消真实 VFX；不得另外维护第二个表现请求路由器。
- ISM Actor 与 Niagara 同属通用鸟群表现机制的 **两种互斥实现后端**。实时场景只能启用一种，避免重复生成鸟群；项目Actor用于可配置原型和编辑器预览，正式 GPU Niagara 完成后可按性能和画质切换后端。专用服务器不使用任何鸟群视觉后端。

## 二、通用 Niagara 图谱

建议插件资源目录 `/GamePlatformVFX/Environment/BirdFlock/Niagara`：
- `NM_GP_Bird_Initialize`：初始化个体随机种子、方位、半径、飞行高度和粒子尺度。
- `NM_GP_Bird_Orbit`：依赖速度、转向上限、目标中心的环绕飞行与轻微高度抖动。
- `NM_GP_Bird_Traverse`：在封闭的体积内从一端飞向另一端，离开边缘平滑重入，不在观察视野中央瞬移。
- `NM_GP_Bird_FigureEight`：连续八字路径和个体相位差。
- `NM_GP_Bird_Wind`：只接收向量输入并作有界扰动，不向Gameplay记录风力事实。
- `NM_GP_Bird_AlignOrientation`：速度对齐模型前向X，限制Pitch/Roll，避免翼展翻滚。
- `NS_GP_BirdFlock_Base`：平台中立Niagara系统，不加载项目鸟种模型。具体Renderer Mesh数组归项目变体。
- `NET_GP_BirdFlock_Ambient`：Ambient环境Niagara EffectType，远景剔除、质量级预算和FixedBounds审查。

建议 GPU 发射器 Stack（堆栈）：

```text
Emitter Spawn
  Set GPU Sim / Persistent Id Optional（GPU模拟/稳定个体身份可选）
  Set Fixed Bounds（显式固定包围盒）
Emitter Update
  Maintain Ambient Budget（环境预算）
Particle Spawn
  Initialize Particle（粒子初始化）
  Spawn Burst Instantaneous = User.BirdCount（一次生成可配置鸟数）
  NM_GP_Bird_Initialize（随机偏移、高度、个体相位、尺寸）
Particle Update
  NM_GP_Bird_Orbit / Traverse / FigureEight（通过Mode控制飞行轨迹）
  NM_GP_Bird_Wind（局部风扰动）
  NM_GP_Bird_AlignOrientation（姿态朝向/转向侧倾）
Render
  Mesh Renderer（项目物种网格）
  Material: MI_DBA_Bird_{Species}（项目材质）
```

不得为了每只鸟创建独立 Niagara System、SkeletalMeshComponent、Tick Actor 或长生命周期物理刚体。鸟群是环境视觉，不应启用逐鸟碰撞和服务器位置复制。

## 三、必须实现的用户参数协议

通过现有 `FGamePlatformVFXParameterSchema` 的 `Rules` 为每种项目定义配置以下稳定属性：

| 参数 | 类型 | 有效范围/单位 |
| --- | --- | --- |
| User.BirdCount（数量） | Integer（整数） | 0–512；0取消可见实例，无残留粒子 |
| User.FlockRadius（群飞半径） | Float（浮点） | 200–100000cm |
| User.FlockHeight（中心高度） | Float | 0–100000cm，世界锚点局部偏移 |
| User.FlightSpeed（飞行速度） | Float | 10–5000cm/s |
| User.WingBeatHz（振翅频率） | Float | 0.1–10Hz；材质GPU变形 |
| User.WindInfluence（扰动幅度） | Float | 0–2000cm |
| User.WindDirection（风向） | Vector（向量） | 有限数值，组件内归一化 |
| User.FlockSeed（随机种子） | Integer | 允许int32，保证可复现 |
| User.FlightMode（轨迹模式） | Integer | 0盘旋/1掠过/2八字；未知值拒绝 |
| User.FlockCenter（世界中心） | Position（大世界坐标） | 使用LWC位置绑定，禁止以float位置精度丢失 |

`User.BirdCount` 初始 Burst 配置不应由每帧SpawnRate连续生产，否则512上限可能演变为每秒512；动态数量变更时正确 Kill/Respawn或调整有效实例数，必须测试增减、0与World EndPlay取消。
`User.FlightMode` 是项目飞行模式参数而非类型；鸟种具体网格由Definition引用的Niagara资源变体决定，不能把项目鸟种硬编码为平台枚举。

## 四、项目资源与Definition

计划三个项目 Niagara 系统：
`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_Seagull`
`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_Swallow`
`/DBAPresentationPack_Core/VFX/Environment/BirdFlock/Niagara/NS_DBA_BirdFlock_Eagle`

项目中的 `DA_DBA_VFX_BirdFlock_{Species}` 必须继承现有 `UGamePlatformVFXWorldDefinition`，引用相应系统和 `NET_GP_BirdFlock_Ambient`，并注册对应 `GamePlatformData` 统一身份。例如：

- `presentation.dba.environment.birdflock.seagull@1`
- `presentation.dba.environment.birdflock.swallow@1`
- `presentation.dba.environment.birdflock.eagle@1`

`MaxLifetimeSeconds=0` 表示由World/Presentation生命周期明确取消而非无限留存；`bAutoDestroy=false`、`bEnableScalability=true`、`bAllowPooling=true` 在真实资产中检查。需要按当前平台Editor Validator要求验证Niagara EffectType一致性、FixedBounds及DataAssetSchema。

上述路径和逻辑ID均为**待创建预期**，除非检查到实际UE资产写盘/回读日志，不得声称已存在。

## 五、分步落地与门禁

1. 确保当前其他任务的UE编辑器安全结束Live Coding，完成正式UE5.8 Editor Win64 Development编译。
2. 通过Monolith/C++编辑器插件而非脚本伪造创建平台 Niagara 模块、基础系统、EffectType。
3. 在项目内容包中以相同Mesh Renderer绑定分别制作三个 Niagara 变体和项目Definition，套用数量/飞行参数校验。
4. 运行 UE Niagara GPU Shader 编译、资产重载、GPU预算统计；重点测试512只、不停生成/停止、停顿和关卡卸载。
5. 配置 Presentation Catalog 的环境语义到DefinitionId映射，再在Village/OpenWorld/角色选择和创建用项目预设调用正式表现请求并在世界切换时取消。
6. 对照同物种 ISM 后端和 GPU Niagara 后端视觉/CPU/GPU成本，优选正式实现，避免双重发射。
7. 最后在真实编辑器演示与目标客户端分辨率人工检查，Cook/Stage以及Dedicated Server剔除验证通过后才能标记功能完成。
