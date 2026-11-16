# 通用飞行法术运动实施计划

**目标：** 在既有GamePlatformCombatProjectile唯一权威运动链上增加可配置速度策略、重力和表现状态，以标准火球完成首例。

**依据：** 用户附件《通用飞行法术加速度系统实施提示词V1.0》；2026-11-01补充明确使用本地/Monolith，新火球1200→1800cm/s、0.15秒，继续追踪原目标，小于30°仅取消额外弧线。

**架构裁定：** 通用机制沿用GamePlatformCombat，不新建Moba运动组件、ProjectileMovement或第二个位移执行者。GamePlatformCore仅提供无战斗含义的只读运动状态接口，VFX经既有账本消费；项目层拥有配置资产。旧SpeedCmPerSecond序列化字段保留，未启用新策略时行为兼容。配置在发射时冻结，发射后修改被拒绝；不引入预测框架。

**单位及边界：** cm、s、cm/s、cm/s²；推进曲线与恒加速互斥。抛物线保留重力分量，MaxSpeed约束总速度。追踪与重力模式分开配置，非法组合拒绝。终止事件一次，来源/目标换代和世界退出沿现有清理。验证客户端不限帧；30/60/120测试通过注入DeltaTime完成，不锁游戏帧率。

## 文件与交付步骤

- [x] 1. 在现有Projectile原生测试中添加缺失能力RED：加速距离、速度上限、减速、曲线、非法配置、30/60/120步长、追踪与碰撞。
- [x] 2. 新增Types/GamePlatformCombatProjectileFlight.h与对应Private/Types实现：五种速度模式、配置、Profile DataAsset、纯求值；既有Motion增加可选配置/Profile。曲线在发射时冻结。
- [x] 3. 扩展既有Actor：统一子步积分、总速上限、重力、距离/时间终止、Blueprint配置/发射/停止及生命周期事件，保留原30度追踪和Sweep裁决。
- [x] 4. GamePlatformCore增加只读运动接口；初始配置/发射时间随现有Actor复制，客户端计算表现速度，真实位置仍由原运动复制修正，不逐帧RPC。
- [x] 5. 在VFX既有实例账本中为Projectile绑定声明过的六个User参数；World作用域更新并清理，不引入项目播放器/池或技能Tick。
- [x] 6. Monolith原生创建DA_DBA_Fireball_Flight与演示配置/现有测试关卡，绑定当前火球Definition；更新项目校验使新Profile合法，其他技能保持原默认。
- [x] 7. 完整Editor增量构建与原生测试；保存/冷重载原生资产并验证Niagara速度参数、世界尾迹与命中收尾。
- [ ] 8. 同步Client/Server构建、Cook及真实运行版本门禁；两个客户端验证Village/MainArena、按1施法，记录视听/网络/GPU/人工验收的真实边界。

## 审查重点

- 旧常速/螺旋/30度边界兼容；不因新Profile解除已选目标追踪。
- 零速减速有最大寿命；曲线空值、非有限值、重复发射/停止必须拒绝或幂等。
- 曲线积分按时间而非帧数；高速Sweep、近目标缩步与帧率无关。
- 复制初始参数就绪前不虚构表现状态；无Niagara仍可完整运动和结算。
- 新增资产由引擎创建，演示资源不进入专服运行闭包；不覆盖现有未提交工作。

## 当前状态

main/d8e0eacd；原位保留既有未提交修改，没有提交或重置。唯一权威Actor已增量扩展，未添加UProjectileMovementComponent。

2026-11-01：完整Editor、Client/Server与双Cook/Stage均通过；52项原生回归全部通过。冷重载三阶段Niagara与24项材质资源通过，10组RHI/SimCache通过。球尾结合部前15%保持饱满、根部在球内淡入，末端及年龄Alpha逐渐衰减。演示地图真实PIE运行8秒，记录482个运动快照，有限数值与总速上限检查通过；该采样不代替每个演示配置的人工签收。

实际Village与MainArena均已换用本次完整服务器包，启动客户端前的真实PID/二进制/战斗合同门禁通过。两客户端经测试账号按钮进入真实Village，首客户端按1施放火球，双端收到相同发生身份的Cast/End通知；日志不代替听感签收。FPS实际显示超过60，t.MaxFPS=0、r.VSync=0。

竞技场点击仅选择目标的既有问题仍未修复，真实MainArena准入未通过。该问题由此前暂停的相关任务继续处理，已经按用户授权发送解除冻结通知。共享柳树编辑器已恢复PID62308、9317、原地图和镜头。音乐及施法声时机已由用户确认正常；最终人工视觉、多人GPU预算及完整竞技场玩法验收仍待完成；不得标记整体已验收或Production Ready。

完整运行ID和分阶段证据见Saved/Validation/ProjectileMotion20261101/Delivery.json及Progress.md；使用入口与参数见ProjectileFlight_Usage.md。

