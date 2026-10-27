# SSLM 火系／冰系掌心效果图实施计划（2026-10-26）

> 执行方式：使用 superpowers:executing-plans 在当前会话通过本地工具及 Monolith MCP 逐项执行。以下复选框记录真实完成情况；本文件是计划，不是资源、性能或人工验收通过证明。

**目标：** 将已选定的“赤金焰核”和“霜蓝晶核”效果图转化为可编辑、可降级、可复验的 UE5.8 掌心 VFX，先交付独立审核版本。

**架构：** 项目美术资源归 DBAPresentationPack_Core，角色骨架与技能语义适配归 DBAClient，播放、生命周期与预算继续归 GamePlatformPresentation / GamePlatformVFX。不新增框架、池、全局计时器或资产管理器。

**技术：** UE5.8、Niagara CPU Mesh/Sprite、离线纹理烘焙、材质实例、Monolith 原生编译与保存、固定镜头 PIE 审核、GPU/CPU 原生分析。

**视觉规格：** `Docs/Art/VFXConcepts/20261026_PalmRedesign/SSLM_FirePalms_Redesign_Concept.png` 与 `SSLM_IcePalms_Redesign_Concept.png`。它们是 AI 概念设计图；评价依据是轮廓、层次、颜色、尺度、附着及动态表现，不把离线概念图称为引擎截图。

## 1. 范围与当前事实

- 本轮完成两个掌心审核效果；不修改飞行、命中、爆炸，不把掌心阶段改成中央大火球。此前 P1—03/04 草稿保持独立，后续合成另做连续性验证。
- 2026-10-26 通过 Monolith 确认当前工程为 `Game/DivineBeastsArena.uproject`、UE `5.8.0-0+UE5`。内容包注册表已登记 `Presentation/DBAPresentationPack_Core`。
- 现有 `NS_DBA_FirePalmCast_V3` 与 `NS_DBA_IcePalmCast_V3` 位于 `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/CastFlight/Niagara/`。两者均为三个 CPU、Local Space、Sprite 发射器，Fixed Bounds 开启、Effect Type 为 null。
- 现有参数只有 `User.CastDurationSec=0.5`、`User.ElapsedAtStartSec=0`、`User.HandSideSign=1`。
- `MI_DBA_FirePalm_Flame_V3` 使用 `M_DBA_PalmFireDirect_V3`：Energy=2.2、Density=0.72；Ember 使用原 `M_PY_Additive`：Energy=2.7、Density=0.5。
- `MI_DBA_IcePalm_Crystal_V3` 与 `MI_DBA_IcePalm_Mist_V3` 共用 `M_DBA_PalmMistDirect_V3`；前者 EmissiveStrength=1.65、OpacityScale=0.72，后者为 1.15、0.55。两个 Direct 母材质均为 Translucent、Two Sided，启用 Niagara Sprite、未启用 Niagara Mesh。
- 最新原生检查中冰掌粒子在正确掌位且 Alpha 非零，但几乎不可见；确切原因仍需逐项隔离纹理通道、乘色、透明度、朝向和曝光。不能把增亮参数当成已查明原因。
- 真实调用方 `DivineBeastsDevelopmentCastingWorldSubsystem.cpp::UpdateDirectedMissilePalms` 已按左右手分别创建附着锚点和表现请求，并消费服务器确认的 DurationSeconds。审核版优先保持此兼容合同。
- 当前为 main 分支，存在既有未提交修改。仅增量增加本计划及审核目录，不提交主分支，不覆盖其他任务的 AGENTS.md、正式资产或人工场景。

## 2. 方案选择与视觉标尺

选用“有体积的 Mesh 主体 + 动画纹理 + 少量装饰 Sprite”。纯 Sprite 无法稳定表达冰晶棱面和火焰卷曲轮廓；运行时 Niagara Fluids 不作为默认方案。离线流体烘焙仅生成纹理，不产生运行时流体模拟成本。

下表尺寸为根据效果图制定的首轮制作标尺，单位厘米，最终以同镜头审核锁定，不宣称概念图含有实测尺寸。

