# SSLM 火球飞行与冰晶飞行 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. 按任务逐项落实；原生资产写入串行通过Monolith，不自动提交主分支。
>
> **Goal:** 制作与两张飞行概念稿轮廓、材质和运动层次相符的两套可编辑UE5.8审核特效。
>
> **Architecture:** 保留GamePlatformPresentation→GamePlatformVFX唯一执行链。表现资源归第三层DBAPresentationPack_Core；MOBA层不承担粒子播放，项目内容不进入平台层。首先在独立审核地图制作，通过人工审核后才接入已有飞行Definition。
>
> **Tech Stack:** UE5.8、Niagara CPU Mesh/Ribbon/Sprite、材质实例、Monolith MCP、本地离线纹理烘焙及原生采集工具。
>
> **Spec:** `Docs/Art/VFXConcepts/20261026_FlightRedesign/SSLM_FireballFlight_Redesign_Concept.png`、`SSLM_IceCrystalFlight_Redesign_Concept.png`及同目录`GenerationPrompts.json`。
>
> **状态：** 本轮完成只读核对和执行计划；下列数值是制作初值与验收目标，不是已实现参数或性能承诺。

## Global Constraints

- 本轮只制作飞行阶段，保留原V3、掌心审核版和正式技能引用；不扩展命中、爆炸或服务器玩法。
- 不调用HWebCodex。真实uasset/umap通过UE5.8及Monolith创建、编译、保存、重载，不写假资产。
- 依赖方向DivineBeasts→MobaCommon→GamePlatform；不新建VFX播放器、池、预算服务或资产加载器。
- 飞行组件跟随既有权威弹体及客户端呈现位置；Niagara只画已确认的运动，不另算追踪目标、贝塞尔权威路径或碰撞。
- 三条拖尾相位为0°、60°、120°，相邻两条差60°；不擅自改为0°、120°、240°。
- 全部效果有局部空气波动；可降级，不能全屏折射、遮蔽目标或代替主体。
- 固定曝光、原生PNG和视频验收；概念图、脚本成功、资产存在均不能当作引擎效果通过。
- 按当前主工作空间独立审核目录操作；不切换当前工程，不覆盖手工地图或其他脏包，不提交主分支。
- 本阶段不需要C++全量构建或清缓存。需要Cook/Stage时输出及中间文件均在当前工程内；正式客户端启动前执行Client/Village/MainArena战斗版本一致性门禁。

## Review Focus

1. 高速和低帧率：3000/4000/5000cm/s、30/60/120fps，短尾迹不能因时间采样稀疏而断线或只剩两点；归任务3。
2. 急转弯与坐标跳变：大弧线、垂直飞行、网络校正、传送，尾迹不能整体转动、翻转或拉成长桥；归任务3。
3. 亮背景与侧背视角：火球不能呈空心带，冰晶不能变石块、塑料或粗白描边；归任务1、2。
4. 取消与重用：取消、弹体结束、World退出及池化重播，不能恢复旧RibbonID、位置或计时；归任务4。
5. 近景叠层与多人并发：不能以少量活粒子或高端显卡平均帧率掩盖透明过绘热点；归任务5。

## 1. 当前工程事实与复用范围

2026-10-26只读核对：Monolith连接UE5.8.0，PID65904，实际工程为`Game/DivineBeastsArena.uproject`；审核地图为PalmRedesign，未运行PIE、无脏包。当前main存在其他未提交修改，保留。

现有资源根：
`/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight`。

| 已存在资源 | 实际接口／结构 | 处理 |
| --- | --- | --- |
| NS_DBA_FireOrbFlight_V3 | Core、SurfaceFlames为GPU局部空间，FireTrail为CPU世界空间；User参数为空 | 复用结构参考，另建审核副本 |
| NS_DBA_IceCrystalFlight_V3 | Core、FacetCluster为GPU局部空间，FrostTrail为CPU世界空间；User参数为空 | 不覆盖原系统 |
| M_DBA_FireOrbVolume_V3 | RadiusCm、FormProgress、EmissionScale、DensityScale | 保留这些真实命名；新参数只在审核材质新增 |
| M_DBA_FireTail_V3 / M_DBA_FrostTail_V3 | 公开标量只有Flavor，暂无纹理参数 | 不冒称已有宽度/流速/密度接口 |
| MI_DBA_IceBody_V3 | EdgeIntensity、CrystalOpacity、SpecularLevel；IceTint、EdgeTint；IceNormal、IceORM、CrackMask | 原生母材质为 /DBAPresentationPack_Core/VFX/IceArrow/Materials/M_DBA_IceArrow_Crystal |
| T_PY_FlameFlipbook | 已核对FireMage的4×4序列 | 用于首轮验证，不能把16帧插值称为新64帧流体序列 |
| PalmRedesign空气扰动与采集经验 | 已有真实IOR层、四档/SimCache/CSV/冷重载流程 | 复用方法和可用基础，不继承高过绘结果为合格 |

