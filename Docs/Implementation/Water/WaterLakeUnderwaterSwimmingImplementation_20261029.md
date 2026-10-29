# SSLM UE5.8 原生湖泊、水下视觉、光散射、焦散与游泳潜水实施记录（2026-10-29）

## 一、执行状态与边界

本轮已更新通用角色游泳参数、垂直输入、编辑器水体审查，并新增只影响审核地图的 UE Python 配置脚本。真实 .umap/.uasset 的写入和图形验证需要有效的 Unreal Editor + Monolith 连接。本轮 Monolith 报告编辑器不可用，不能把新增 Python 脚本视作已经在编辑器执行。

实际主工程：Game/DivineBeastsArena.uproject。现有 GamePlatformWater 插件分 Runtime、Client、Editor 三模块；权威水域 Actor GamePlatformWaterRegion 与 WaterInteractor 已存在。本轮不创建第二套水体运行模块、网络复制或 Niagara 播放系统。

正式新手村 /DBAWorldPack_Village/Maps/L_Village_Start 不进行任何自动修改。独立审核地图采用 /Game/Development/Water/L_GP_WaterReview，磁盘上同时发现旧的 /Game/Development/Water/Maps/L_GP_WaterReview（更小的同名副本），必须由 UE 编辑器确认后才允许人工清理或迁移，不自动覆盖两个资产。未提交的 Landscape Grass/Gaea/地形材质工作树更改保持原样。

## 二、审核地图执行流程（必须在真实 UE5.8 编辑器完成）

1. 打开 Game/DivineBeastsArena.uproject，确认 Water 和 Editor Scripting Utilities/Python 插件可用；先保存正在编辑的地图，再手动打开 /Game/Development/Water/L_GP_WaterReview。
2. 在 UE 内部 Python 控制台运行 Tools/Water/ConfigureNativeLakeReviewUE58.py。默认 APPLY=False：只读显示当前湖泊、水体 Zone、PhysicsVolume 和材质。检查输出无误、没有正在修改中的地图后，再把脚本顶部 APPLY 改为 True 手动运行一次。
3. 脚本只在独立审核地图且无脏地图的情况下运行；如果没有 Lake，创建一个官方 WaterBodyLake；有一个则复用；超过一个立即拒绝。成功时关闭 AffectsLandscape、补齐缺失的现有湖面材质与 Water 插件水下后处理材质，开启 Underwater Post Process 与 Generate Overlap Events，并通过引擎保存关卡。
4. 只在审核地图编辑湖岸 Spline：湖面保持合理高度；Spline 围合全部湖盆且避开三岛地表。检查 WaterZone 覆盖、水面网格是否渲染、有无水面闪烁及与地形相交。岛屿部分按实际需求使用 Water Body Island/Exclusion；不能直接以三岛中心一个巨大矩形水面替代真实湖岸。
5. 通过 Place Actors → Volumes → Physics Volume 添加原生 PhysicsVolume。使用 Brush/Edit Geometry 调整实体形状，让其水面与湖面高度一致，向下覆盖湖底；设置 Water Volume=true、Fluid Friction=0.35、Priority=30。不能仅给普通 BoxComponent 打标签，也不能放一个没有有效 Brush 的空 PhysicsVolume。
6. 使用 GamePlatformWaterRegion 维护服务器和客户端一致的权威区域：RegionId=Review.Lake；Kind=Lake；bSwimmable=true；ShallowDepthCm=90cm。WaterBounds 的 XY 范围与湖岸内侧一致，底面至少覆盖潜水深度。注意本平台 WaterBounds 是无碰撞的查询体，不负责引擎 PhysSwimming。
7. 通过编辑器控制台执行 gp.Water.Audit。检查：有效的原生 Lake 数量、水下材质与 Overlap 条件、可游泳权威区与 PhysicsVolume 是否同时存在、重复 RegionId 是否为0。
8. PIE 开始后执行 r.Water.VisualizeActiveUnderwaterPostProcess 2 并将自由视角降到水面以下 100–200cm；确认选中的 Underwater Post Process WaterBody 不为空、材质有效。测试后设置 0 关闭诊断。
9. 保存并关闭地图后重新打开，核验 Spline、材质实例、Underwater Settings、水区与 PhysicsVolume 均能回读；此步骤不能以文件存在代替。

脚本只处理审核地图 WaterBodyLake 的安全预置配置，不自动创建潜水 PhysicsVolume 的几何，也不擅自重塑正式 Gaea Landscape。

## 三、光散射、吸收、水下雾、焦散实施规格

水面采用现有 GamePlatformSurface 水材质 /GamePlatformSurface/Materials/Water/MI_GP_Lake_Native。要求材质主图采用 Single Layer Water 模型，通过 Absorption Coefficients 和 Scattering Coefficients 实现水体浸透感，结合 Normal、Roughness 和适度透明度展示近岸可见湖底、深处蓝绿。它不等于摄像机进入水下后的视线衰减，水下仍需额外后处理。