| 项目 | 火系 | 冰系 |
|---|---|---|
| 单掌外包络 | 宽20、高28，允许微调±10% | 宽20、高24，允许微调±10% |
| 掌心核心 | 直径4，白金亮部占可见效果面积约5%—10% | 直径6，白蓝内部小核心、冰蓝外体 |
| 核心离掌高度 | 底部距掌面2 | 底部距掌面2 |
| 主体结构 | 3片宽幅三维翻卷火焰；尖梢只占少量轮廓 | 6片有厚度弯曲冰瓣，围绕晶核分层张开 |
| 颜色组织 | 白金 → 金黄／橙金 → 朱焰橙 #F16B46 | 白蓝 → 透明浅冰蓝 → 寒蓝 #409CE5 |
| 外围装饰 | 少量火舌末梢、火星、局部热扰动 | 贴掌冷雾、少量冰屑、克制空气折动 |
| 禁止结果 | 细线团、平面火片、白色爆光球、整臂着火 | 不可见、蓝色火焰、实心石块、浓雾盖住晶核 |

HEX 均为屏幕 sRGB 参考；制作时按项目 V3 色彩规范转为 UE 线性值。禁止把 HEX 的 0—255 除255后直接当线性值使用。固定曝光下先调主体色与 Alpha，再调 Energy，最后检查 Bloom。

角色手指、前臂和胸部轮廓必须清楚。概念图中的手心朝上姿势需要真实动画与掌面坐标共同成立；不能仅靠 Niagara 偏移修补错误手势，也不能用冻结的审核姿势证明实战附着正确。

## 3. 文件与资源归属

**真实审核挂载点（计划新增）：** `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign/`

**对应磁盘根：** `Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/OrbProjectile/Review/V3/PalmRedesign/`

| 子目录 | 职责及计划资源 |
|---|---|
| Textures | 下节列出的线性流动、火焰、冰面与冷雾纹理 |
| Meshes | `SM_DBA_PalmFireCurl_A/B/C`、`SM_DBA_PalmIceCore`、`SM_DBA_PalmIcePetal_A/B/C` |
| Materials | `M_DBA_PalmFireCurl_Review`、`M_DBA_PalmIceCrystal_Review`；复用母材质能通过单层检查时只增加 MI；另建冷雾母材质仅在原母材质确实不满足时进行 |
| Niagara | `NS_DBA_FirePalmCast_V3_ConceptReview`、`NS_DBA_IcePalmCast_V3_ConceptReview`；必要的项目审核运动模块沿用已有模块接口 |
| Maps | `L_DBA_PalmRedesign_Review`，固定灯光／曝光／机位的独立审核地图 |

源文件归同一内容包的 `SourceArt/VFX/PalmRedesign/`，包含可编辑几何源文件、连续烘焙帧、纹理、参数预设、来源及许可记录。源文件与运行资产分开，禁止将整张概念图贴到面片上当技能。

作者与验证工具计划为 `Tools/Unreal/VFX/DevelopmentMagic/AuthorPalmRedesignReviewUE58.py`、`CapturePalmRedesignReviewUE58.py`、`AuditPalmRedesignReviewUE58.py`；只操作拥有者清单中的审核资产，必须有完整中文职责说明、异常退出和源资源保护。

原 `NS_PY_Charge`、FireMage 纹理、原 V3 四套系统及 P1—01/02 保持原身份。可复用资源以软引用/现有内容依赖使用；需要改图时建立独立审核派生，禁止修改公共母材质。

## 4. 纹理制作计划

