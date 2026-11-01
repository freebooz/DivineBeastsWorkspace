# 通用飞行运动使用与验证

本功能归GamePlatformCombat；GamePlatformCore只提供只读运动状态接口，MobaCommon无需依赖具体项目资源。平台无Niagara依赖，VFX无Combat依赖。所有位移、Sweep及伤害继续由AGamePlatformCombatProjectile唯一执行。

## 配置和单位

`FGamePlatformCombatProjectileMotion`保留SpeedCmPerSecond等既有序列化字段，新增Flight和FlightProfile。非空Profile优先于Flight；bEnabled=false沿用旧速度语义，但共用同一位移积分。服务器`AuthoritySetFlightConfig(Config)`可在发射前覆盖，发射后返回false；模板和曲线在发射时冻结。

| 字段 | 含义 |
| --- | --- |
| SpeedMode | ConstantSpeed、ConstantAcceleration、AccelerateThenCruise、Decelerate、SpeedCurve互斥 |
| InitialSpeed / MinSpeed / MaxSpeed | cm/s；初始/最低推进速度，最大速度必须为正，约束包含重力及螺旋的总速度 |
| Acceleration / Deceleration | 非负cm/s²；曲线模式不叠加恒定推进 |
| AccelerationDuration / DecelerationDuration | 秒；加速后巡航必须在给定时长内达到上限 |
| SpeedCurve / CurveDuration | CurveFloat横轴0..1，输出MinSpeed..MaxSpeed系数；起点须匹配InitialSpeed，越界输出夹紧 |
| GravityScale | 乘世界重力；重力与选中目标追踪分开配置，重力模式须关闭bEnableHoming |
| bEnableHoming / HomingAcceleration | 保留原目标；0沿用原Bezier，正值提供有限转向能力。冻结的<30°规则仍直接追踪原目标 |
| bRotationFollowsVelocity / bEnableSubStepping | 可选旋转；曲线/重力/螺旋仍强制足够细的安全Sweep子步 |
| MaxFlightDistance / MaxFlightTime | cm / 秒的硬停止预算，不延长原目标移动后的期限 |
| bEnableSpeedBasedVFX | 独立关闭速度表现联动，不影响运动/伤害 |

蓝图使用Make GamePlatformCombatProjectileMotion填入Profile，经已提交技能提供的Spec调用`AuthorityInitialize(Spec,Motion,Direction,StyleId)`。方向为非零有限向量；来源/目标化身资格仍检查。停止使用`AuthorityCancel()`。读取`Get Motion Snapshot`可取得总速度、推进加速度、归一化速度、单位方向、阶段和飞行时间。客户端不调用权威位移。

蓝图事件：OnFlightLaunched、OnAccelerationStarted、OnMaxSpeedReached、OnDecelerationStarted、OnFlightStopped、OnProjectileImpact；终态先锁住再广播，重入停止不复活。原生CPP仍可订阅既有OnFlightFactsChanged。没有Actor对象池；没有新建池或第二个运动Tick。

## 标准火球和Niagara

唯一生成合同为Tools/Unreal/Combat/ProjectileFlightProfiles.json；运行模板为DA_DBA_Fireball_Flight。新配置1200cm/s、4000cm/s²、0.15s达到1800cm/s，重力0，追踪开启。未选择目标直飞，后续切换选择不修改在途目标。

Niagara只消费声明且类型一致的User.ProjectileSpeed(Float)、SpeedNormalized(Float)、AccelerationNormalized(Float)、FlightDirection(Vec3)、FlightPhase(Int，0停/1加速/2巡航/3减速)、TrailLengthScale(Float)。VFX World服务复用唯一实例账本上传状态，Stopping不再更新；查询重入后重新校验完整句柄、组件反向Owner、附着来源及世界代次，新Owner继续跟踪。没有逐帧运动RPC。复制仅在一次发射时写入冻结配置、方向、服务器起始时间，位置/重力/螺旋总速度继续使用已有ReplicatedMovement。

当前火球核心保持Local Space；主尾/金丝保留World Space，按实际路径距离衰减。尾迹最长约700cm，速度联动比例0.55..1，0.5秒年龄上限保证停止后有界收尾；年龄透明度在最后35%寿命渐隐，空间透明度仍覆盖整段。原始根宽137.14cm补偿透明留白，前15%保持饱满；球内末端5%淡入，尾端14%渐隐。不要用某一个网络更新帧的瞬时速度决定整条尾迹生命周期。

## 入口和验收边界

原生创建脚本AuthorProjectileFlightProfilesUE58.py、AuthorProjectileFlightDemoUE58.py只通过Monolith在UE内运行。演示地图L_DBA_ProjectileMotion_Demo包含七个配置发射夹具，Standalone/PIE每3秒使用同一投射物Actor发射；空ProfileActor仅为追踪目标。调试球体是演示代理，正式火球沿既有Niagara链。演示地图不加入服务器地图闭包。

原生自动化前缀GamePlatform.Combat.Projectile、GamePlatform.VFX.ProjectileDrain、GamePlatform.VFX.ProjectileMotion。不同帧率以注入DeltaTime验证，不锁客户端帧率。Editor构建/原生测试不等于Cook、真实Village/MainArena准入、多人同步、GPU测量或人工视听签收；各项实际状态以Saved证据为准。