正式调用方`DivineBeastsDevelopmentMagicPresentation.cpp`以Persistent请求附着真实Projectile RootComponent。平台Definition已经有ParameterSchema、DefaultParameters、QualityVariants接口，不需要第二套传参服务。

现有C++法术定义默认速度为3000cm/s、允许3000—5000cm/s；真实运行还需读取权威资产/服务器事实。本次美术实施不修改该值，也不把审核动画速度当作权威速度。

## 2. 造型方案

选择**实体网格弹头＋动画材质＋三条世界空间带状尾迹＋少量装饰＋局部扰动**。不采用大量交叉透明片堆球，也不采用运行时Niagara Fluids作为默认。

| 项目 | 火球飞行 | 冰晶飞行 |
| --- | --- | --- |
| 主轮廓 | 直径60cm，允许55—70cm；近球形、有厚度、向后略拉伸 | 长65cm、宽35cm；单一尖头棱晶，三片贴身后掠晶鳍 |
| 结构 | 密实体积承托层＋一层低重叠卷焰外壳；白金高温区控制在弹头可见面积约10%以内 | 一个合并的闭合晶体网格；非六瓣莲花、非球形冰壳 |
| 色彩 | 朱焰橙#F16B46为基底，金黄/琥珀中层，小面积暖白 | 寒蓝#409CE5与浅蓝#C4DCFF，深蓝内核，细窄冰白高光 |
| 拖尾 | 150cm起步，三股宽幅火流，末端破碎消散 | 180cm起步，三股较轻冷流，少量冰屑、薄雾 |
| 禁止 | 空心S形带代替球体、粗亮边、整球曝白、连续激光束 | 粗白描边、均匀蓝色实片、颗粒状霜噪声、雾遮弹头 |

比例和色彩是审核初值；必须在全景和近景都与概念对照，不能靠改变曝光弥补材质错误。HEX是屏幕sRGB参考，写入UE向量前转换线性颜色。

## 3. 纹理与网格制作清单

新资源根拟定为：
`/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/FlightRedesign`，下文记为B。
磁盘归同一内容插件`Content/VFX/OrbProjectile/Review/V3/FlightRedesign`；
可编辑源归`SourceArt/VFX/FlightRedesign`。

| 拟制作／复用纹理 | 规格 | 通道及用途 |
| --- | --- | --- |
| T_DBA_FlightFire_ShellPacked | 2048²、线性 | R密度、G温度、B侵蚀、A厚度；大尺度卷纹为主 |
| T_DBA_FlightFire_Flow | 1024²、线性 | RG双通道流向，解码为-1..1；B低频变化、A遮罩 |
| T_DBA_FlightFire_Licks | 2048²，8×8共64帧 | RGBA连续火舌图集，256²/帧；32fps、2秒循环，帧间插值与边缘保护 |
| T_DBA_FlightTrail_Packed | 1024×256、线性 | 连续带状密度、侵蚀、流动噪声；火/冰实例分别调制 |
| T_DBA_FlightIce_Surface | 2048²、线性 | R厚度、G粗糙度、B少量裂纹、A局部霜面；不要满面裂纹 |
| T_DBA_FlightIce_Normal | 2048²、Normalmap | 小尺度微法线；大晶面由几何法线表达 |
| T_DBA_FlightFrost_Mist | 优先核对现有1024²/16帧；不足则2048²/64帧 | 连续稀薄冷雾及真实Alpha |
| T_DBA_FlightAir_Normal | 256²、Normalmap | 平滑扰动，复用合格已有资源时不重复制作 |