| 资源名（计划） | 规格与通道 | 制作与用途 |
|---|---|---|
| `T_DBA_PalmFire_Flipbook` | 2048²、8×8、64帧；R温度、G主体密度、B侵蚀细节、A轮廓；线性 | 离线烘焙宽幅火焰连续翻卷。24fps起点，首尾连续循环；相邻帧插值。每格边界留8像素扩边，检查Mip串帧。不要将64张独立AI图拼成动画。 |
| `T_DBA_PalmFire_Flow` | 512²；RG有符号二维流向编码，BA备用；线性 | 沿卷片UV输送纹理，环流接缝连续，源文件保存流向约定。不得直接当法线解码。 |
| `T_DBA_PalmIce_Surface` | 1024²；R局部霜化、G几何厚度、B粗糙度变化、A内部裂纹强度；线性 | 从晶核/冰瓣实际网格烘焙；保留大而干净的晶面，裂纹只作局部内部细节。 |
| `T_DBA_PalmIce_Normal` | 1024²切线法线；Normalmap压缩、sRGB关闭 | 大晶面由真实几何承担，法线只增加内部微弱折面，不能靠高频噪声替代晶体。 |
| `T_DBA_PalmMist_Flipbook` | 1024²、4×4、16帧；RGB柔和散射、A密度；此版按线性灰度场制作 | 离线连续冷雾卷动，12fps起点，相邻帧插值；从掌面附近向外缓慢流下。 |
| `T_DBA_PalmRipple_Normal` | 512²连续循环法线；优先复用既有合法扰动纹理 | 火焰热扰动和冷气折动共享纹理、分开实例参数。 |

原 `T_PY_FlameFlipbook` 优先用于小火舌/接线验证，原火星纹理继续复用。主体新纹理只有在单层近景检查证明原纹理不满足宽幅卷曲和时间连续性后才制作；已有素材可达到视觉标准则不重复生成。

导入：遮罩/流向/厚度为线性；法线使用引擎法线压缩；RGBA多通道优先在Win64实际Cook格式中检验BC7或等价保真格式，不能让独立通道相互污染。保留Mip、以目标距离验证；不对整张Atlas使用会跨格破坏透明边界的锐化。记录原始与导入Alpha、解码方向、帧序和许可。

## 5. 网格与材质

### 5.1 几何

- 火焰主体：三种具有真实弯曲、凹凸和前后遮挡关系的卷片/开放壳网格，单片LOD0约300—500三角形；沿长度布置UV，顶点色R作为摆动权重、G作为根部锁定、B作为边缘衰减。不是三张互相垂直的平板。法线连续，卷片根部不穿指，包围盒覆盖最大WPO。
- 冰晶核心：约160—300三角形，非均匀菱形/圆润多面体，避免规则石球。冰瓣：三种真实封闭薄壳，单片LOD0约100—180三角形，根厚约0.4—0.8cm、尖端自然收薄；每掌交错使用6片，避免共面与排序翻转。
- 同类共享材质和网格资源，MeshIndex选择变体；首轮不依赖Nanite或运行时细分。简化LOD必须保住轮廓。

### 5.2 材质实例初值

以下为**新审核材质的候选初值**，不是当前原生母材质已有全部接口，也不是实测最优值。沿用真实存在的 Energy/Density/OpacityScale 等名字，其余接口由审核母材质明确定义。

| 实例／模式 | 参数初值 | 实现要点 |
|---|---|---|
| `MI_DBA_PalmFire_Core_Review`；复用Additive Unlit | Energy=10，Density=0.9；核心直径4cm由Niagara控制 | 白金只占很小面积，核心有结构而非大光晕。关闭动态点光，不依赖Lumen发光照明保证可见性。 |
| `MI_DBA_PalmFire_Curl_Review`；AlphaComposite Unlit、Mesh | Energy=3.5、Density=0.85、FlowSpeed=0.35 UV/s、FlowStrength=0.04 UV、ErodeAmount=0.18、WPOAmplitudeCm=0.8 | 火焰轮廓由几何与Alpha共同形成；颜色按温度采样白金/金黄/橙红曲线；不是只输出单一橙色。 |
| `MI_DBA_PalmFire_Detail_Review`；复用Flipbook/Additive | Energy=2.0、Density=0.45、FlipbookFPS=24 | 仅少量末梢和火星。对Mesh主体使用材质内Atlas坐标，不误用仅依赖Sprite SubImage数据的接线。 |
| `MI_DBA_PalmIce_Core_Review`；Translucent Default Lit、Mesh | EmissiveStrength=1.8、OpacityScale=0.78、Roughness=0.14、Metallic=0、Specular=0.5 | 真实晶面、内部厚度、浅冰蓝色；高档优先Surface ForwardShading验证，记录其真实成本。 |
| `MI_DBA_PalmIce_Petal_Review`；共享IceCrystal母材质 | EmissiveStrength=0.6、OpacityScale=0.52、Roughness=0.2、EdgeGlowStrength=0.7、ThicknessStrength=0.6 | 单层冰片透明而清晰，六片层叠仍可读；实际折射首轮克制，IOR模式起点1.31，仅High/Epic启用后实测。 |
| `MI_DBA_PalmIce_Mist_Review`；AlphaComposite Unlit | EmissiveStrength=0.25、OpacityScale=0.22、DepthFadeCm=3、FlipbookFPS=12 | 冷雾采用明度稳定的浅蓝白色，避免Additive在亮背景消失；不得把整个晶体也套冷雾材质。 |
| `MI_DBA_PalmFire_Ripple_Review`／`MI_DBA_PalmIce_Ripple_Review`；共享扰动母材质 | 火：强度归一化0.02、流速0.3；冰：0.008、0.12；强度范围0—0.05 | 新强度为作者接口，由审核材质映射原生折射输入，不能把它当IOR。复用原热扰动母材质时，先读回真实参数名称及响应曲线再确定实例值。 |

