# SSLM 火球击中与冰晶击中 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. 原生资产通过本地Monolith串行创建；勾选项必须有对应证据，不自动提交主分支。
>
> **Goal:** 在独立审核地图实现“赤金迸焰”和“霜晶碎爆”两套命中表现，对照概念图验证接触位置、立体层次、消散节奏及性能。
>
> **Architecture:** 复用GamePlatformPresentation→GamePlatformVFX唯一执行链、GamePlatformData及Niagara原生伸缩/池化。项目资产归第三层DBAPresentationPack_Core，MOBA层不承担播放；不新建播放器、预算服务或网络命中判定。
>
> **Tech Stack:** UE5.8、Monolith MCP、Niagara Mesh/Sprite、离线连续序列烘焙、材质实例、原生截图/视频/SimCache和性能采集。
>
> **Spec:** [火球命中概念图](../../Art/VFXConcepts/20261027_ImpactRedesign/SSLM_FireballImpact_Redesign_Concept.png)、[冰晶命中概念图](../../Art/VFXConcepts/20261027_ImpactRedesign/SSLM_IceCrystalImpact_Redesign_Concept.png)、[原始生成提示词](../../Art/VFXConcepts/20261027_ImpactRedesign/GenerationPrompts.json)。
>
> **状态／日期：** 2026-10-27完成P0只读核对和执行计划。以下资源名和制作数值除明确列为“现有”者外均为拟新增／初值，不是原生实现、编译、视觉或性能通过证明。

## Global Constraints

- 仅制作两种命中审核效果。保留原V3、掌心、飞行及正式技能引用；人工视觉通过前不替换正式Definition/Catalog，不提交主分支。
- Codex内禁止HWebCodex；真实uasset/umap只由UE5.8及Monolith创建、编译、保存、重载。
- 不修改服务器技能时间、冷却、伤害、命中、追踪或复制合同。碰撞命中不等同成功造成伤害；表现不能新增冻结、燃烧、击退或AOE语义。
- 命中锚点使用服务器确认的ImpactLocation；人物目前采用碰撞中心，不能硬编码某英雄胸骨偏移、重新瞄准当前选择或把客户端表面查询当作权威命中。
- 所有魔法有局部空气波动；可降级，关闭后保留命中位置及主轮廓。无全屏SceneColor、巨大光环、强镜头闪白或默认运行时Fluids。
- 共用公共母材质保持不变。仅在独立审核目录复制并扩展确有缺失的材质接口。
- 所有离线Bake、缓存、原生证据和后续Cook/Stage均在当前工程目录内；本轮计划及纯内容制作不触发C++全量构建/清缓存。
- 本次不清理、保存或覆盖其它任务的脏包和手工地图。开始原生制作前再次核对工作分支、输入指纹、编辑器身份及其它任务。
- 颜色遵循[全局配色V3.0](../../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)，性能遵循[项目Niagara规范](../../Architecture/SSLM十二生肖技能VFX性能与视觉质量规范.md)。

## Review Focus

1. 斜撞墙面、地面和不同体型目标：锚点正确、法线朝向稳定，不固定朝世界X轴；任务1、4验证。
2. 30fps与亮背景：短闪光可读、没有整块白曝和细碎闪烁，厚火焰与透明碎晶仍可识别；任务2、3、4验证。
3. 多次命中、取消、池化及World退出：不双播、不残留旧位置/随机种子，不让VFX失败吞掉音效；任务4、6验证。
4. 飞行结束与命中过渡：不把前段长拖尾再生成一份，不留下完整弹头/冰冠；任务4、6验证。
5. 近景重叠和十人并发：目标轮廓、关键反馈与四档降级均清晰；真实CPU/GPU和透明过绘不得用粒子数量代替；任务5验证。

## 1. P0：已核对的工程事实

- 当前主工程为Game/DivineBeastsArena.uproject；Monolith连接UE5.8.0-0+UE5，PID65904，地图为PalmRedesign审核地图。检查结束时脏包数量为0。
- 分支main；AGENTS.md与总体目录说明已有未提交修改，保留。计划不改变引擎资产。
- 注册表确认内容插件物理目录为Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core，挂载点为/DBAPresentationPack_Core。
- 已读根AGENTS.md、总体规划、Game/Plugins/插件开发规范.md、整合交付说明及相关VFX/色彩规范；本任务路径未发现额外AGENTS覆盖文件。