图集必须来自连续动画源。现有FireMage先验证材质链；若无法提供概念的厚实翻卷，使用本机已安装的Blender 5.2制作离线源并烘焙64帧，缓存只放项目内。不得用64张独立生成图拼出跳动序列；离线烘焙不是运行时流体。

Masks/Flow/厚度/粗糙度/Alpha关闭sRGB，法线使用Normalmap导入；颜色贴图才启用相应sRGB合同。BC5等实际平台压缩与Cook格式需原生回读，不仅检查文件后缀。图集各格保留透明边与扩边，验证低Mip不串帧。

网格候选：
- SM_DBA_FireFlightBody：800—1200三角形，连续闭合的轻微起伏球体；WPO幅度1—3cm。
- SM_DBA_FireFlightFlameHull：500—900三角形，一层有厚薄变化的卷焰轮廓，避免穿插三层球。
- SM_DBA_IceFlightCrystal：1200—1800三角形，主体与三片晶鳍合并为单个静态网格，外表面闭合；移除不可见内部重叠面。
- SM_DBA_IceFlightShard：30—80三角形，共用少量变体。
- 扰动使用低面数闭合网格，复用已有模型时核对实际三角数，不能直接把高细分Sphere视为免费。

以上为源网格制作上限候选，运行时实际提交数另测。

## 4. 材质配置与初值

所有新母材质只位于B/Materials。先复用原母材质可满足的部分；必须改图时复制到审核目录，不修改公共母材质。

| 材质／实例方案 | 模型 | 起始参数 |
| --- | --- | --- |
| M_DBA_FireFlightBody_Review → MI_DBA_FireFlightBody_Review | 主体Unlit/Opaque承托体积；温度场与动态流动形成火焰色层；不使用石纹 | RadiusCm=30，FormProgress=1，EmissionScale=4，DensityScale=1，FlowSpeed=.45周期/秒，WarpStrength=.06UV，WPOAmplitudeCm=2 |
| M_DBA_FireFlightSkin_Review → MI_DBA_FireFlightSkin_Review | 单面AlphaComposite卷焰外壳；少量辅助图集火舌 | EmissionScale=3，DensityScale=.65，FlipbookFPS=32，Erosion=.25；外壳局部透明，不能令主体空心 |
| M_DBA_FlightRibbon_Review → MI_DBA_FireFlightTail_Review | AlphaComposite；宽度/长度由Niagara控制，UV沿弧长平铺 | EmissionScale=2.5，DensityScale=.55，FlowSpeed=.8，TailFadePower=1.7 |
| M_DBA_IceFlightCrystal_Review → MI_DBA_IceFlightCrystal_Review | 单面Translucent/DefaultLit；几何晶面、厚度吸收及有界环境反射 | CrystalOpacity=.58，SpecularLevel=.5，Roughness=.10，EdgeIntensity=.15，NormalStrength=.30，EmissionScale=1.1 |
| M_DBA_FlightRibbon_Review → MI_DBA_IceFlightTail_Review | 冷色AlphaComposite，低密度纹理，禁止直接把火焰RGB染蓝 | EmissionScale=1.2，DensityScale=.30，FlowSpeed=.45，TailFadePower=2 |
| M_DBA_FlightMist_Review → MI_DBA_IceFlightMist_Review | AlphaComposite，单次Alpha包络 | OpacityScale=.20，EmissionScale=.8，图集帧间插值 |
| M_DBA_FlightAir_Review → Fire/Ice实例 | 原生局部折射；只覆盖弹头附近 | Fire IOR偏移幅度=.006，Ice=.002；默认一个扰动粒子，强度渐入渐出 |

冰晶实际照明模式先对比Surface与SurfaceForwardShading的同角度结果和成本；前者若仍有塑料感，不用加亮边掩盖，改后者并重新测量。内部反射可用厚度图与环境图近似，但必须标明不是完整光线追踪多次折射。外轮廓不能依赖高强度Fresnel白环。

AlphaComposite输出合同：源数据保存直通颜色，Emissive=线性颜色×EmissionScale×Alpha，Opacity=Alpha；只预乘一次。普通Translucent按其模型输入，不复制同一预乘公式。所有参数名除第1节已存在者以外均为拟新增接口。

概念图的地面暖光单列为可选装饰：默认不开额外灯光；审核可测试同一主体发射器内一个无阴影Light Renderer，100lm、150cm半径，并单独记录成本和画面差异。不得为了对齐概念给每个粒子加灯或修改地图全局照明。

