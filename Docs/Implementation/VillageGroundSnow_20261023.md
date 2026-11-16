# 新手村地面积雪、停雪与融雪实施记录

本次由用户明确批准2026-10-23审查方案。现有链路内增量实施，不新建插件、天气播放器、复制协议或资源管理器；人工视觉签收与GPU实测必须另列真实结果。

**2026-10-24当前修订：** 按“雪不够厚、不够蓬松、雪花倒飞”反馈，将松雪最大置换调为10厘米，形成约5—10厘米圆润雪层；脚印压实深度独立保持不超过5厘米。近/远景改为上空生成、世界空间负Z速度，3个发射器数量不变。新证据使用`Saved/Validation/VillageSnowFluffy20261024`。下文2026-10-23的5厘米与旧视频记录为历史基线，不能充当本次运行验收。

## 三层归属

- 平台GamePlatformWeather保留唯一服务器快照、服务器时间插值和客户端Surface/VFX通知。
- 平台GamePlatformSurface拥有`Scripts/SnowCoverageOpacity.py`唯一制作配方及真实`MF_GP_SnowCoverageOpacity`函数。覆盖形状与实覆强度分开：实覆强度为`saturate(SnowAmount/FullCoverAmount)`，完整覆盖阈值默认0.55。坡度/海拔继续由接收表面控制。
- 共享Village中性Landscape桥由同一平台配方展开少量纯数学节点，保持既有项目参数合同；不硬引用ClientOnly Surface函数或纹理，避免Dedicated Server Cook引入纯客户端资源。这是编辑器生成的适配图，不持有另一份天气状态，也不手工维护第二份覆盖算法。
- 平台GamePlatformVFX保留唯一降雪执行链。详细雪系统保留3个Emitter，近景CPU、远景和贴地漂雪GPU；绑定原生`NET_GP_WeatherAmbient`及固定边界。Low/Medium/High/Epic与Cinematic发射数量比例为25%/50%/75%/100%；系统数量上限为本地天气装饰保护配置，不是GPU性能承诺。
- 项目DBAClient保留既有世界期Data租约和Landscape MID桥。项目DBAWorlds仅在显式非Shipping审核开关下提供服务器天气演示数据；MobaCommon无独立职责，不新增模块。

## 地面表现与保护边界

仅修改现有Landscape母材质，不改高度、碰撞、导航、地图Actor、PlayerStart或用户手工地形。雪面加入受控法线强度及归一化、低对比基础色细节，并在积雪处解除草地AO暗斑。零雪量恢复原草地，湿润度继续改变既有底层外观。

早期基线只有薄积雪PBR覆盖，2026-10-23接入最大5厘米的真实Nanite Displacement；2026-10-24按厚度反馈改为最大10厘米，并保留有界屏幕衰减。地图源高度与原Actor位置保留，不以WPO或视差冒充置换。当前地图无雪苔标记岩石，未擅自增加屋顶、岩石或人为雪堆。

## 四阶段原生审核入口

服务器增加显式`-DBAVillageGroundSnowReviewSchedule`，仅接受Village Dedicated Server、`-unattended`、没有正式天气表和初始预设的世界。两个客户端正常PostLogin后通过既有Weather服务执行有限序列：

| 阶段 | 目标雪量 | 雪强度 | 温度 | 过渡/总阶段秒数 |
| --- | --- | --- | --- | --- |
| 小雪 | 0.23 | 0.35 | -2℃ | 4/30 |
| 大雪 | 0.85 | 0.90 | -8℃ | 18/60 |
| 停雪保留 | 0.85 | 0 | -4℃ | 2/35 |
| 升温融雪 | 0 | 0 | 6℃ | 40秒融化，再保留无雪湿地15秒，湿润0.65 |
| 审核驻留 | 0.85 | 0.90 | -8℃ | 10秒恢复，持续保持大雪积雪供人工检查 |

审核驻留由同一服务器天气权威发出，非客户端强制改材质。四阶段仅执行一次；驻留不循环、不重复融雪，退出世界仍沿用原生命周期清理。用户2026-10-23反馈地面无雪，定位为旧审核序列以融雪完成收尾，本次明确修正此行为。

这使用服务器目标值与时间插值，未冒称按每一片雪碰撞累计厚度的物理模拟。历史`-DBAVillageSnowReviewSchedule`断线重入测试序列保持兼容。

客户端显式`-DBAVillageSnowTraceWeather -DBAVillageGroundSnowCaptureReview`在天气事件与全部MID回读匹配后，每阶段分别请求一次原生截图，包含融化中途和完成图；不轮询UI，不在普通客户端自动截屏。视频必须从两个真实RHI客户端录制，不能将NullRHI辅助客户端当作视觉验证。

## 5厘米雪层脚印实现（用户已明确选择）

