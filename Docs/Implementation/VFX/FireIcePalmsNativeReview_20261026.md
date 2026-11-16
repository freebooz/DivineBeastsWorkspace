# SSLM 火系／冰系掌心原生审核实施记录

日期：2026-10-26。状态：**两套原生审核资源已制作、运行、保存并冷重载；等待人工视觉审核，尚未达到概念图一致性验收。**

本轮落实《FireIcePalmsConceptImplementationPlan_20261026》中的掌心审核资产。没有替换正式技能、Catalog、服务器时序、伤害或弹道，没有提交主分支。旧火冰V3及P1草稿保留。全过程使用本地工具和直接Monolith，没有调用HWebCodex。

## 1. 工程及架构边界

- 实际工程：`E:/work/2026/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject`，UE `5.8.0-0+UE5`，Win64 D3D12。
- 沿用已注册内容插件 `DBAPresentationPack_Core`，真实挂载目录为 `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign`。下文记作 **B**，例如 `B/Niagara/...` 应展开为此完整挂载目录。
- 资产与审核配置属于 DivineBeasts 项目表现层；平台 GamePlatformVFX、GamePlatformPresentation 和可选 MobaPresentation 职责不变，没有新增播放器、池、预算服务或服务器表现依赖。
- 审核脚本直接创建Niagara组件，仅用于离线审核，不构成正式Gameplay接入。正式接入仍应由已有服务器确认事实、平台唯一执行链及既有取消/代次合同驱动。
- 当前分支为 `main`，保留已有未提交文件，本轮没有提交或切换分支。19份受保护原始CastFlight资源SHA256全部保持不变。

## 2. 可直接打开的原生资源

| 资源 | 完整路径 |
| --- | --- |
| 火系掌心 | `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign/Niagara/NS_DBA_FirePalmCast_V3_ConceptReview` |
| 冰系掌心 | `/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V3/PalmRedesign/Niagara/NS_DBA_IcePalmCast_V3_ConceptReview` |
| 共享CPU运动模块 | `B/Niagara/NM_DBA_PalmConceptMotion_Review` |
| 独立审核地图 | `B/Maps/L_DBA_PalmRedesign_Review` |
| 掌心朝上审核姿态 | `B/Animations/AS_DBA_PalmConceptPose_Review` |

共29份原生资产：7网格、4纹理、4特效母材质、8实例、1审核地面材质、2系统、1运动模块、1姿态、1地图。真实资产由UE/Monolith创建，源文件没有改扩展名冒充资产。

原生读回清单：`Saved/Validation/PalmRedesignReview20261026/SavedAssetReadback.json`；系统编译和结构读回：`FinalNativeReadback.json`。其中早期渲染器读取把冷雾名称误写为 `E_IceMist`，随后已实际核实正确名称为 `E_IceMistMotes`，未创建第二个冷雾发射器。

## 3. 网格与纹理

网格均以厘米为单位。火壳是闭合截面沿曲轴收束的三维表面；冰瓣为不规则六棱弯晶，采用真实切面法线。以下三角数为导入源网格记录，不作为GPU实际提交三角数测量。

| 资源（位于B/Meshes） | 数量／源三角数 | 用途 |
| --- | --- | --- |
| `SM_DBA_PalmFireCurl_A/B/C` | 3 × 496 | 三种不等高卷焰壳 |
| `SM_DBA_PalmIcePetal_A/B/C` | 3 × 92 | 六片冰瓣复用三种模型 |
| `SM_DBA_PalmIceCore` | 80 | 切面晶核 |

| 纹理 | 规格与色彩空间 | 使用情况 |
| --- | --- | --- |
| 原有 `/DBAPresentationPack_Core/VFX/Library/Game/FireMage/Textures/T_PY_FlameFlipbook` | 1254×1254，4×4帧 | 原稿不变；两帧插值与UV扰动驱动火焰 |
| `B/Textures/T_DBA_PalmFire_Flow` | 512×512，线性Masks | 已导入的候选流场，当前材质未引用，不计作运行成本 |
| `B/Textures/T_DBA_PalmIce_Surface` | 1024×1024，线性Masks | R霜纹、G厚度、B粗糙候选、A裂纹；当前R/G/A参与材质，粗糙度以实例标量控制 |
| `B/Textures/T_DBA_PalmIce_Normal` | 1024×1024，线性Normalmap | 微法线 |
| `B/Textures/T_DBA_PalmMist_Flipbook` | 1024×1024，4×4帧，线性Masks | 确定性解析流场烘焙冷雾，非物理流体模拟 |

源文件与生成清单位于内容插件 `SourceArt/VFX/PalmRedesign`。未使用运行时Niagara Fluids。新增OBJ/PNG可重新导入；原FireMage素材继续使用项目既有来源。

## 4. 材质与真实参数