## 5. Niagara系统、参数及真实曲线拖尾

拟创建：
- `B/Niagara/NS_DBA_FireballFlight_ConceptReview`
- `B/Niagara/NS_DBA_IceCrystalFlight_ConceptReview`

每颗弹体一个System、四个逻辑Emitter；少量主体先用CPU模拟，是否改GPU以实测决定。优先复用已存在的 `/DBAPresentationPack_Core/VFX/CommonSkillVFX/Runtime/DirectedMissile/Scalability/NET_DBA_Review_CombatProjectile`，实施前核对其实际伸缩设置，不把施法CombatCast类型直接用于飞行。

| 层 | 火球High初值 | 冰晶High初值 |
| --- | --- | --- |
| E_Body | 2个局部空间Mesh：承托主体＋外壳 | 1个局部空间合并晶体Mesh |
| E_Trails | 一个世界空间CPU Ribbon Emitter内3个RibbonID | 同左 |
| E_Accents | 最多12个活粒子，火舌/火星共用发射器，主图不靠它成立 | 最多12个活粒子，冰屑/雾通过Renderer Visibility Tag区分，禁止两个Renderer无差别全画 |
| E_AirRipple | 1个局部空间Mesh，范围约90cm | 1个局部空间Mesh，范围约85cm |

### 新增User参数合同

| 参数 | 类型／单位 | 火／冰默认 |
| --- | --- | --- |
| User.BodyScale | float，无量纲 | 1 / 1 |
| User.EffectIntensity | float | 1 / 1 |
| User.TrailLengthCm | float，cm | 150 / 180 |
| User.TrailSpacingCm | float，cm | 4 / 4 |
| User.TrailWidthCm | float，第一股最大宽度 | 18 / 12；另两股乘.78、.56 |
| User.TrailPhaseStepDegrees | float，度 | 60 / 60 |
| User.TrailOrbitRadiusCm | float，尾迹起点横截面螺旋半径 | 14 / 9；随尾部归一化弧长u乘(1-.6u) |
| User.TrailTwistCyclesPerMeter | float，圈/米 | .35 / .25 |
| User.AccentMaxCount | int | 12 / 12 |
| User.AirRippleStrength | float，倍率 | 1 / 1 |
| User.QualityLevel | int，Low=0..Epic=3 | 2（High审核） |
| User.ElapsedFlightSec | float，秒 | 审核0；正式只有已验证来源才能传入 |
| User.StopEmission | bool | false |
| User.ResetTrail | bool，单次重置请求 | false |

这些字段当前原V3均不存在。材质以DynamicParameter传入阶段时间/粒子种子/层与淡出信息；不使用全局Time造成暂停后继续流动。正式配置应进入既有Definition ParameterSchema和DefaultParameters；未知参数在接入前必须验证。

### 拖尾算法必须这样落地

1. 弹头组件继续附着真实弹体RootComponent，主轴沿其当前飞行方向；不再在Niagara内追踪一个目标坐标。
2. 尾迹在世界空间保存已经飞过的位置。按累计弧长每4cm产生一个采样位置，并为3个RibbonID各生成一个点；不足一段的距离余量跨帧保留。
3. 使用相邻位置/切线构建连续输运坐标架，在出生时施加0°/60°/120°的螺旋偏移；相位随累计路径长度×TrailTwistCyclesPerMeter连续增长。横截面半径和尾端收束采用上表数值；起点置于弹头后部，使宽带与球壳/晶体后缘相接。旧点不跟随弹头继续旋转，竖直飞行不使用会退化的固定WorldUp叉乘。
4. RibbonLinkOrder由各条带的递增采样序号产生，不能依赖数组排列或在重复出生时重用旧序号。三条带不能串成一条。
5. High采用Tube形Ribbon、TubeSubdivisions=4、TessellationFactor最大2；UE5.8本地头文件已确认这些接口。形状参数须按Monolith实际schema设置及回读。
6. 150cm火尾在4000cm/s下仅相当于约37.5ms历史，180cm冰尾约45ms。不能用每秒20—40点的时间发射模拟。必须验证帧内插值出生点及粒子年龄，30fps也不能只剩1—2个尾点。
7. 以尾部弧长裁去旧点，再加最大时间兜底；速度变化不能突然拉长为光束。每条点数约ceil(长度/间隔)+1，High预期火39点、冰46点；包含头尾锚点的实际数量用SimCache验证。
8. 当前头部锚点每帧更新且与最新采样点连续连接；突然跨越阈值的位移执行切断重建，不能连成跨地图长线。阈值拟为max(2×尾长, 2×最近有效速度×dt)，验证低帧率不误断。
9. 不把概念图中一条漂亮弧线固定写死为所有弹体路径。审核用固定种子弧线，正式只消费既有真实运动。Ribbon不碰撞，不负责命中或伤害。