目标：保留Displacement默认5世界厘米。真实脚底落地时压实局部积雪，凹陷周边保留柔和雪唇，法线及粗糙度同时响应；新雪回填，融雪后清空。角色移动、碰撞、伤害和地形源高度继续由原权威链拥有。

方案比较：纯Decal成本低但不能形成真实置换凹陷；逐脚印独立Mesh或SceneCapture容易增加组件及整场景渲染成本。本次采用有界双缓冲RG16f高度场，扩展现有Surface世界服务，不增加天气服务、全局播放器或复制纹理。

依次执行：

- [x] 原生图回归先确认缺少压痕支路；新增足底触地门限、重复事件、离地/静止、非法值的原生测试。
- [x] `GamePlatformSurfaceClient/Public/Types/GamePlatformSnowContactTypes.h`提供厘米请求和只读材质绑定合同。既有`IGamePlatformSurfaceService`和世界子系统承接有界队列、Data材质租约、RT批量更新和世界退出清理；实现拆入Private/Snow。每世界两张RG16f，High为1024²/16米窗口，共8MiB纹理数据；这是内存规格，不代表GPU预算通过。
- [x] `Public/Components/GamePlatformSnowFootContactComponent.h`及Private实现从最终骨骼姿态事件读取左右足与移动组件缓存地面，只在接地门限跨越时做确认射线；不新增Actor Tick。项目新手村桥通过Actor生成/退出事件装配组件，并把Surface瞬态高度场绑定至原64个Landscape MID。
- [x] Monolith制作平台Update材质及项目中性雪支路；高低RT使用同一世界空间合同。原雪高度按压实率下降，边缘略抬起，最终归一化高度限制0—1，默认5cm内；法线/粗糙度响应压实。Dedicated Server不加载或Cook纯表现资产。
- [x] 完整Editor/Client/Server增量构建及Cook，核对实际Village/MainArena服务器战斗版本后启动两个RHI客户端，真实准入Village；实际左右脚连续行走触发高度场，小雪/大雪/停雪/融雪和恢复大雪驻留均完成原生日志与截图核对。
- [ ] 人工视觉签收；全部角色换装、跳跃/陡坡/退出重入的完整联机矩阵；四档与多人并发的独立脚印GPU成本预算。不得用本轮有限验证冒充这些验收完成。

性能边界：最多16个接触合批、仅有足迹时进行回填更新、关闭后保留雪主体。窗口外不创建全地图纹理；无CPU读回、无逐脚印Decal或Niagara System。材料加载失败时保留原雪面并给出诊断，不伪造脚印成功。

2026-10-24原生复核：脚印纹理同时进入原材质Displacement、Normal和Roughness，保留原草地/雪覆盖法线混合。RG16f真实GPU读回确认中心压力0.949707、边缘雪唇0.992676、窗口外0；回填后压力0.749512，滚动窗口保留正确位置。该测试只证明材质与高度场算法，不代替真实角色触地及人工视觉验收。High档双RT数据8MiB，默认16米局部窗口、最多16接触合批、5Hz回填；编译指令数不等于实测GPU毫秒。

首次双客户端行走未触发压痕。经Monolith读取实际压缩步态确认Kachujin有75骨，脚骨为LeftFoot/RightFoot而非foot_l/foot_r；左脚踝骨站立约10.73—10.81厘米、步行10.46—32.32厘米、跑步10.55—39.43厘米。平台组件现由项目提供最多8个候选名，在异步模型装配/换装后重新解析并随World清理，不硬编码项目模型。接地迟滞14/18厘米，包含踝骨和引擎约2厘米悬距，随后仍需真实地面射线确认；不使用角色胶囊中心伪造足印。修复前媒体单独保留在本轮MediaRoot/BeforeFootBoneFix。

## 参考与写实打磨（用户后续明确要求）

用户指定Quixel《Realistic Snow in UE4 with Quixel Mixer》32秒附近画面，并提供UE4方向积雪节点文本：颜色、法线、粗糙度、方向混合、Subsurface Color和World Displacement。该文本引用的Snow_Diffuse/Normal/Roughness/Displacement贴图未随附；本轮使用Poly Haven作者Rob Tuytel的CC0 Snow 02配套2K Diffuse、DX Normal、ARM与Displacement，来源及校验记录在平台SourceArt/Snow/PolyHavenSnow02/SourceManifest.json。原扫描RGB保持原样，正确区分颜色sRGB与线性数据。