| 现有资源 | 原生读回结果 | 复用决策 |
| --- | --- | --- |
| /DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NS_DBA_DevFireballImpact | 3个GPU局部空间发射器：Flash 1、Halo 32、Fragments 8；User参数为空；Bounds±250cm | 作为旧版对照，不把光环面片堆叠当作厚实卷焰 |
| /DBAPresentationPack_Core/VFX/IceArrow/Niagara/NS_DBA_IceArrow_Impact | 4个GPU局部空间发射器：ImpactCore 1、BurstShards 28、IceCrown 1、ImpactMist 18；User参数为空；Bounds±700cm | 审核副本取消冰冠语义，减少冷雾覆盖；保留旧资源 |
| /DBAPresentationPack_Core/VFX/Library/Game/FireMage/Niagara/NS_PY_Impact | 5个CPU发射器，包括RollingFlames、RadialEmbers、ResidualSmoke、Shockwave、IgnitionFlash | 复用可用纹理与材质思路，不整套照搬2.3秒、大光环效果 |
| 同FireMage/Materials/M_PY_Flipbook | 标量Energy、Density；没有公开Texture参数 | 实例只能调整已有接口，换序列需审核目录的母材质副本 |
| 同FireMage/Materials/M_PY_Additive | Energy、Density和Mask纹理参数 | 小闪光/火星可用独立实例复用 |
| /DBAPresentationPack_Core/VFX/IceArrow/Materials/MI_DBA_IceArrow_ImpactCrystal | EdgeIntensity、CrystalOpacity、SpecularLevel；IceTint、EdgeTint；IceNormal、IceORM、CrackMask | 先复用晶体材质，粗糙度等缺失接口只能在审核副本中增加 |
| 同目录MI_DBA_IceArrow_ImpactFlash | EmissiveStrength、OpacityScale、IceTint、MaskTexture | 独立审核实例复用 |
| /DBAPresentationPack_Core/VFX/Library/00_Core/Textures/Fire/T_VFX_Fire_Burst_Flipbook_8x8_01 | UE当前尺寸读回32×32、sRGB关闭 | 名称不证明真实64帧品质；不作为本次最终爆燃序列 |
| /DBAPresentationPack_Core/VFX/Library/Game/FireMage/Textures/T_PY_FlameFlipbook | UE当前尺寸读回2048×2048、sRGB开启 | 先作链路验证，需核对实际帧格和内容；不从名称推断新序列 |
| /DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign/Materials/M_DBA_PalmAirRipple_Review | P_Intensity、P_Opacity；作者源码使用IOR=1+强度×Alpha×局部波动 | 可复用原生扰动基础并建新实例；原母材质不改 |
| /DBAPresentationPack_Core/VFX/Library/00_Core/EffectTypes/MagicImpact/ET_VFX_MagicImpact | 原生NiagaraEffectType存在 | 首选复用；制作时读回伸缩设置并实测，存在不代表预算合格 |

现有两个InstantDefinition位于/DBAPresentationPack_Core/VFX/DevelopmentMagic/Definitions：
- DA_DBA_VFX_DevFireballImpact：引用旧火球系统，MaxLifetimeSeconds=0.8，默认参数及质量变体为空。
- DA_DBA_VFX_DevFrostboltImpact：引用旧冰箭系统，MaxLifetimeSeconds=1.1，默认参数及质量变体为空。
- 两者Definition的EffectType引用当前为空；本计划不会直接改正式定义。

实际调用合同：
- DivineBeastsDevelopmentMagicPresentation.cpp把ImpactLocation和ImpactNormal放入中立请求，命中SourceRotation当前为零。
- GamePlatformVFXNiagaraExecutor.cpp已有User.ImpactLocation(Position)与User.ImpactNormal(Vector3)写入，审核Niagara必须真正消费它们，不能靠系统旋转猜方向。
- GamePlatformCombatProjectile.cpp对有效战斗目标将表现点放在目标碰撞中心；真实表面碰撞仍留在伤害HitContext。现有复制事实没有完整IncomingVelocity，也没有可供审核特效直接使用的表面接触点/骨骼绑定。
- 正式链路先取消Flight，再提交Impact；目前不能假设原拖尾自动保留。
- DirectedMissile.Impact现共用Fireball.Impact定义；以后替换该定义会影响定向飞弹，接入时必须单独评审范围。
- Frostbolt另有独立Residue地面表现，本次不修改，也不把它算作新命中效果的一部分。

## 2. 视觉拆解与时间合同