AlphaComposite合同：输入纹理为直通Alpha；最终 `A = saturate(TextureAlpha × Density × ParticleAlpha × Envelope × EdgeMask)`；最终RGB先算颜色与Energy，再**仅预乘一次最终A**送入Emissive，A送Opacity。禁止纹理已预乘后再次乘A。Additive核心是独立合同，不照搬这一输出。

冰晶Alpha必须先在黑／灰／白背景隔离核对；沿厚度生成约0.45—0.85的主体可见区，再应用一次阶段包络。DepthFade仅用于冷雾交界，不能用大DepthFade抹掉贴掌晶核。Fresnel只增强边缘，不把中心抠空。

空气扰动限制在每掌约半径16cm、高32cm的局部范围内，使用单一有界Mesh；不读取自定义全屏SceneColor、不重复叠多层折射。关闭扰动后主体外观必须完整。

## 6. Niagara系统结构与首轮参数

**兼容方案：每种元素一份System资产，按现有左右手附着逻辑各播放一个组件。** 因而一次双掌施法是**2个真实System、8个Emitter**，不能将“一份资产”写成“一个运行System”。本轮先避免重构已经存在的取消/服务器时序链；后续03/04合成时，若准备统一双掌动态参数，再在既有执行器内评估单组件方案，不建立平行播放器。

每个System四个CPU发射器，Local Space，Determinism=true，审核种子31026；不启用粒子碰撞、事件发送器或实时流体。少量Mesh优先CPU起步，是否改GPU须根据实测。

### 火系（表内计数为每只手）

| 发射器 | 渲染及数量上限 | 运动／参数 |
|---|---|---|
| `E_FireCore` | Mesh，1个 | 直径4cm；上下缓动幅度0.3cm、频率2Hz；独立小白金核心。 |
| `E_FireCurlBody` | Mesh，3个 | 三种卷片错位，方位角基准0/120/240°并加入固定种子±12°偏移；缓转25°/s，左右手镜像；高度和宽度受单掌总包络限制。 |
| `E_FireDetails` | Sprite，最多8个 | 4个较大末梢+最多4个小火星，共用审阅过的Atlas/材质；末梢宽3—6cm、高6—12cm，寿命0.15—0.28s；火星0.15—0.35cm、上升20—45cm/s。 |
| `E_FireAirRipple` | Mesh，最多1个 | 局部包络、软边衰减，Low不生成。 |

High每掌最多13粒子、双掌最多26；粒子数不是draw call、GPU成本或总体性能承诺。

### 冰系（表内计数为每只手）