- 平台Surface制作配方SnowPBRDetail.py展开至既有中性Landscape桥。雪法线强度0.90、原扫描颜色细节1.0、覆盖边缘0.08；向上31度以内完整留雪，31—69度连续减弱，垂直面不挂雪。
- 原生Opaque＋Subsurface模型；覆盖区以配套扫描ORM产生粗糙度/AO，次表面颜色和密度随实际雪覆盖混合，未覆盖区不贡献雪次表面颜色。保留近景BumpOffset辅助细节，在4—12米淡出。
- 以用户最新色板为准，次表面颜色为扫描Albedo×`VillageSnow Subsurface Tint`×实际覆盖。可调默认值采用线性RGB(0.547744, 0.710151, 1.0)，对应屏幕sRGB约#C4DCFF，Alpha为1。不把色板中的线性数值当作sRGB重复转换，不改动基础色和Specular标量。前一张较深蓝色被最新浅蓝色替代。
- 雪面复用配套扫描高度/法线建立两档高密度细节：`Snow Noise Density=12`、`Snow Fine Noise Density=48`、`Snow Noise Strength=0.30`。同时影响高度、法线与小幅粗糙度，纹理Mip和置换屏幕衰减负责远景稳定；这些参数与编译结果仍需由最终真实客户端画面及GPU验证。
- 用户要求真实Displacement参数默认5。UE5.8 Python隐藏MP_Displacement枚举，平台SurfaceEditor补充仅编辑器原生连接接口，由Monolith调用。真实Nanite置换与视差分别验收，不能互相冒充。参数名Displacement Multiplier，默认5，范围0—10世界厘米；Landscape实际100倍缩放通过材质DisplacementScaling 0.1对象单位转换，Center=0，雪层只向外生长。Native Displacement Fade按8—2像素屏幕幅度衰减。
- 真实Nanite地形需保存生成显示数据；操作前备份用户地图，比较原Actor Transform和地图内嵌源高度纹理导出SHA。仅Nanite显示数据允许变化，禁止重新雕刻、导入或覆盖人工地形和PlayerStart。
- 平台Surface的无状态Nanite材质桥通过引擎真实SourceLandscapeComponents映射，将既有MID绑定到各Nanite组件OverrideMaterials；不修改共享StaticMesh、不建另一套天气或动态材质。项目原世界租约扩展至八张纹理，逐MID回读全部八张，取消/退出仍由原链处理。
- 用户要求背景音乐默认不开启：平台SFX支持项目提供首次三路默认音量，项目声明Music=0，Effects/Ambient=1；已保存用户偏好优先。继续事件通知现有执行器与设置页，不增轮询。

写实PBR无置换历史基线Shader：121节点、12采样器、估计21像素纹理采样、148VS/631PS指令。最终含脚印材质为162节点、13采样器、估计26像素纹理采样、148VS/694PS指令；这些是编译统计，不能视为GPU毫秒。最终同版本包的验证范围见下节，旧基线不覆盖后续位移/音乐改动。

- 修改前原生连线回归因大雪透底失败；修改后测试通过，雪量0/0.23/0.5/0.85/1的平均平地混合权重约0/0.197/0.735/0.974/0.993，包含零雪量、单调与25°/50°/垂直面检查。
- 已通过Monolith创建、编译和保存真实材质、函数、Niagara及EffectType；两个GPU Emitter原生编译无错误和警告。
- 最新完整构建、Cook、双客户端阶段同步、原生媒体与GPU结果以本轮`Saved/Validation/VillageSnowDisplacement20261023`证据为准；历史`VillageGroundSnow20261023`仅作基线追溯。

## 2026-10-24最终包与实际运行证据

本节记录机器执行与素材核对结果，人工签收仍为否。地图原嵌入高度与Actor Transform比较保持一致；未改PlayerStart或地形源数据。

| 检查 | 实际结果及证据 |
| --- | --- |
| 完整Editor构建 | `2659c3df-79f9-459c-bfbf-0ccd617e71ce`，Passed |
| 原生测试 | `e47e1265-8645-4133-853a-039a8302e5dc`，3通过/0失败：真实MID、异步Data租约取消/类型失败、接地门限与去重 |
| 完整Client＋Server构建 | `6ac45e43-fe2a-4381-a316-7665d5363879`，Passed |
| Client Cook/Stage | `ca9bf883-a850-428a-a0d4-38f8c94d9bf0`，Passed |
| Server Cook/Stage | `d105e133-1392-4205-b0ec-03c90986d36d`，Passed；纯Surface脚印与雪贴图未进入Server容器 |
| 战斗版本 | Client及实际Village/MainArena运行包SHA256均为`70d372b8d99167d82f2a7f1868884fb9b1f525cf911cbf87d452ab1aef55ff11`，见`RuntimeCompatibility.json`；本轮实际准入验证为Village |
| 双真实D3D12客户端 | PID17892、48064均正常进入Village，无战斗版本/复制校验错误；每端64原Landscape MID及64 Nanite材质槽读回Displacement=5 |
| 实际触地 | 两端均解析LeftFoot/RightFoot，连续真实行走产生压痕；第二端也接收附近另一角色的本地表现接触事件。足印为局部装饰，未新增服务器RT复制 |
| 天气阶段 | 服务器按顺序执行全部5步（4阶段＋大雪驻留），双方各6张原生截图，共12张，见`RuntimeVisualEvidence.json` |