| 层次 | 火球“赤金迸焰” | 冰晶“霜晶碎爆” |
| --- | --- | --- |
| 接触焦点 | 6—12cm暖白金高温焦点，白区约占主爆燃可见面积5—8% | 5—9cm冰白接触光，白区小于主效果约5% |
| 主体 | 峰值直径90—110cm，5个不对称厚卷焰叶瓣，前中后分层 | 峰值宽85—105cm，10片清晰大碎晶＋18片细碎冰屑，呈定向散射 |
| 材质 | 金黄/琥珀中层、#F16B46橙红卷边、少量暗橙折面 | #409CE5寒蓝、#C4DCFF浅蓝、深蓝内部、薄窄高光 |
| 装饰 | 少量短火星、极薄暖灰烟、局部热扰动 | 薄冷雾、少量细冰屑、局部压力折动 |
| 禁止项 | 大面积纯白、平面烟花星芒、空心S带、光环替代爆燃、岩浆/地裂 | 整圈冰冠、莲花晶簇、残留完整飞弹、厚白描边、冰封角色或巨大冰刺 |
| 收尾 | 主火焰0.45秒内收束，余烬/烟最迟0.70秒消失，系统0.75秒自然完成 | 碎晶0.85秒内消失，冷雾0.85秒消失，系统0.95秒自然完成 |

时间以确认命中为t=0，不延长服务器技能时间：
- 火：0—0.04秒压缩点火；0.04—0.16秒卷焰展开；0.16—0.45秒翻卷侵蚀；0.45—0.70秒稀疏余烬收尾。
- 冰：0—0.04秒晶核开裂；0.04—0.16秒定向碎裂峰值；0.16—0.55秒碎晶减速/转动；0.55—0.85秒薄雾和碎片淡出。
- 固定截图取0.04、0.10、0.16、0.35、0.65秒；视频覆盖命中前后全程。图中的长拖尾属于前一飞行阶段，不在Impact里伪造连到施法者的光束。
- 中心锚点与胸甲表面接触并不完全等价。初版围绕既定中心组织可见卷焰/碎裂体积，检查身体遮挡；不私自平移整套特效伪称精确表面命中。若精确贴甲闪光确需新增表面事实，列为独立合同工作，不能在本轮暗改服务器。

## 3. 资产归属与交付清单

以下B代表/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/ImpactRedesign，所有名称均为拟创建：

| 资源 | 职责 |
| --- | --- |
| B/Niagara/NS_DBA_FireballImpact_ConceptReview | 火球四发射器审核系统 |
| B/Niagara/NS_DBA_IceCrystalImpact_ConceptReview | 冰晶四发射器审核系统 |
| B/Niagara/NM_DBA_ImpactReviewMotion | 仅两套审核效果共用的Niagara运动模块，不是新框架 |
| B/Definitions/DA_DBA_FireballImpact_ConceptReview | InstantDefinition，MaxLifetimeSeconds=0.8；独立审核身份 |
| B/Definitions/DA_DBA_IceCrystalImpact_ConceptReview | InstantDefinition，MaxLifetimeSeconds=1.1；独立审核身份 |
| B/Maps/L_DBA_ImpactRedesign_Review | 固定相机、角色尺度与曝光的审核地图 |
| B/{Textures,Materials,Meshes}/ | 下表纹理、实例和网格；不覆盖原路径 |
| 同插件SourceArt/VFX/ImpactRedesign/ | 可编辑模型、连续动画源、纹理和导入清单 |
| Game/Saved/Validation/ImpactRedesignReview20261027/ | 原生PNG、MP4、SimCache、CSV、加载/编译证据 |

两份审核Definition使用不与正式Catalog冲突的身份。先以审核工具驱动原生组件做资产视觉检查；正式调用、网络去重及运行时World服务验收单独报告，不能用审核组件代替正式链路证明。

## 4. 纹理与网格制作