| 发射器 | 渲染及数量上限 | 运动／参数 |
|---|---|---|
| `E_IceCore` | Mesh，1个 | 直径6cm，悬浮幅度0.25cm、频率1.3Hz，缓转12°/s。 |
| `E_IcePetals` | Mesh，6个 | 方位角基准每60°一片、固定种子±8°变化，高度12—19cm、宽3—6cm；根部半径4—7cm，中心外倾10—25°；整体轻转8°/s，不能像风扇。 |
| `E_IceMistMotes` | Sprite，最多10个 | 6团雾+最多4粒冰屑，使用一份Atlas与逐粒子类型区分，避免多开Emitter；雾片6—11cm、寿命0.18—0.35s、径向4—8cm/s、向下2—4cm/s；冰屑0.2—0.6cm。 |
| `E_IceAirRipple` | Mesh，最多1个 | 强度小于火系，Low不生成。 |

High每掌最多18粒子、双掌最多36。若雾与冰屑无法共享合适的渲染合同，首轮舍去可选冰屑，不为它增加第五发射器。

### 时序、附着与参数合同

| 参数 | 类型／默认 | 来源及用途 |
|---|---|---|
| `User.CastDurationSec` | Float，0.5秒 | 保留既有接口，正式播放消费服务器确认时长，不由VFX改写。 |
| `User.ElapsedAtStartSec` | Float，0 | 保留既有迟到同步接口；必须夹紧，过期请求不播放完整起手。 |
| `User.HandSideSign` | Float，左-1／右+1 | 保留既有左右旋向接口。 |
| `User.EffectScale` | Float，1，审核范围0.8—1.2 | 新审核参数，只改变艺术尺寸，不改变范围判定。 |
| `User.EffectIntensity` | Float，1，审核范围0.5—1.5 | 新审核参数，Energy乘数。 |
| `User.AirRippleScale` | Float，1 | 新审核参数，强度乘数；0关闭扰动。 |

质量档通过原生Niagara Effect Type/Scalability及审核预设控制，不能误认为旧系统已有 `User.QualityLevel`。未来若需用户整数参数，先补齐实际类型及提供者桥接，不把Float字典当Int接口使用。

每粒子保存固定随机值，不逐帧重随机。阶段进度 `u=clamp((SystemAge+ElapsedAtStartSec)/CastDurationSec,0,1)`；纹理流动使用真实秒，避免短施法把整套动画极速播完。

- u=0—0.2：掌心核心及主体渐显，对应默认0—0.10秒。
- u=0.2—0.8：完整造型与持续流动，对应0.10—0.40秒。
- u=0.8—1：停止装饰生成并收束，对应0.40—0.50秒；到服务器截止点完成退出。取消、死亡、换化身、World退出按现有请求身份清理。
- 不增加0.5秒以外的权威蓄力；美术循环查看可以由审核工具重复播放，但不得冒充单次实战时长。
- 每掌以现有AttachComponent为位置与旋转源；掌面轴和艺术偏置按角色表现配置/Socket确定，不写死某一英雄或某个世界坐标。
- 初始组件局部Fixed Bounds取每轴±60cm，覆盖手骨旋转和最大EffectScale；测量后收紧。测试包括极端手势、模型缩放、快速转身、WPO最大幅度。Bounds跟随附着组件，不使用固定世界包围盒。

## 7. 四档质量与性能验证

| 档位 | 火系：核心／主体／装饰／扰动（每手） | 冰系：核心／冰瓣／装饰／扰动（每手） |
|---|---|---|
| Epic | 1／3／12／1 | 1／8／14／1 |
| High | 1／3／8／1 | 1／6／10／1 |
| Medium | 1／3／4／1，扰动强度减半 | 1／6／6／1，晶体折射关闭、空气扰动减半 |
| Low | 1／3／0／0 | 1／6／3团雾／0，晶体折射关闭 |

Low仍保留主体尺寸、双手位置、元素轮廓和冰系关键冷雾。采用真实Spawn数量/渲染器变体降级，不能仅把Alpha设零并宣称省了模拟成本；禁用的发射器是否仍存在固定开销须通过原生分析确认。Mip、Mesh LOD和材质开关分别记录。

使用已有CombatCast类别及有效审核Effect Type；当前V3 Effect Type为null，复制后须补充绑定与实际四档响应验证。不能把竞技场预算自动应用于新手村。