原生截图目录由`Plan.json.MediaRoot`指向本次可视化目录中的`SSLMVillageSnowDisplacement20261023/Screenshots`。其中`VillageSnow_StoppedSnow_PID17892_20261023_165136.png`显示连续足印；`MeltedGround`两张显示融雪后恢复原地表；`HeldHeavySnow`两张证明最后重回大雪积雪。

录屏先前的GDI窗口捕获虽退出0，但抽帧发现D3D12画面陈旧，已在`RejectedWindowVideo.json`明确拒绝，不能作为视觉证据。最终采用DXGI桌面复制，以Sky实读的两个游戏窗口矩形分别记录1920×1128、目标30fps（文件实测平均29.94fps）；无缩放、调色、AI补帧。最终视频与解码核对在`MediaVerification.json`列出。视频包含控制台操作和前后台切换；客户端分辨率为1280×720，操作系统DPI显示尺寸与截图不同。

原生CSV性能采样采用RT首次实际就绪后2秒至采样结束的所有帧，按日志UTC与累计FrameTime对齐，两端偏差约0.07秒；没有删除慢帧。RTX4090、High效果档、双真实客户端、`t.MaxFPS=0`：客户端1的8669帧FrameTime P95/P99为6.76/7.44ms、GPUTime为5.74/6.40ms；客户端2的7300帧为7.42/267.69ms、GPUTime为6.44/237.83ms。采样包含后台客户端限帧、软件录屏与编辑器命令行测试重叠，不能推导独立脚印GPU成本、稳定双端帧率或竞技预算。完整口径见`MeasuredPerformance.json`，生产性能批准为否。
# 2026-10-24 积雪蓬松度与降雪方向修正计划

本轮按用户要求直接执行，沿用既有三层、Surface/Weather/VFX插件及唯一运行链。
原生资产读取确认近/远景未保存Velocity和Box Size覆盖，编译GPU代码采用(0,0,+50)及100cm默认盒。

- [ ] 先运行真实资产方向回归并记录失败，再修复近/远景上空生成、世界空间负Z速度；地面雪粉不再向上升。
- [ ] 真实雪材质改为连续圆润的5—10cm视觉积雪，细噪声保留但不主导轮廓；脚印压陷独立限制5cm。保留原草地、浅蓝SSS、用户地图高度和Actor变换。
- [ ] Monolith编译、保存、重载；方向/图连接回归与原生模拟取样。检查无新增透明覆盖、发射器和纹理采样。
- [ ] 仅烘焙受影响内容，绑定当前构建输入核对Client/Village/MainArena；两个实际客户端截图、视频与人工审核分开报告。
# 2026-10-24 追加执行记录：降雪光雾与小音量音乐

用户追加要求：积雪厚度随雪量变化，阴天降低照度，降雪增加雾；环境音和音乐默认采用小音量并修复音乐静音。

- 已完成的第一轮：10cm最大蓬松雪层、5cm压实脚印、向下雪花；两真实客户端四阶段及最终保持大雪已产生12张原生截图。证据位于 `Saved/Validation/VillageSnowFluffy20261024`，该轮不包含后续光雾修复，不能代替最终验收。
- 根因：WeatherClient只写地表六字段，未消费阴云/雾到场景灯光；本次客户端日志确认Music=0、Ambient=1且Village音乐请求已受理，静音来自旧默认配置。
- 执行：平台WeatherClient私有适配器复用已有有限天气插值，项目Village显式启用。相对捕获的原场景照度渐变；阴天/雨雪渐变雾；晴天、取消和世界退出恢复原值，不保存地图。最大积雪10cm与脚印5cm保持独立。
- 音量裁定：本次明确要求恢复音乐，取代此前首次默认关闭音乐要求；新默认Music=0.15、Ambient=0.25，Effects保持1；已经手动保存的设备偏好优先。
- 验证顺序：真实灯光/雾组件红灯回归→实现及绿灯、音量持久化回归→完整目标/新Cook→实际Village/MainArena版本一致性→双客户端四阶段截图/动态视频和音频输出证据。视觉与听感保留人工审核。
- 编译缓存：先复查此前授权清理的19项历史缓存均已不存在；当前NoSharedPCH增量缓存、内容、地图、当前Stage与审核资料保留。不存在的历史缓存不重复删除，不以完整Clean扩大范围。
- 性能：不新增天气Tick、常驻轮询、天空重捕获或体积雾通道。第一轮雪粒子寿命增加使高强度估计存活粒子约增加36%，不能宣称成本不变；最终记录真实双客户端指标，仍不等同孤立GPU或十人竞技预算验收。