| 拟新增纹理 | 规格与通道 | 用途 |
| --- | --- | --- |
| T_DBA_ImpactFire_Burst64 | 2048²，8×8、64连续帧；RGBA=密度/温度/侵蚀/遮罩，线性 | 卷焰动画；非循环，按归一化生命播放完整64帧 |
| T_DBA_ImpactFire_Flow | 512²，RG有符号流向、BA扰动/厚度，线性 | 打散重复叶瓣纹理和机械同步 |
| T_DBA_ImpactContact_Mask | 512²，线性灰度/Alpha | 紧凑接触光与火星；无大面积星芒 |
| T_DBA_ImpactSmoke_Atlas | 1024²，4×4连续帧，线性密度/Alpha | 低密度暖烟与冷雾共用，实例调色 |
| T_DBA_ImpactIce_Surface | 2048²，R厚度/G粗糙度/B细裂纹/A霜边，线性 | 清透晶片的厚薄、裂口与局部霜化 |
| T_DBA_ImpactIce_Normal | 1024²，Normalmap | 微法线；主晶面由几何法线承担，不用噪声伪造 |
| T_DBA_ImpactAir_Normal | 256²，Normalmap，仅需非周期扰动时新增 | 审核专用扰动副本使用；先验证现有IOR基础能否满足 |

制作方式：
- 优先复用经过逐帧与多Mip检查的真实素材；32×32爆燃资源不通过此门禁。现有4×4火焰可验证管线，不能称为新64帧。
- 最终爆燃使用一次连续离线流体/火焰动画烘焙，缓存留在Game/Saved/Bake/ImpactRedesign。允许Blender离线源；默认不引入运行时Niagara Fluids。
- 64帧按火焰粒子的NormalizedAge映射0—63，并插值相邻帧；0.40秒生命周期相当于遍历整段烘焙，不把32fps循环序列直接套用为短命中爆燃。
- 图集每格留8像素透明保护边并验证Mip串帧；Mask、Flow、温度、厚度关闭sRGB。法线使用正确法线压缩，实际Cook格式须读回。
- AlphaComposite只预乘一次，源图与材质明确分工；颜色由线性化后的项目色板生成，不用额外Gamma补偿。

网格：
- SM_DBA_ImpactFireLobe_A/B：各约250—500三角形，有真实弯曲截面与回卷边；5个实例在三个深度层，不能全部朝向相机。厚度来自几何和流动遮罩，避免透明片堆球。
- SM_DBA_ImpactContactCore：约200—400三角形的小型不规则核心，短时支持立体感，不留下完整球形弹头。
- SM_DBA_ImpactIceShard_A/B/C：各约40—120三角形，尺寸形态有差异；闭合外表面、正确硬边法线，去除内部重叠面，禁止统一规则花瓣。
- SM_DBA_ImpactAirHull：约100—200三角形，单层局部扰动壳。是否复用已有网格以实际三角数/UV为准。
- 模型单位cm，冻结变换；源模型、导入尺寸、三角数、材质槽逐项回读。

## 5. 材质实例与初值

参数值是固定曝光下的制作起点，不代表物理单位或跨场景保证。旧接口与新增接口明确分开。

| 拟新增实例 | 父材质与真实/拟新增接口 | 初值 |
| --- | --- | --- |
| MI_DBA_ImpactFire_Contact | 复用M_PY_Additive；Energy、Density、Mask现有 | Energy=6，Density=0.85；只用于小面积接触光 |
| MI_DBA_ImpactFire_Lobe | B/Materials/M_DBA_ImpactFlame_Review；复制适用火焰母材质，保留Energy/Density；新增BurstTexture、FlowTexture、FlowSpeed、WarpStrength、Erosion、WPOAmplitudeCm | AlphaComposite/Unlit；Energy=3.5，Density=0.8，FlowSpeed=0.6，WarpStrength=0.025UV，Erosion从0.05升至0.95，WPOAmplitudeCm=1.5 |
| MI_DBA_ImpactFire_Ember | 复用M_PY_Additive现有接口 | Energy=2.0，Density=0.65；火星长度2—6cm |
| MI_DBA_ImpactFire_Smoke / MI_DBA_ImpactIce_Mist | B/Materials/M_DBA_ImpactVapor_Review；新增AtlasTexture、Tint、OpacityScale、DepthFadeCm | AlphaComposite，OpacityScale=0.12/0.20，DepthFadeCm=4；不以调低Alpha替代减少过绘 |
| MI_DBA_ImpactIce_Crystal | 先基于M_DBA_IceArrow_Crystal审核副本；保留EdgeIntensity、CrystalOpacity、SpecularLevel、IceTint、EdgeTint、IceNormal、IceORM、CrackMask；新增ThicknessTexture、Roughness | CrystalOpacity=0.50，SpecularLevel=0.5，EdgeIntensity=0.12，Roughness=0.10；单面受光透明材质 |
| MI_DBA_ImpactIce_Flash | 复用M_DBA_IceArrow_Luminous现有接口 | EmissiveStrength=3.0，OpacityScale=0.65；细小冰白光 |
| MI_DBA_ImpactFire_Air / MI_DBA_ImpactIce_Air | 复用M_DBA_PalmAirRipple_Review现有P_Intensity、P_Opacity | P_Intensity=0.006/0.002，P_Opacity=0；设置/验证真实IOR，不能只连Refraction却关闭折射模式 |