测量矩阵：目标机器、1920×1080、锁定曝光、VSync与帧率上限状态留档；四档分别测0人、1人双掌、10人双掌同时施法，冷/热启动分开。记录真实System/Emitter/粒子数、Niagara GT/RT、GPU相关pass、帧P95/P99、透明复杂度/Quad Overdraw、材质指令与纹理驻留。多次相同机位有/无效果对照，不能把Python调用耗时当GPU耗时。

首轮掌心10人并发的优化目标可设为新增GPU P95≤0.6ms、Niagara GT P95≤0.2ms，**仅是待测候选，不是项目已批准预算或达标声明**；正式发布仍执行项目整场战斗预算和10分钟稳定/取消/换World门禁。无法获得独占GPU数据时明确列为未测。

## 8. 按顺序执行的任务与验收

### P0：建立真实基线并定位冰掌不可见原因

**文件：** 读取原V3系统及材质、现有 `CaptureCastFlightV3InspectionUE58.py`；创建 `AuditPalmRedesignReviewUE58.py` 及拥有者/哈希清单。

- [ ] 保存原资源身份、引用、磁盘哈希与现有未提交状态；检查编辑器未保存资源，保护人工编辑。
- [ ] 在独立审核环境逐层测试冰晶与冷雾：纯白不透明替代材质→正确纹理→正确Alpha→粒子Color→原始材质，定位首次不可见步骤；使用实际手骨朝向与黑/灰/白背景。
- [ ] 验证左/右锚点、0.5秒时序、迟到同步和结束清理，记录具体失败点。不得以脚本无异常代替引擎实际绘制。

**退出条件：** 确切可见性问题或仍未解释的环节有原生证据；原资源不变；审核地图身份明确。

### P1：先完成单掌几何与纹理

**文件：** SourceArt、Textures、Meshes中的上述资源；材质初值预设归同一SourceArt清单。

- [ ] 制作火焰三维卷片、冰核及冰瓣；烘焙连续纹理与厚度/法线，记录帧序、单位、轴向、许可。
- [ ] 通过Monolith导入，读回纹理尺寸/sRGB/压缩/Mip和网格三角数/UV/法线/Bounds。
- [ ] 火、冰分别在同一只手上做无装饰静态近景，正面/侧面/背面验证体积，缩到实际游戏尺寸确认细节不碎。

**退出条件：** 主体轮廓和厚度接近母版；未依赖火星、冷雾或Bloom遮掩几何缺陷。

### P2：建立材质及实例

**文件：** Materials，增量实现 `AuthorPalmRedesignReviewUE58.py`。

- [ ] 优先验证原母材质可复用部分；只有Mesh主体/透明合同不满足才创建独立审核母材质。
- [ ] 明确直通/预乘Alpha合同、颜色空间、Mesh/Sprite Usage、Atlas采样和动态参数通道。
- [ ] 逐材质编译并读回所有实际参数；灰/白背景检查冰掌可读性、火焰高亮面积、贴掌深度交界。
- [ ] 保存材质和实例，重新加载核对引用与参数。

**退出条件：** 火焰有金橙厚薄层次；冰晶透明可辨、无石块感；没有编译失败或默认棋盘材质。

### P3：制作两套Niagara审核系统

**文件：** Niagara及作者工具；只改新审核目录。

- [ ] 从真实有效模板建立每类四发射器，绑定材质/网格/参数和Effect Type，设固定种子及Bounds。
- [ ] 使用引擎完整AddEmitter路径，必要时重建系统图；不依赖已发现会遗漏系统图连接的简单DuplicateEmitter流程。
- [ ] 编译后必须运行，逐层记录SimCache数量、Renderer可见性与世界位置；0编译错误不作为“全部层已运行”的证明。
- [ ] 检查0.10/0.25/0.40/0.50秒关键帧和取消事件；纹理材质时间可暂停、重放，避免全局Time导致不同帧不一致。

**退出条件：** 各层真实运行、两个手掌方向正确、末帧无残留、时间和尺寸符合合同。

### P4：保存重载及真实动画审核