水下后处理先接入官方 /Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume 验证完整检测链，别在此阶段同时修改多个材质。基础效果正常之后再制作项目材质实例，复用 MF_GP_WaterUnderwater；水下雾的混合系数推荐 F=1-exp(-k*d)，其中 d=SceneDepth(cm)/100 换算为米、k 初始 0.07 至 0.14 每米，需随场景实际曝光、能见度调试。岸边水草清晰可见、远湖底逐步青蓝化；仅在检测摄像机在水下时启用，禁止全世界 Unbound PostProcess 蓝绿着色。

水下焦散可复用平台 MF_GP_WaterCaustics 资产，但是必须在 UE 材质编辑器检查其真实节点、纹理采样、UV流动和是否连接到湖底材质。建议：
- 使用两层不同滚动速度的法线/焦散纹理，交错形成动态闪烁光斑；采样通过岩石和浅水地面材料的 WorldPosition UV。
- 焦散乘以直射太阳方向权重、深度渐隐（浅水最强，超过约8米明显衰减）及水体清澈度；夜间按场景主光照淡出。
- 小规模湖底可选局部 Light Function 或材质投射；先在 10m×10m 浅滩调试，再扩展。此项还没有由 UE 引擎构建或画面验收，不能标记焦散已经生效。
- 粒子漂浮、水底气泡、游泳水花应走既有 GamePlatformVFX 和 Presentation 语义链；专服完全不依赖 Niagara。不得复制一套独立粒子对象池。

质量初值仅为待测候选：高档焦散与漂浮粒子限近景 18m；中档 10m；低档关闭动态焦散及高成本漂浮粒子，保留稳定基本水色。通过 stat gpu / stat unit / stat water 对比水面-only、水下、焦散、粒子各阶段，不能当作已测的性能承诺。

## 四、游泳和潜水系统（已落地代码）

角色 ADivineBeastsGameplayCharacter 构造中设置 MaxSwimSpeed=280cm/s、Buoyancy=1.05、BrakingDecelerationSwimming=260cm/s²。原生 CharacterMovement 根据 PhysicsVolume.bWaterVolume=true 切换 MOVE_Swimming，由 PhysSwimming 负责浮力、流体阻力、网络移动预测和校正。

玩家输入沿用已有 DBA.MoveForward、DBA.MoveRight。游泳时 W/S 按视角 Pitch/Yaw 前进，走路时仍只按 Yaw 水平前进；DBA.SwimVertical 的 Space=+1 上浮、LeftControl=-1 下潜，只有 Movement.IsSwimming 为真才向 AddMovementInput(FVector::UpVector) 送入控制；原有 Space 跳跃输入保留。所有移动仍经过原生 CharacterMovement，不新增Z轴位置客户端RPC。

平台 WaterInteractor 仍以服务器权威水域查询、离散状态复制为准，客户端水下视觉使用本地摄像机与 WaterBody 后处理；两个状态来源不可混淆。关卡美术、水体碰撞与权限一致性必须逐区验证：禁游泳危险浅水区不能被一个覆盖整湖的 bWaterVolume 误判为可游泳。游泳动画 SwimIdle/Surface/Forward/Dive/Ascend 与氧气/溺水GAS规则、本地水下音频切换属于后续原生资源和动画接线工作，本轮尚未声明完成。

## 五、验收

- [ ] Monolith真正连接 UE Editor，审核地图正确加载，安全脚本 APPLY=True 保存，关卡冷重载回读通过
- [ ] 湖面 WaterMesh、Spline 和岛屿岸线正确，Gaea高度不被无意雕刻
- [ ] 摄像机进出水面时 Underwater Post Process 启用与退出，水下渲染没有只剩一张平面的问题
- [ ] 水面吸收散射和水下深度雾在早晨/夜间有美术截图证据
- [ ] 湖底动态焦散实际投射且可降级，未在夜晚产生错误高亮
- [ ] 原生 PhysicsVolume 覆盖湖盆，水边/深水的 MOVE_Walking/MOVE_Swimming/MOVE_Falling 切换正常
- [ ] 水中 W/S、Space、LeftCtrl 和摄像机俯仰操作正确；陆地行走、跳跃、技能未退化
- [ ] 两客户端 + DedicatedServer 运动校正、入水/出水、重连和跨关卡行为一致
- [ ] Editor/Client/Server 三目标增量编译、最小 Cook 通过，服务器无纯水体渲染资产依赖
- [ ] GPU/CPU 实测及四档画质记录，无伪造帧率或成功状态

本次代码、脚本、文档已纳入本地工作树，未自动提交，不代表已完成以上未勾选的运行验收。