以下均在 `B/Materials`，只制作本轮独立审核材质，不修改原公共母材质。

| 母材质 | 模型及关键实现 | 实例与最终主要值 |
| --- | --- | --- |
| `M_DBA_PalmFireCurl_Review` | Unlit、AlphaComposite；图集两帧插值、动态流动、预乘Alpha、卷壳WPO、核心视角软边 | `MI_DBA_PalmFire_Core_Review`：Energy 9、Density .85、CoreMode 1；`...Fire_Curl...`：3.4/.95/0；`...Fire_Detail...`：2/.55/0；FlowSpeed .35 |
| `M_DBA_PalmIceCrystal_Review` | Translucent、DefaultLit、Surface Lighting Volume；厚度/切面法线/环境立方图反射近似 | `MI_DBA_PalmIce_Core_Review`：EmissiveStrength 2.3、OpacityScale .85、Roughness .14；`...Ice_Petal...`：2.8/.8/.16 |
| `M_DBA_PalmMist_Review` | Unlit、AlphaComposite；4×4图集插值及单次Alpha包络 | `MI_DBA_PalmIce_Mist_Review`：OpacityScale .72、EmissiveStrength 2.8 |
| `M_DBA_PalmAirRipple_Review` | 本地有界球Mesh，零着色、IOR折射；显式启用RM_INDEX_OF_REFRACTION | `MI_DBA_PalmFire_Ripple_Review`：P_Intensity .008；`MI_DBA_PalmIce_Ripple_Review`：.003；两者P_Opacity 0 |

冰晶环境反射引用 `/Engine/MapTemplates/Sky/DaylightAmbientCubemap`，不是物理多次内部折射。其RGB正确接到反射向量的 `UVs` 接口。空气扰动的数值是材质IOR偏移幅度，不是屏幕像素位移。

最终材质原生编译成功；火焰约233条PS指令、冰晶约1273条PS指令，后者仍较重。指令数只是编译统计，不能等同实际GPU毫秒。

## 5. Niagara结构及参数

两套系统各4个CPU发射器，局部空间、一次播放；每个发射器确定性种子31026，系统级determinism开关没有因此宣称已启用。FixedBounds为各轴±60cm。

Effect Type复用 `/DBAPresentationPack_Core/VFX/CommonSkillVFX/Review/V2/AttackSlash/Scalability/NET_DBA_Review_CombatCast`。正式运行伸缩尚未接入；审核脚本显式控制数量并关闭组件自动伸缩，以验证指定数量。

| 系统 | 发射器 | High单掌数量 | Renderer／实例 |
| --- | --- | ---: | --- |
| Fire | E_FireCore | 1 | Sphere Mesh / Fire_Core |
| Fire | E_FireCurlBody | 3 | 三种FireCurl Mesh / Fire_Curl |
| Fire | E_FireDetails | 8 | Sprite / Fire_Detail |
| Fire | E_FireAirRipple | 1 | Sphere Mesh / Fire_Ripple |
| Ice | E_IceCore | 1 | IceCore Mesh / Ice_Core |
| Ice | E_IcePetals | 6 | 三种IcePetal Mesh / Ice_Petal |
| Ice | E_IceMistMotes | 10 | Sprite / Ice_Mist |
| Ice | E_IceAirRipple | 1 | Sphere Mesh / Ice_Ripple |

上表Sphere为 `/Engine/BasicShapes/Sphere`；材质实例全名与上节相对应。

统一User参数：`CastDurationSec=.5`、`ElapsedAtStartSec=0`、`HandSideSign=±1`、`EffectScale=1`、`EffectIntensity=1`、`AirRippleScale=1`；整型 `CoreCount=1`、`BodyCount=3/6`、`DetailCount=8/10`、`RippleCount=1`。输入时间来自Niagara Emitter.Age与迟到偏移，不使用材质全局Time。

动态材质通道：R=阶段秒数，G=确定性粒子种子，B=逻辑层，A=生灭包络。20%起始渐显、20%末端渐隐；Lifetime取剩余窗口。左、右手符号影响旋转方向。Niagara内部没有世界固定坐标或特定英雄骨骼偏移。

审核Rig从真实hand/middle/index/pinky骨骼求掌心与掌面法线。骨骼命名仅属于Manny审核适配；尚未宣称跨英雄验证完成。实际原施法动画长约.7秒，视频映射至既定.5秒审核窗口，没有修改服务器时序。

## 6. 质量预设及性能

冷重载后实际SimCache验证：`Performance/20261026T152642006728Z/Result.json`。每项同时验证左右两个组件，核心均保留。

| 档位 | 火（核/壳/装饰/扰动） | 冰（核/瓣/雾/扰动） |
| --- | --- | --- |
| Epic | 1/3/12/1 | 1/6/14/1 |
| High | 1/3/8/1 | 1/6/10/1 |
| Medium | 1/3/4/0 | 1/6/5/0 |
| Low | 1/3/2/0 | 1/6/2/0 |