- 冰材质比较Surface与SurfaceForwardShading的原生效果和成本；不能照搬旧复杂冰材质再靠高Fresnel遮住问题。晶体内层反射/厚度近似必须如实说明，不能称为完整多次折射。
- 冰面避免两面都绘制造成重叠热点；Shader Complexity与Quad Overdraw必须实际检查。
- 发光不自动保证地面和铠甲获得稳定照明。High/Epic可在接触发射器使用至多1个无阴影Light Renderer，火150lm、冰60lm、半径150/120cm、寿命0.10秒为候选；优先验证实际Light Renderer支持的单位/接口，再校准。若接口非流明，记录映射，不把该数字直接填入无单位Intensity。
- Medium/Low关闭该辅助灯。无灯版本同时保存原生对照；不得修改整张地图曝光或加隐藏常驻灯伪造反光。

## 6. Niagara配置

共同规则：
- 每次命中一个System、四个逻辑发射器；第一版CPU模拟，按实际并发结果评估装饰是否值得GPU化，不声称CPU天然更快。
- EmitterState先于SpawnBurst；Once、无循环、无预热，所有粒子有有限Lifetime及Kill条件。此前掌心曾出现模块顺序导致双倍发射，本次必须读回实际峰值数量。
- 世界空间模拟，命中瞬间采样锚点/法线构建正交基；之后碎片独立运动，不跟随目标转动。User.ImpactLocation保持Position类型并正确处理Niagara大世界坐标，不能当普通float3直接混用。
- SurfaceOut=N=normalize(ImpactNormal)。N近零时：有可信入射方向则用-D；否则采用组件方向的反向，仅为可视回退并标记证据。选择与N不平行的参考轴建立切线，避免顶面/垂直法线退化。
- IncomingDirection在审核中可明确提供，正式当前合同没有该字段，默认-D近似为-N；不得从当前选中目标或施法者位置重新推断大弧线最后切线。
- 多渲染器通过RendererVisibilityTag分组；不能让每个粒子被所有Mesh/Sprite/Light Renderer重复绘制。
- 固定随机种子20261027，仅用于复现实验；不宣传CPU/GPU/网络跨端随机结果必然一致。

### 火球命中（High）

| 发射器 | Renderer／数量 | 生命周期与运动 |
| --- | --- | --- |
| E_FireContact | Mesh 1＋Sprite 1，按可见标签分组 | 0.08—0.10秒；6—12cm焦点；小核心收缩、闪光迅速衰减 |
| E_FireRollingLobes | Mesh 5；两种卷焰网格变体 | 0.28—0.45秒；初始长20—35cm，峰值组合宽90—110cm；向表面外法线半球展开，初速80—160cm/s，阻力4/s，上浮加速度60cm/s² |
| E_FireAccents | Sprite 14火星＋2薄烟 | 火星0.25—0.55秒、250—500cm/s、阻力4/s；烟0.45—0.70秒、12—30cm/s；组间独立材质和标签 |
| E_FireAirRipple | Mesh 1 | 0.03—0.25秒；直径由30增至120cm，0.10秒后衰减；不画可见玻璃球 |

初始峰值上限24粒子（2+5+16+1），不是测量值。爆燃叶瓣夹角、深度、旋转与UV相位不完全一致，避免规则五瓣花或平面放射线。

### 冰晶命中（High）

| 发射器 | Renderer／数量 | 生命周期与运动 |
| --- | --- | --- |
| E_IceContact | Mesh 1＋Sprite 1，分组 | 0.07—0.10秒；短时碎裂核、无完整冰弹残留 |
| E_IceFragments | Mesh数组：10大碎片＋18细屑 | 大片8—22cm、0.45—0.85秒、150—300cm/s；细屑1—5cm、0.20—0.50秒、250—500cm/s；阻力2/s，重力加速度-250cm/s²；角速度180—540°/s |
| E_IceMist | Sprite 3 | 0.03—0.85秒；初始20—30cm、峰值35—60cm，低密度卷动，保持目标上半身轮廓 |
| E_IceAirRipple | Mesh 1 | 0.02—0.22秒；直径25→110cm，低幅度压力折动 |

