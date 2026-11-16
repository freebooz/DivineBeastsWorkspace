# 《神兽联盟》火球/冰块成形、飞行与拖尾 UE5.8 实施清单 V1.0

工作空间：DivineBeastsWorkspace（神兽联盟工作空间）。引擎：UE5.8（虚幻引擎5.8）。本文件是独立审核资产计划，尚未实际生成的新资源不得当成已存在。坚持 GamePlatformVFX（平台特效）/ DivineBeasts（项目表现）分层，保留主分支脏工作树，禁止覆盖正式VFX、GAS、Dedicated Server（专用服务器）时序、命中和伤害。

## 1. 四套原生 Niagara 候选

统一审核路径：/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1/

| 阶段 | Niagara资源 | 中文职责 |
|---|---|---|
| 火球成形 F1 | NS_DBA_FireOrb_Form_Review | 白金核心、三维橙金球壳及外卷火舌 |
| 冰块成形 I1 | NS_DBA_IceBlock_Form_Review | 块状多面实体冰块、凝霜和晶体生长 |
| 火球飞行 F2 | NS_DBA_FireOrb_FlightTrail_Review | 伴随权威弹体运动的燃烧核心及世界空间Ribbon（带状拖尾） |
| 冰块飞行 I2 | NS_DBA_IceBlock_FlightTrail_Review | 伴随权威弹体运动的实体冰块及寒雾Ribbon（带状拖尾） |

子目录：Textures（纹理）、Materials（材质）、Meshes（网格）、Niagara/Modules（模块）、Niagara/Systems（系统）、Definitions（特效定义）。截图视频放工作空间 Saved/Validation/OrbProjectileV1（审核证据），不加入专服Cook（资源打包）。

## 2. 工程已确认的复用资源

- 火球蓄力：/DBAPresentationPack_Core/VFX/Library/01_Cast/NS_Fire_Fireball_Charge（现有火球蓄力系统，含火焰和火星精灵）。
- FireMage（火焰法师）：NS_PY_Charge、T_PY_FlameFlipbook（火焰序列帧）、MI_PY_FlipbookCore（核心材质）、MI_PY_Ember（余烬材质）。
- 火球飞行：/DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NS_DBA_DevFireballFlight（开发版火球飞行系统，含Sprite而非经本轮确认的Ribbon）。
- 冰箭飞行：/DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_Flight（现有冰箭飞行）、SM_DBA_IceArrow_CrystalShard（冰晶碎片网格）、SM_DBA_IceArrow_CrystalLance（细长晶体长矛），后者不可充当厚冰块核心。
- 冰霜材料：M_DBA_IceArrow_Crystal（PBR冰晶母材质）、MI_DBA_IceArrow_Mist（寒雾实例）、MI_DBA_IceArrow_Trail（拖尾材质）、T_DBA_IceArrow_CrackMask（裂纹）、T_DBA_IceArrow_IceNormal（法线）、T_DBA_IceArrow_FrostMist（寒雾）、T_DBA_IceArrow_TrailStrip（流带）。
- 通用Ribbon：M_VFX_Ribbon_Translucent_Master（半透明Ribbon母材质）、NM_VFX_RibbonSpawn / NM_VFX_RibbonUpdate（带状粒子运动模块）、ET_VFX_ProjectileTrail（弹体拖尾效果类型）。
- 已有平台 UGamePlatformVFXTrailDefinition（拖尾定义）和 AGamePlatformCombatProjectile（权威弹体）。客户端处理复制飞行事实并绑定弹体根组件；不得再建第二条视觉弹道。

## 3. 各阶段纹理与材质资源命名

### F1 大火球成形

| 资源名 | 中文说明 | 规格/建议 |
|---|---|---|
| T_DBA_FireOrb_FlameFlip_Review | 外层火舌透明动画纹理 | 1024x1024 RGBA，4x4帧连续运动，优先复用现有FireMage序列帧 |
| T_DBA_FireOrb_CoreFalloff_Review | 白金色高温核心柔边 | 512x512 RGBA |
| T_DBA_FireOrb_SurfaceFlow_Review | 橙金球面低频热流噪声 | 512x512可平铺 |
| MI_DBA_FireOrb_Form_Core_Review | 核心发光材质实例 | 暖白局部高温，保留橙金细节 |
| MI_DBA_FireOrb_Form_Shell_Review | 三维火焰球壳实例 | Sprite序列帧，层次清晰 |
| MI_DBA_FireOrb_Form_Tongues_Review | 外翻火舌实例 | 不同动画相位和局部方向 |

### I1 冰块成形