FixedBounds初值各轴±350cm，以当前弹头为中心覆盖尾迹和WPO；需验证急转弯及相机边缘，不能用覆盖全地图的大Bounds防止误裁。System重用必须重置距离余量、RibbonID代次、LinkOrder、上一个世界位置和输运坐标架。

### 四档初值（均保留三条尾迹及关键弹头）

| 档位 | 弹头 | 采样间隔/Tube边数 | 活装饰上限 | 扰动 |
| --- | --- | --- | ---: | --- |
| Epic | 完整 | 3cm / 6 | 20 | 强度1、范围1 |
| High | 完整 | 4cm / 4 | 12 | 强度1、范围1 |
| Medium | 简化外壳/低阶冰材质 | 6cm / 3 | 4 | 强度.4、范围.7 |
| Low | 核心弹头保留 | 10cm / 3 | 0 | 不生成 |

可降级内容只影响装饰；不得裁掉敌方弹头或方向信息。原生伸缩的局部范围和项目竞技覆盖必须通过运行验证，不建立全局第二预算。

## 6. 分任务执行与门禁

### 任务1：先完成静态弹头材质样件

**文件：** B/Meshes、B/Textures、B/Materials；SourceArt/VFX/FlightRedesign；
拟新增`Tools/Unreal/VFX/DevelopmentMagic/AuthorFlightRedesignUE58.py`。
**接口：** 输入两张概念图与保护清单，输出单独可预览的Mesh/MI，不制作完整飞行系统。

- [ ] 固定旧系统、公共材质、正式Definition的哈希与引用，记录编辑器脏包并保护。
- [ ] 制作球体/冰晶可编辑模型；通过Monolith导入并读回尺寸、UV、法线、三角数和材料槽。
- [ ] 制作上述纹理，原生检查颜色空间、Alpha、Mip、图集逐帧连续性。
- [ ] 在黑/灰/白及庭院背景，以正面/侧面/背面固定曝光截图；此时关闭拖尾、火星、冷雾、Bloom辅助。
- [ ] **出口：** 火球无空心带，冰晶不呈石块；轮廓/比例不合格则修主体，不进入拖尾制作。

### 任务2：制作两套飞行动画主体

**文件：** 两份新Niagara系统，`B/Modules/NM_DBA_FlightBody_Review`，对应材质实例。
**接口：** 消费任务1的Mesh/MI和局部组件姿态，输出各自4发射器中的Body与Accents。

- [ ] 从现有系统复制审核副本；使用原生AddEmitter完整链路，不用已知会漏系统图的快捷复制路径。
- [ ] 实现主体缓慢转动、卷焰流动、晶面闪动，主轮廓不跟随噪声破碎。
- [ ] 原生编译后运行SimCache，核对真实数量、MeshIndex、Renderer Visibility及材质绑定。
- [ ] 将主体旋转一周，确认效果不只在概念单角度成立；比较双面开关导致的成本与视觉差异。
- [ ] **出口：** 主体连续运行、0错误/警告，无默认棋盘材质；视觉基本成立后才接拖尾。

### 任务3：三条曲线拖尾与空气扰动

**文件：** `B/Modules/NM_DBA_FlightTrail_Review`、新Ribbon/扰动材质；
作者源`Tools/Unreal/VFX/DevelopmentMagic/FlightRedesignTrail.ush`。
**接口：** 消费组件世界运动与User距离参数，输出3组持久RibbonID，不接受TargetActor。