初始峰值上限34粒子（2+28+3+1），不是测量值。碎片主要沿N朝向的外侧半球扇出，角度相对N为20—80°；加入切向动量但投影回外半球，避免大量穿进墙面。无Niagara碰撞决定伤害；第一版装饰不启用昂贵碰撞，片段撞地穿插严重时先缩短生命和收敛速度，再评估可选视觉碰撞。

### 参数接口

| 参数 | 类型／默认值 | 来源与用途 |
| --- | --- | --- |
| User.ImpactLocation | Position | 已有平台自动写入；确认命中锚点 |
| User.ImpactNormal | Vector3 | 已有平台自动写入；表面外向法线 |
| User.IncomingDirectionWS | Vector3，默认(0,0,0) | 拟新增；审核已知切线，正式未提供时回退，不改服务器 |
| User.HasIncomingDirection | Bool=false | 拟新增；避免把零值当可信方向 |
| User.ImpactScale | Float=1，审核范围0.5—1.25 | 纯视觉尺寸，不代表伤害范围；最大倍率需覆盖Bounds |
| User.EffectIntensity | Float=1 | 主色能量倍率；限制白区，不动态提高曝光 |
| User.AirRippleStrength | Float=1 | 质量档之外的美术倍率 |
| User.QualityLevel | Int=2：Low0/Medium1/High2/Epic3 | 审核预设；现有执行器不会自动为此参数填值，正式使用原生质量设置/既有Definition质量合同 |
| User.RandomSeed | Int=20261027 | 拟新增；接入实际随机模块并回读，只有同名参数无绑定不算生效 |
| User.EnableImpactLight | Bool=true（High/Epic候选） | 只控制接触发射器的一个可选光源 |

将粒子动态材质四通道明确绑定为D0=NormalizedAge、D1=纹理相位、D2=侵蚀量、D3=强度；材质按相同合同读取。所有新参数在审核Definition的ParameterSchema及DefaultParameters登记，不能仅写文档。

FixedBounds初值火±250cm、冰±300cm；以最大ImpactScale、最高速度、全部画质和法线方向的SimCache实际粒子范围加20cm余量验证。范围不足就修Bounds或收敛设计，不能靠巨大Bounds掩盖错误。

## 7. 四档质量与性能验证

| 档位 | 火：核心／卷焰／火星／烟／扰动 | 冰：核心／大片／细屑／雾／扰动 | 空气波动与灯光 |
| --- | --- | --- | --- |
| Epic | 2/6/20/2/1，最多31粒子 | 2/12/24/4/1，最多43粒子 | 强度1、范围1；候选辅助灯开启 |
| High | 2/5/14/2/1，最多24粒子 | 2/10/18/3/1，最多34粒子 | 强度1、范围1；候选辅助灯开启 |
| Medium | 2/4/7/1/1，最多15粒子 | 2/8/8/2/1，最多21粒子 | 强度0.4、范围0.7；关灯 |
| Low | 2/3/4/0/0，最多9粒子 | 2/6/0/0/0，最多8粒子 | 扰动/雾/灯关闭，保留接触与主体 |

- 质量变化通过原生伸缩及明确参数预设生效，不建立轮询预算器。减少粒子不能令主体丢失；冰晶Low允许简化受光材质，仍须保留棱面和透明感。
- 复用ET_VFX_MagicImpact前，读取其距离、实例上限、重要性和四档覆盖。审核中报告任何被拒绝/剔除的System，不能让压力测试靠隐藏主体显得更快。
- 测试组：空场基线、单火/单冰、10次同帧命中、20次同帧混合命中，四档；50和150次/秒各10秒压力探针；10次/秒混合持续10分钟并插入取消/出入World。
- 高峰压力是诊断和项目性能门禁输入，不承诺单命中独立System方案天然满足150次/秒；若失败，先减少过绘/材质/装饰，后续再评估现有NDC路径，不能在本轮复制一套NDC框架或宣称已可投产。
- 记录实际System/Emitter/粒子峰值、GT/RT/GPU、帧时间P50/P95/P99、GPU型号、分辨率、RHI、TSR、帧率上限、相机及灯光设置。区分激活峰值和稳定播放成本。
- 使用Shader Complexity、Quad Overdraw及原生GPU计时；热图不等同GPU毫秒，不把色标当精确透明层数。编辑器数据不能冒充打包Client或十人联网验收。
- 主体先过视觉门禁，再优化；优化后相同镜头复拍，避免以降低画质掩盖与概念的差距。