| 资源名 | 中文说明 | 规格/建议 |
|---|---|---|
| SM_DBA_IceBlock_FacetedCore_Review | 厚重多面体冰块主网格 | 新增低面晶体块，三视角审核，非冰箭长矛 |
| T_DBA_IceBlock_CrackMask_Review | 冻结裂纹灰度 | 1024x1024，先复用冰箭裂纹 |
| T_DBA_IceBlock_Normal_Review | 冰面微法线 | 1024x1024，复用冰箭法线 |
| T_DBA_IceBlock_FrostMist_Review | 外部收缩冷雾 | 512x512透明纹理 |
| MI_DBA_IceBlock_Form_Crystal_Review | PBR实体冰晶材质 | 候选粗糙度0.10–0.24，受控折射 |
| MI_DBA_IceBlock_Form_Frost_Review | 晶面结霜材质 | 浅蓝霜纹、局部高光 |
| MI_DBA_IceBlock_Form_Mist_Review | 冷气凝聚材质 | 蓝白半透明、低噪声 |

### F2 火球飞行与拖尾

| 资源名 | 中文说明 | 规格/建议 |
|---|---|---|
| T_DBA_FireOrb_TrailGradient_Review | Ribbon宽度和Alpha渐隐 | 512x128 RGBA |
| T_DBA_FireOrb_RibbonFlame_Review | 沿带状粒子流动的火纹 | 1024x256，复用火焰Ribbon |
| T_DBA_FireOrb_Ember_Review | 尾部余烬纹理 | 256x256，复用现有余烬 |
| MI_DBA_FireOrb_Flight_Core_Review | 飞行内核材质 | 暖白+金橙层 |
| MI_DBA_FireOrb_Flight_Flame_Review | 外围翻卷火舌材质 | 多层序列帧 |
| MI_DBA_FireOrb_Flight_Ribbon_Review | 主火焰拖尾材质 | 基于已有M_Fire_Ribbon |
| MI_DBA_FireOrb_Flight_Ember_Review | 火星材质 | 柔和、无锐利闪光 |

### I2 冰块飞行与拖尾

| 资源名 | 中文说明 | 规格/建议 |
|---|---|---|
| SM_DBA_IceBlock_FacetedCore_Review | 飞行实体厚冰块网格 | 复用I1 |
| T_DBA_IceBlock_TrailGradient_Review | 冷白Ribbon渐隐 | 512x128 RGBA |
| T_DBA_IceBlock_MistNoise_Review | 低频冷雾扰动 | 512x512 RGBA |
| T_DBA_IceBlock_CrystalFlake_Review | 冰屑纹理 | 256–512正方形，复用冰箭素材 |
| MI_DBA_IceBlock_Flight_Crystal_Review | 实体冰块PBR材质 | 多面体反射/微裂纹 |
| MI_DBA_IceBlock_Flight_Ribbon_Review | 寒雾Ribbon材质 | 派生MI_DBA_IceArrow_Trail |
| MI_DBA_IceBlock_Flight_Mist_Review | 冷雾尾流材质 | 派生冰箭雾气 |

## 4. 发射器结构与数量

| 系统 | Emitter发射器名（中文） | 类型 | 审核初值 |
|---|---|---|---|
| F1 | OrbCore（高温球心） | Mesh/稳态多层核心 | 1 |
| F1 | OrbShellFlame（球壳火焰） | 3D球面分布的Flipbook Sprite | 8–12 |
| F1 | OuterFlameTongues（外卷火舌） | Flipbook Sprite | 8–16 |
| F1 | HeatFlow（近球面热流） | 轻量Sprite | 5–10 |
| F1 | EmberSparks（余烬） | 小精灵 | 4–8 |
| I1 | IceCoreMesh（厚冰块） | Mesh Renderer，真实块状网格 | 1 |
| I1 | FacetGrowth（晶面生长） | 小Mesh/冰晶 | 5–9 |
| I1 | FrostWrap（表面霜层） | 透明Sprite | 6–10 |
| I1 | ColdCondensation（寒雾凝聚） | 透明Sprite | 8–14 |
| I1 | CrystalGlint（棱面高光） | 极少量Sprite | 3–6 |
| F2 | TravelCore（飞行热核） | 贴附真实弹体的核心 | 1 |
| F2 | FlightFlames（飞行表面火焰） | 局部空间Flipbook | 8–16 |
| F2 | PrimaryFireRibbon（主火焰拖尾） | 世界空间Ribbon | 1条 |
| F2 | SecondaryWisps（次流焰） | 低对比流线 | 4–8 |
| F2 | TrailingEmbers（余烬） | 世界空间Sprite | 4–10 |
| I2 | FlyingIceCore（飞行实体冰块） | Mesh | 1 |
| I2 | FacetShimmer（晶面光泽） | 局部高光Sprite | 3–6 |
| I2 | PrimaryFrostRibbon（寒气主拖尾） | 世界空间Ribbon | 1条 |
| I2 | ColdMistWake（冷雾尾流） | 世界空间Sprite | 6–12 |
| I2 | CrystalShards（碎晶） | 小Mesh或Sprite | 4–8 |