这是审核预设的明确调整：Medium关闭空气扰动，Low仍保留少量装饰，Epic冰瓣仍为6片。不同于计划候选值，不是已接入游戏设置菜单的自动画质合同。

**可见并发实测**：`Performance/20261026T151120892388Z`。RTX4090、i9-14900K、D3D12、驱动610.88；1920×1080 SceneCapture，固定43°视场，明确相机坐标与look-at，20个活跃系统逐一读取数量且有同期原生截图。循环为.5秒施法+.15秒间隔；每项300帧，统计包含间隔，非持续峰值负载。是十组双掌的合成审核，不是十个联网角色。

| High场景 | GPU整帧平均/P95(ms) | 全透明Pass平均/P95(ms) | 全折射Pass平均/P95(ms) |
| --- | --- | --- | --- |
| 无审核特效基线 | 3.1812 / 3.7005 | .0027 / .0036 | 无统计 |
| 火，2系统 | 3.4119 / 4.0304 | .1083 / .5693 | .0144 / .0284 |
| 火，20系统 | 3.3110 / 3.5011 | .0985 / .1381 | .0141 / .0385 |
| 冰，2系统 | 3.2176 / 3.5019 | .0595 / .0842 | .0121 / .0277 |
| 冰，20系统 | 3.3243 / 3.5314 | .1109 / .1618 | .0134 / .0343 |

整帧包含编辑器和SceneCapture，其GT/RT数据见Summary.json。2与20系统的GPU整帧并不单调；噪声、覆盖面积、相位和编辑器工作都有影响，不能据此宣称20系统更快，不能把基线差值当孤立VFX成本。帧时间约8.33ms存在编辑器节拍影响，不宣称120FPS达标。未测量独立Niagara GPU成本、打包客户端、十人联网、10分钟稳定性或正式战斗预算。

早期Performance目录中相机朝向未明确的CSV和未开启IOR的结果均作废为性能结论；保留诊断记录，不与以上有效采样混用。

**过绘门禁未通过。** 原生Shader Complexity与Quad Overdraw已取得，见 `Native/20261026T153529426743Z`：近景火焰壳、装饰片和核心叠加形成粉白热点；冰晶中心与冷雾区域达到Quad Overdraw色标最高档10。热图是相机相关的定性诊断，不把该色标当成精确逐像素透明层数，也不由此计算GPU毫秒。后续优化应优先减少闭合晶体背面渲染、装饰覆盖与复杂冰材质采样；当前尚未完成这种优化后的视觉回归及重测，故不宣称预算达标。

- [火焰Shader Complexity](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T153529426743Z/Fire_ShaderComplexity.png)
- [冰晶Quad Overdraw](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T153529426743Z/Ice_QuadOverdraw.png)

## 7. 编译、保存、重载及生命周期

- 两份系统原生编译均为0错误/0警告；实际运行读取到全部四层粒子，不以编译结果代替运行。
- 保存后退出本轮拥有的编辑器PID36800，无其他脏包；新进程PID49056从保存的审核地图启动。29个磁盘包SHA一致，29份资产重新加载成功。证据：`ColdReload.before.json`、`ColdReload.result.json`、`EditorFinalColdReloadStart.json`。
- 调试热图采集结束后，再次冻结最终交付文件并由PID65904独立打开：29资产加载、29哈希一致、19原稿不变；见 `FinalFreeze.before.json`、`FinalFreeze.reload.json`、`EditorDeliveryReloadStart.json`。此后没有再写审核资产。冷重载后的两套系统编译诊断仍为0错误/0警告，见 `ColdReload.diagnostics.json`。
- 生命周期探针 `Lifecycle/20261026T152249484137Z/Result.json`：两类共18项，覆盖.10/.25/.40/.50/.60秒、迟到.2/.6秒、停止后排空。全程保持可见，不使用隐藏组件伪造消失。
- **边界观察**：DesiredAge=.5时固定步进出生偏移仍有粒子；.6时四层均为0。末端透明包络已衰减，但不能称.500秒恰好释放全部粒子。过期迟到.6秒输入无有效粒子。
- 取消探针采用组件Deactivate后恢复正常时间推进，等待.65秒验证排空；不是即时Kill，也不代表正式GAS取消、死亡、换化身、换World、重复通知或请求代次验收通过。
- 审核结束销毁拥有的组件、退出自己的PIE。此操作计数为0不等于平台正式池化与跨世界生命周期已全量验证。

## 8. 原生截图与视频

截图源目录：`Saved/Validation/PalmRedesignReview20261026/Native/20261026T150944611028Z`，每类Wide/Close/Side/Back/Cast/RippleOff/Low，1920×1080、固定曝光、35°视场、Bloom .15，无后期增强。是专用审核地图相机，不冒充正式游戏相机。