**文件：** Maps、`CapturePalmRedesignReviewUE58.py`；证据归 `Saved/Validation/PalmRedesignReview/<run-id>/`。

- [ ] 保存所有拥有者清单内审核包，独立卸载/重载；保护未拥有的脏包。可以安全重启时再做冷加载，不能擅自丢弃用户修改。
- [ ] 母版同类机位与手姿：固定曝光、焦距、角色比例、灯光；输出1920×1080整体与单掌近景，不后期增强。
- [ ] 真实施法动画机位：审核实际0.5秒动作、跑动/转身/换角色与取消；不得只在冻结掌心朝上时通过。
- [ ] 提供每类实时原速视频、单独明确标注的慢放视频、正侧背转台；保留原始帧和录制时间。

**退出条件：** 保存重载参数不丢、动态近景不穿帮、镜头与证据可重复。

### P5：性能及生命周期

**文件：** 审核配置和Audit工具；必要时复用现有 `DivineBeastsDevelopmentCastingLifecycleTests.cpp` 的相关测试，不扩张无关测试集。

- [ ] 四档和十人并发实测，验证实际少生成装饰、核心不被误裁及Bounds有效。
- [ ] 取消/死亡/换化身/退出世界/迟到回调后不存在遗留组件、请求或材质时间源。
- [ ] 输出透明Overdraw、GT/RT/GPU数据及未测项，热点先削减装饰、屏幕覆盖与材质采样，再评估系统合并。

**退出条件：** 性能有真实记录；未达标项保持失败状态，不宣称可投产。

### P6：人工视觉审核与后续接入边界

- [ ] 交付两份原生System、材质/纹理/网格清单、参数表、编译/保存/重载证据、同机位PNG与动态视频、性能报告。
- [ ] 对照轮廓、核心大小、厚薄层次、颜色、掌心附着、动态流动、空气扰动逐项列出通过/未通过。
- [ ] 停在人工视觉审核；未经下一次明确确认不替换正式Catalog映射、不提交主分支、不扩大到飞行/命中/爆炸。
- [ ] 获批后另执行正式调用方接入与Cook/Stage；启动联网客户端前执行Village/MainArena战斗版本一致性门禁。服务器纯表现资源剥离需真实容器证据。

## 9. 必须覆盖的五类失败输入

1. 亮背景/夜间/不同曝光导致冰掌消失或火焰过曝：P0、P2、P4分别留下原生对照。
2. 不同骨架轴向、真实动作翻掌和角色缩放导致错位/穿手：P1/P3/P4覆盖，不硬编码单一英雄。
3. 透明叠层、背面、Mip串帧、TSR拖影或快速运动导致轮廓破碎：P1/P2/P4覆盖正侧背和运动视频。
4. 迟到、重复、取消、死亡、换World导致重播或残留：P3/P5核对请求代次与截止时间。
5. Low或并发剔除丢失主体、关闭扰动仍在模拟、Fixed Bounds裁掉火尖：P3/P5核对真实计数、边界与画面。

## 10. 依据与完成状态

Epic官方参考：[Niagara性能与伸缩](https://dev.epicgames.com/documentation/unreal-engine/scalability-and-best-practices-for-niagara)、[Niagara Renderer参考](https://dev.epicgames.com/documentation/unreal-engine/render-module-reference-for-niagara-effects-in-unreal-engine?lang=en-US)、[材质属性](https://dev.epicgames.com/documentation/en-us/unreal-engine/unreal-engine-material-properties)、[局部折射PNO](https://dev.epicgames.com/documentation/en-us/unreal-engine/refraction-using-pixel-normal-offset-in-unreal-engine)。具体实现以项目UE5.8原生反射、编译及实际截图为准。

计划制定时仅完成只读核对。2026-10-26后续已制作29份原生审核资产，并完成运行、四档数量、合成并发及保存冷重载；实际参数调整、未通过的视觉项和未执行的正式接入门禁见[原生审核实施记录](FireIcePalmsNativeReview_20261026.md)。本计划中的未勾选条目不能整体推断为已经通过。三层规则、唯一VFX链、空气波动及颜色规范继续以AGENTS.md和正式架构文档为准。