## 8. 执行任务与出口

### Task 1：审核基线、接口与隔离资源

**Files:** 新建Tools/Unreal/VFX/DevelopmentMagic/AuthorImpactRedesignUE58.py；创建B/Maps、B/Definitions中的上述资产；源清单SourceArt/VFX/ImpactRedesign/AssetManifest.json。

**Interfaces:** 消费两张概念图和现有ImpactLocation/Normal；输出准确挂载路径、旧资产哈希、固定相机/曝光、角色尺度、复用参数清单。

- [ ] 重读当前编辑器和Git输入，保护原资源、脏包和其他任务；只创建拥有的审核目录。
- [ ] 建立固定相机审核地图及两份InstantDefinition，核对生命周期0.8/1.1秒和EffectType真实引用。
- [ ] 使用已有Niagara最小核心验证非原点、大世界坐标、六个法线方向、斜墙及不同体型；保持SourceRotation为零仍应正确朝向。
- [ ] **出口：** 原生位置/法线有效、正式映射无变化；不能把中心锚点说成精准胸甲表面点。

### Task 2：火球卷焰与两类接触材质

**Files:** SourceArt/VFX/ImpactRedesign/{Fire,Shared}/、B/{Meshes,Textures,Materials}/；作者仍为AuthorImpactRedesignUE58.py。

**Interfaces:** 消费第4/5节纹理和材质合同；输出火焰主网格、连续序列、核心/火星/烟雾/扰动实例与导入回读。

- [ ] 检查可复用原图的分辨率、实际帧内容、Alpha和Mip；不满足则制作连续离线源及对应图集。
- [ ] 通过Monolith导入，创建/复制审核材质并绑定真实纹理参数；原公共母材质哈希不变。
- [ ] 在黑/灰/亮背景及审核场景，关闭Bloom分别检查正侧背三视角：火焰厚度、白区比例、边缘连续性及预乘透明合同。
- [ ] **出口：** 主体无空心卷带、纸片和大白斑；失败先修网格/纹理，不能靠叠更多面片补厚度。

### Task 3：碎晶材质与两套Niagara

**Files:** SourceArt/VFX/ImpactRedesign/Ice/、B/Niagara、B/Materials、B/Meshes；新增Tools/Unreal/VFX/DevelopmentMagic/ImpactRedesignMotion.ush作为审核模块源记录。

**Interfaces:** 消费第6节参数和材质动态通道；输出两份可编译、有限生命的四发射器系统。

- [ ] 导入三类碎晶，验证尺寸、硬边法线和单面渲染；固定曝光对比透明内层与薄边高光。
- [ ] 按表创建CPU发射器，设置Once、EmitterState→SpawnBurst顺序、Life/Kill、分组标签、随机种子、FixedBounds和真实材质引用。
- [ ] 编译并保存，读回参数类型、模块顺序、Renderer材质、实际峰值粒子数，防止双发或全部粒子被全部Renderer绘制。
- [ ] 隐藏装饰层检查核心，再逐层恢复；六方向和旋转相机查看碎片厚度、法线稳定性及身体遮挡。
- [ ] **出口：** 火/冰各自匹配主体轮廓；无旧冰冠、循环喷射、僵硬对称花瓣或残留完整飞弹。

### Task 4：时序、原生截图/视频、保存重载

**Files:** 新建Tools/Unreal/VFX/DevelopmentMagic/CaptureImpactRedesignUE58.py、AuditImpactRedesignUE58.py；证据写入Game/Saved/Validation/ImpactRedesignReview20261027。

**Interfaces:** 消费两份审核系统和固定种子，输出逐阶段PNG、原速视频、可选慢放视频、SimCache及编译/加载日志。

- [ ] 记录0.04/0.10/0.16/0.35/0.65秒及自然结束后的原生图；保留无后期原图，概念对照排版另存。
- [ ] 30/60/120fps和明暗背景重放；检查短闪可读性、材质跳帧、碎晶运动和热/冷空气折动。
- [ ] 单独播放Impact后再做“当前真实Flight→新Impact”审核串联；新FlightRedesign此前只有计划，不能假称已完成并用它作实现证据。
- [ ] 检查旧飞行取消会否立即抹掉尾迹。Impact不重绘一条长尾；独立审核可展示现有组件的自然排空，正式取消语义未验证则列为未通过项。
- [ ] 运行自然结束、Deactivate、组件销毁、池化重播、审核World退出的原生探针；不通过隐藏画面伪装清理。
- [ ] 只保存本次资产；在安全的独立编辑器重载或保留用户状态后的冷重载中核对所有引用、编译和固定镜头。不能强行关闭有他人任务的编辑器。
- [ ] **出口：** 两系统及引用确实保存并可重载；火≤0.75秒、冰≤0.95秒自然完成，核心结束/残留证据齐全。正式网络生命周期仍单列。