- [ ] 先记录旧版在高速、大弧线的断线/细丝问题，以同样路径作为回归输入。
- [ ] 实现上述距离采样、世界历史、连续输运坐标架和3条链接；读回RibbonID/LinkOrder/位置。
- [ ] 覆盖3000/4000/5000cm/s×30/60/120fps、竖直飞行、90°转向、校正及传送；停止时不把尾点吸回弹头。
- [ ] 加入有界空气扰动，原生确认实际Refraction模式开启；输出同镜头开/关对照。
- [ ] **出口：** 三股相位正确、转弯不翻面、不连接旧生命周期，关闭装饰仍有完整弹头与方向。

### 任务4：独立审核场景及生命周期

**文件：** `B/Maps/L_DBA_FlightRedesign_Review`；
拟新增`CaptureFlightRedesignUE58.py`、`AuditFlightRedesignUE58.py`（同工具目录）。
**接口：** 只驱动自有审核组件，复用现有原生截图、SimCache、CSV采集方法，不连接正式Catalog。

- [ ] 固定随机种子31026、镜头、曝光、背景、两单位距离和弧线路径；记录相机变换，不只依赖窗口外观。
- [ ] 采集全景、弹头近景、侧背面、Low及扰动关闭图，逐项对照概念。
- [ ] 原速完整飞行与明确标注慢放视频分开；检查弹头与前沿尾迹连续，禁止用隐藏组件证明自然结束。
- [ ] 验证停止发射后尾迹排空、强取消、重复启动、池化复用、迟到与World退出；明确组件测试和正式GAS测试的边界。
- [ ] 保存只属于本轮的资产；独立编辑器重载后再运行，保护原资产哈希不变。
- [ ] **出口：** 提供真实PNG、动态视频和重载证据；不声明联网通过。

### 任务5：性能审核与必要优化

**文件：** 现有质量预设、原生Effect Type、审核报告。
**接口：** 消费任务4可重放场景，在相同机器/相同输入下输出测量，不修改服务器规则。

- [ ] 分别测空场景、1/10/20弹体、火冰混合；四档实际生成数量、Emitter数、System数和GPU/GT/RT分开统计。
- [ ] 每档包含持续可见负载和重复发射两个测试，记录采样窗口，不让间隔帧稀释峰值。
- [ ] 原生Shader Complexity/Quad Overdraw检查全景与近景；确认截图是真正调试视图。
- [ ] 先去掉闭合网格无必要背面、内部叠面、无效透明边及重复图集采样，再减装饰；不能只降低Opacity声称减少过绘。
- [ ] 记录画质切换及恢复是否闪断；跑10分钟取消/重播循环检查组件和内存稳定性。
- [ ] **出口：** 真实成本与未达项清晰。沿项目已有预算候选审查，不自行承诺某毫秒或FPS；未达标不标为可投产。

### 任务6：交付人工审核，之后再接正式技能

**文件：** `Docs/Implementation/VFX/FireIceFlightNativeReview_20261026.md`；
未来获批后修改真实Flight Definition、对应ParameterSchema/DefaultParameters及必要项目适配。
**接口：** 本阶段交付独立审核System；正式适配继续使用当前Request.AttachComponent链。

- [ ] 交付两套真实系统、准确依赖/参数、原生编译/保存/重载证据、固定镜头截图和原速/慢放视频。
- [ ] 列出视觉差距，特别是火焰厚度、冰晶内部层次、地面光与空气波动；不得用“技术通过”代替概念一致性。
- [ ] **停在人工视觉审核。** 未经下一次明确确认，不替换正式Flight定义，不提交主分支，不制作命中或爆炸。
- [ ] 获批后才完成正式绑定、参数白名单、取消/来源换代回归及Client Cook/Stage；真实联机前检查Village/MainArena版本一致，服务器不包含纯表现资产。

## 7. 本轮结果与技术依据

已完成：正确工程/Monolith检查、原飞行系统/母材质接口回读、正式附着调用方检查、三层归属确认、可执行计划。尚未创建上述FlightRedesign审核资产，没有将本计划称为已经实现效果。

Niagara的Mesh/Ribbon/Sprite职责及形状/绑定以本机UE5.8反射和源码为准；Emitter、System和过绘成本分别测量。参考Epic官方[Renderer说明](https://dev.epicgames.com/documentation/en-us/unreal-engine/render-module-reference-for-niagara-effects-in-unreal-engine)与[性能及伸缩建议](https://dev.epicgames.com/documentation/en-us/unreal-engine/scalability-and-best-practices-for-niagara)。这两页支持技术选择，不证明项目已达到预算。