只有Niagara Ribbon Renderer才算主拖尾，不允许以一个拉伸的大Sprite冒充。成形核心和外壳使用局部空间；拖尾必须留在世界空间记录真实历史路径。急转、位置纠正、瞬移后须截断旧轨迹，不允许长条穿越地图。

## 5. 统一审核参数初值表

参数均为效果初值而非实测游戏平衡，单位cm、s、度/秒。系统的User参数应通过FGamePlatformVFXParameterSchema（平台参数白名单）校验，不得超过单请求已有MaxOverrideCount默认16。

| 参数（中文说明） | F1火球成形 | I1冰块成形 | F2火球飞行 | I2冰块飞行 |
|---|---|---|---|---|
| User.Phase（表现进度0–1） | 0→1 | 0→1 | 由飞行阶段驱动 | 由飞行阶段驱动 |
| User.FormDurationSec（成形时长） | 0.20–0.45 | 0.25–0.55 | 无 | 无 |
| User.CoreDiameterCm（核心直径） | 14–22 | 45–70 | 20–30 | 40–60 |
| User.VisualDiameterCm（总视觉直径） | 70–90 | 45–70 | 50–75 | 40–60 |
| User.SurfaceSpinDegSec（自旋角速度） | 18–45 | 6–18 | 30–65 | 15–35 |
| User.EmissiveScale（发光强度倍率） | 2.0–3.4 | 0.45–1.2 | 2.0–3.2 | 0.5–1.2 |
| User.DecorRate（辅助粒子倍率） | 0.3–0.7 | 0.2–0.5 | 0.3–0.8 | 0.2–0.6 |
| User.TrailWidthCm（主拖尾宽度） | - | - | 14–28 | 15–30 |
| User.TrailLifeSec（主拖尾寿命） | - | - | 0.15–0.30 | 0.18–0.36 |
| User.TrailGapCm（按距离采样间隔） | - | - | 12–24 | 15–28 |
| User.TrailBreakGapCm（瞬移断尾阈值） | - | - | 150–240 | 150–240 |
| User.OpacityScale（透明度） | 0.65–0.9 | 0.35–0.75 | 0.55–0.85 | 0.30–0.70 |

补充：User.CoreColor（核心颜色）、User.OuterColor（外层颜色）采用项目元素预设。弹体位置和方向来源于权威/复制轨迹，不允许依据Niagara参数另算一条独立飞行路线。

## 6. 成形与飞行的实际挂点

1. 成形：以角色左右手hand_l（左手骨骼）、hand_r（右手骨骼）中点为中心，在客户端视觉锚点朝前偏移10–18cm、朝上偏移6–12cm。当前平台请求只支持一个标准AttachComponent（附着组件），项目层可创建一个中心视觉Anchor随施法动作更新；不得改变冰箭现有地面法阵。
2. 飞行：核心/冰块/外层火舌跟随AGamePlatformCombatProjectile（平台战斗弹体）根组件。GamePlatformPresentation请求生命周期与现有复制飞行事实同步，不更改服务器速度、轨迹、碰撞与命中。
3. 拖尾：同一Niagara的Ribbon发射器或已有UGamePlatformVFXTrailDefinition（拖尾定义）附着到同一根组件；生成的历史点留在世界空间。弹体结束或被销毁时按RequestId取消并清除轨迹缓存。
4. 大火球与冰块不能同时被Niagara与Projectile组件双重施加世界运动；成形转飞行仅是视觉从掌心Anchor切换到权威弹体的确认时间点。

## 7. 四档画质和预算

- Epic（极致）：球体主核+完整表层+1条主Ribbon+少量副流焰/冰屑。
- High（高）：减弱外层火舌、减少冰屑与次流焰，保留主Ribbon与主体。
- Medium（中）：简化球壳与雾气，降低Ribbon节点数和透明Overdraw（过度绘制），仍保持清晰球体/实体冰块。
- Low（低）：禁用热扰动与昂贵冰面折射；保留主体和单条短淡尾，禁止将火球/冰块裁剪成不可辨识点光。

复用既有ET_VFX_ProjectileTrail（拖尾效果类型）、现行Effect Type实例预算、距离裁剪及质量分级；现有文档的120/80/58/40个World总候选只是待测目标，不代表引擎默认硬限。要在1v1至5v5、最多十个角色同场时测量真实System实例数、Niagara CPU/GPU毫秒和透明过绘。