### Task 5：四档与性能测量

**Files:** 扩展本次AuditImpactRedesignUE58.py的perf模式；原始CSV、GPU捕获与调试视图保留在本次证据目录。

**Interfaces:** 输入第7节负载和画质，输出真实成本、被剔除数、视觉损失与未通过项。

- [ ] 先采空场与单实例，再执行并发、速率、稳定性矩阵；每次保留相同镜头/输入指纹。
- [ ] 对比有无空气扰动、薄雾、光源；定位成本后只修改对应层并复测。
- [ ] 四档截图确认Low仍有命中核心、火焰/碎晶轮廓，关闭扰动不改变位置和时序。
- [ ] **出口：** 提供真实测量表，不填猜测毫秒；失败明确登记，不以单卡平均FPS宣称发布达标。

### Task 6：实施报告与人工审核停止点

**Files:** 新建Docs/Implementation/VFX/FireIceImpactNativeReview_20261027.md；更新总体目录说明中本任务条目。

**Interfaces:** 汇总实际资产路径、全部材质参数、Niagara结构、编译/保存/重载、原生图/视频、性能和剩余差距。

- [ ] 报告逐项区分计划、已创建、已编译、已重载、已测量和人工审核状态，附精确资源引用。
- [ ] 对照概念的接触焦点、厚卷焰/透明碎晶、空气波动、角色可读性与收尾节奏，不把编译成功当作视觉通过。
- [ ] **停止在人工视觉审核。** 不改正式技能、不提交主分支；人工批准后才处理正式引用、必要客户端适配及发布验证。

## 9. 人工通过后的正式接入范围（本轮不执行）

- 复用既有Definition/Catalog，不另建播放器。替换前评估DirectedMissile共用FireballImpact及Frostbolt.Residue的影响，保持原技能范围。
- 必要时在DivineBeastsDevelopmentMagicPresentation.cpp补充纯表现参数/阶段收尾，先验证已有平台接口；不能直接向Gameplay引入Niagara依赖。
- 精确入射切线或表面点若必须扩展权威复制合同，另立兼容任务。本计划默认不修改。
- 复用并必要地扩展Private/Tests/DivineBeastsDevelopmentMagicLifecycleTests.cpp，运行DivineBeasts.Presentation.DevelopmentMagic.CleanupAndSynchronousReentry，验证重复确认、取消/换World、VFX失败但SFX可用；测试成功仍不代表原生/联机成功。
- 如涉及C++，仅构建实际受影响目标，遵守既定构建参数和用户当次缓存清理授权；Cook/Stage输出留在工程内。
- 启动两个正式客户端前核对Client/Village/MainArena战斗版本与运行进程身份；从登录界面进入验证，分别报告启动、准入、玩法及人工视听结果。

## 10. 制作提示摘要

火球：以确认命中点为中心制作一个非循环System；小面积暖白金核心、五个有截面与回卷边的金橙火焰网格、极少余烬/暖烟、局部热扰动。峰值约1米，0.75秒内自然结束。按表使用四发射器、明确随机种子和材质动态通道，不能产生屏幕闪白、光环、岩浆或新伤害范围。

冰晶：以同一命中合同制作一个非循环System；小接触光、十片通透大碎晶和十八片细屑、三片薄冷雾、局部压力折动。峰值约1米，0.95秒内自然结束。晶体向法线外侧不对称散射，不生成冰冠或冻结状态；保留角色轮廓。以上提示是作者规格，必须通过真实Monolith操作、编译、运行和重载落地，提示词本身不构成Niagara资产。

官方技术参考：[Niagara伸缩与实例/发射器优化](https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara)、[材质混合与AlphaComposite公式](https://dev.epicgames.com/documentation/en-us/unreal-engine/material-blend-modes-in-unreal-engine)。仅采用与本机UE5.8实际接口核对一致的部分；所有性能结论以本项目实测为准。