![火掌原生整体](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T150944611028Z/Fire_Wide.png)

![冰掌原生整体](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T150944611028Z/Ice_Wide.png)

视频源目录：`Native/20261026T151254891961Z`。每类整体、近景、真实施法动画三个机位，各两次施法。原生固定1/60秒模拟步进，共504张未调色PNG；封装为60fps视频，不代表实时采集运行达到60FPS。

- [火掌原速视频](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T151254891961Z/Fire_PalmReview_60fps.mp4)，252帧，4.2秒。
- [冰掌原速视频](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T151254891961Z/Ice_PalmReview_60fps.mp4)，252帧，4.2秒。
- [火掌半速审核](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T151254891961Z/Fire_PalmReview_HalfSpeed.mp4)、[冰掌半速审核](../../../Saved/Validation/PalmRedesignReview20261026/Native/20261026T151254891961Z/Ice_PalmReview_HalfSpeed.mp4)，明确为半速，每份约8.38秒。

视频已用ffprobe实际解码计数确认尺寸和帧数。原PNG和编码警告均保留；PNG附加块警告没有造成解码失败，不删除警告来伪造无告警。

## 9. 视觉审核结论与未完成项

| 对照项 | 结果 |
| --- | --- |
| 双掌独立、掌心锚点、火金橙／冰蓝白配色 | 原生可见，待人工判断是否符合设计 |
| 火焰立体厚度、非十字片主体 | 已改有截面的卷焰壳；仍有明显卷带与空心轮廓，**未达到概念图的自然丰满翻卷** |
| 火核及能量流 | 小核心和连续图集流动可见；核心仍偏规则，需人工审定 |
| 冰晶厚度与反射 | 不规则切面与反射层可见；晶瓣排列、亮边仍比概念图规整，未达到物理晶体内折射质感 |
| 冷雾及冰屑 | 冷雾可见；概念图飞散冰屑尚未独立实现，不把雾称为冰屑 |
| 空气扰动 | High/Epic有局部IOR折射，Medium/Low不生成；已记录原生开关对照和Distortion Pass，强弱待人工审核 |
| 风格/亮背景/跨英雄/真实游戏尺寸 | 审核环境与概念环境不同；没有完成昼夜、天气、跨英雄、游戏相机全矩阵 |
| 正式画质适配与技能接入 | 未执行，须在人工确认之后接入已有链路 |
| Cook/Stage、网络与服务器剥离 | 本轮未执行；未启动正式双客户端，不宣称联机或发布验收 |

**当前交付是可编辑、可重放的原生审核版本。视觉一致性、正式生命周期及发布性能门禁仍未通过，不能标为Production Ready。** 本轮停止于人工视觉审核，不继续飞行、命中或爆炸。

## 10. 作者工具与问题记录

工具目录：`Tools/Unreal/VFX/DevelopmentMagic`。

- `BuildPalmRedesignSources.py`：确定性网格和线性纹理源生成。
- `AuthorPalmRedesignReviewUE58.py`、`PalmRedesignMotion.ush`：Monolith内导入、材质、拥有者保护与CPU运动；原始包哈希每阶段核对。
- `AuthorPalmReviewPoseUE58.py`：独立审核姿态，保留原施法动画。
- `CapturePalmRedesignDualReviewUE58.py`：双掌机位、原生SimCache、固定步进PNG；保存前设置真实相机朝向，视频全程保持组件可见。
- `AuditPalmRedesignUE58.py`、`AuditPalmLifecycleUE58.py`：质量/并发CSV与生命周期探针。
- `SummarizePalmReviewEvidence.py`、`EncodePalmReviewVideos.py`：只归纳原生数据与封装原生帧。
- `CapturePalmOverdrawReviewUE58.py`：原生调试视图，最终图已确认有Shader Complexity/Quad Overdraw热图和引擎色标；早期参数名和SimCache前置条件失败记录保留。
- 早期单掌Probe/Review工具保留用于诊断，不作为最终双掌交付入口。

已修正的真实问题：SpawnBurst早于EmitterState造成双倍发射；移动模块后参数链接丢失需重新绑定；DynamicParameter必须接RGBA；TextureCube必须连接UVs；原扰动母材质RM_NONE导致没有折射；背景球投影使角色变黑；保存地图前相机未转向；视频隐藏不能作为自然结束证据。此前NewBlankMap遭受Python持有World引用导致的GC断言已记录，后续只操作拥有的审核World并通过独立编辑器重启完成最终加载验证。

中文说明覆盖本轮作者、采集、审核脚本及参数/权威边界；未开展全仓旧脚本注释整改。所有失败尝试留在Saved/Validation，不冒充通过项。