## 8. 分阶段人工审核

- M0：资产盘点、纹理草稿及冰块实体网格三视图，人工优选；不把概念图当成真实引擎截图。
- M1：导入原生纹理、网格及材质实例，固定曝光、三角度材质球/实体审核，接受后进Niagara。
- M2：构建四套新Niagara审核系统、编译、保存、冷重载及参数绑定回读；任意一套渲染不见主体必须记录失败，不得宣称通过。
- M3：真实UE5.8 PIE（编辑器内运行）和打包客户端截帧录像。成形各至少3个过程节点与3个角度；飞行各至少4个时间节点，附加急转、网络位置校正、取消/命中时的拖尾清理。
- M4：人工确认，再接入正式Gameplay/Presentation映射和多人性能验收。禁止修改正式GAS/权威时序/伤害，不自动提交主分支。

## 9. 本轮真实工程执行记录（与目标设计严格区分）

- 已在当前UE5.8工作空间内单独创建审核源网格：SM_DBA_IceBlock_FacetedCore_Review（块状多面体、24个面三角形），其OBJ源与校验清单位于项目SourceArt/VFX/OrbProjectile/Review/V1，不是现有细长冰箭。
- 已将7张现有纹理复制进本审核路径（源火焰序列帧、火Ribbon纹理、火拖尾渐变、冰裂纹、冰法线、寒雾、冰尾迹）。这些是**实际纹理审核副本，不是新绘制或重新烘焙的高清终版纹理**。
- 已派生12个实际可冷加载的独立审核材质实例；参数初值仅在已存在的材质参数接口上进行小范围设置，不改公共母材质。
- 已保存四套Niagara原生**复用基线系统**：NS_DBA_FireOrb_Form_BaselineReview、NS_DBA_IceBlock_Form_BaselineReview、NS_DBA_FireOrb_Flight_BaselineReview、NS_DBA_IceBlock_Flight_BaselineReview。四套从源资产克隆，用于后续发射器改造，**不等于目标的20个发射器已经完成，也不等于真正Ribbon Renderer已经接入**。
- 独立UE5.8进程冷重载编译：上述四套基线系统均0错误、0警告；基线发射器分别为HandFlame/GatheringSparks，CrystalArrow/FrostRibbons/ColdMist/OrbitShards/LaunchFlash，Core/Aura/Trail，CrystalArrow/FrostRibbons/ColdMist/OrbitShards/LaunchFlash。
- **未完成**：厚冰块Mesh替换原冰箭长矛、四套目标系统的全量专用发射器、真实世界空间Ribbon渲染器、实际弹体附着、连续UE5.8截图视频、性能与人工签收。它们必须保持未通过状态，不得切换正式技能。
- 真实证据：Saved/Validation/OrbProjectileV1/SourceInventory.json、ReviewAssetFoundation.json、FoundationColdLoad.json；git不执行提交或清理。

## 10. 下一次真实制作的验收顺序

1. M1：人工审查新厚块OBJ三视图和火焰/冰晶材质质量，确定修改意见；缺图不自动通过。
2. M2：从四套BaselineReview复用起点创建四套独立目标Review系统，在单独自有编辑器中逐层建立本文定义的20个发射器并验证Ribbon真正使用Niagara Ribbon Renderer。每次新增模块后重新原生编译、保存、冷加载。
3. M3：分别出火球成形、冰块成形、两种飞行/拖尾真实UE5.8截图视频，核对空间轨迹、世界空间历史粒子、网络纠正断尾、弹体停止时自动清理。
4. M4：人工接受后才讨论正式绑定及多人打包性能验收；永不擅自改GAS、服务器时序和主分支。

## 11. 一次性执行任务提示词

请在DivineBeastsWorkspace（神兽联盟工作空间）中制作四套独立审核特效，复用已存在火焰、冰箭、Ribbon模块及GamePlatformVFX（游戏平台特效插件）。先审核新火球球壳动画、厚块多面体冰块以及透明拖尾纹理，再制作材质，最后分系统创建Niagara并检验发射器。火球成形必须白金内核+橙金实体球壳+不均匀火舌；冰块成形必须厚多面PBR冰核+凝霜+冷雾；火球飞行必须围绕权威弹体保留燃烧核心和历史Ribbon火尾；冰块飞行必须保留实体块状冰晶和逐渐消散的冷雾冰屑Ribbon。所有资产只放Review目录，不覆盖或重新导入任何正式资产，不改变服务器发射时刻、速度、碰撞、命中、GAS技能判定。分别交付真实UE5.8编译、保存、冷重载、截图、视频、性能、风险与人工审核结论，待人工确认后再绑定正式内容包。
