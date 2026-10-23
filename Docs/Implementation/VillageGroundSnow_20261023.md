# 新手村地面积雪、停雪与融雪实施记录

本次由用户明确批准2026-10-23审查方案。现有链路内增量实施，不新建插件、天气播放器、复制协议或资源管理器；人工视觉签收与GPU实测必须另列真实结果。

## 三层归属

- 平台GamePlatformWeather保留唯一服务器快照、服务器时间插值和客户端Surface/VFX通知。
- 平台GamePlatformSurface拥有`Scripts/SnowCoverageOpacity.py`唯一制作配方及真实`MF_GP_SnowCoverageOpacity`函数。覆盖形状与实覆强度分开：实覆强度为`saturate(SnowAmount/FullCoverAmount)`，完整覆盖阈值默认0.55。坡度/海拔继续由接收表面控制。
- 共享Village中性Landscape桥由同一平台配方展开少量纯数学节点，保持既有项目参数合同；不硬引用ClientOnly Surface函数或纹理，避免Dedicated Server Cook引入纯客户端资源。这是编辑器生成的适配图，不持有另一份天气状态，也不手工维护第二份覆盖算法。
- 平台GamePlatformVFX保留唯一降雪执行链。详细雪系统保留3个Emitter，近景CPU、远景和贴地漂雪GPU；绑定原生`NET_GP_WeatherAmbient`及固定边界。Low/Medium/High/Epic与Cinematic发射数量比例为25%/50%/75%/100%；系统数量上限为本地天气装饰保护配置，不是GPU性能承诺。
- 项目DBAClient保留既有世界期Data租约和Landscape MID桥。项目DBAWorlds仅在显式非Shipping审核开关下提供服务器天气演示数据；MobaCommon无独立职责，不新增模块。

## 地面表现与保护边界

仅修改现有Landscape母材质，不改高度、碰撞、导航、地图Actor、PlayerStart或用户手工地形。雪面加入受控法线强度及归一化、低对比基础色细节，并在积雪处解除草地AO暗斑。零雪量恢复原草地，湿润度继续改变既有底层外观。

该地面实现为薄积雪PBR覆盖；没有给全地图开启WPO、曲面细分或全屏透明覆盖。当前实际地图无雪苔标记岩石，未擅自添加屋顶、岩石或人为雪堆，也不将不存在的几何厚雪宣称完成。场景后续明确的厚雪边缘应使用局部授权雪堆Mesh，并独立核对脚部、碰撞及GPU成本。

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

## 验证进度

## 5厘米雪层脚印实施计划（用户已明确选择）

目标：保留Displacement默认5世界厘米。真实脚底落地时压实局部积雪，凹陷周边保留柔和雪唇，法线及粗糙度同时响应；新雪回填，融雪后清空。角色移动、碰撞、伤害和地形源高度继续由原权威链拥有。

方案比较：纯Decal成本低但不能形成真实置换凹陷；逐脚印独立Mesh或SceneCapture容易增加组件及整场景渲染成本。本次采用有界双缓冲RG16f高度场，扩展现有Surface世界服务，不增加天气服务、全局播放器或复制纹理。

依次执行：

- [ ] 原生图回归先确认缺少压痕支路；新增足底触地门限、重复事件、离地/静止、非法值的原生测试。
- [ ] `GamePlatformSurfaceClient/Public/Types/GamePlatformSnowContactTypes.h`提供厘米请求和只读材质绑定合同。既有`IGamePlatformSurfaceService`和世界子系统承接有界队列、Data材质租约、RT批量更新和世界退出清理；实现拆入Private/Snow。每世界两张RG16f，High为1024²/16米窗口，共8MiB纹理数据；这是内存规格，不代表GPU预算通过。
- [ ] `Public/Components/GamePlatformSnowFootContactComponent.h`及Private实现从最终骨骼姿态事件读取左右足与移动组件缓存地面，只在接地门限跨越时做确认射线；不新增Actor Tick。项目新手村桥通过Actor生成/退出事件装配组件，并把Surface瞬态高度场绑定至原64个Landscape MID。
- [ ] Monolith制作平台Update材质及项目中性雪支路；高低RT使用同一世界空间合同。原雪高度按压实率下降，边缘略抬起，最终归一化高度限制0—1，默认5cm内；BaseColor/法线/粗糙度采用克制的压雪变化。Dedicated Server不加载或Cook纯表现资产。
- [ ] 完整Editor/Client/Server增量构建及Cook，核对真实服务器战斗版本后启动两个RHI客户端。核对左右脚落地、他人可见足迹、连续行走/转向/跳跃、停雪/回填/融雪及退出；提供原生近景截图和视频，单列GPU实测及人工签收。

性能边界：最多16个接触合批、仅有足迹时进行回填更新、关闭后保留雪主体。窗口外不创建全地图纹理；无CPU读回、无逐脚印Decal或Niagara System。材料加载失败时保留原雪面并给出诊断，不伪造脚印成功。

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

写实PBR无置换基线Shader：121节点、12采样器、估计21像素纹理采样、148VS/631PS指令，已通过原生图回归；这些是编译统计，不能视为GPU毫秒。最终置换、Nanite实际动态绑定、双端四阶段、驻留截图和视频仍需在最终同版本包中独立验证，旧基线包不覆盖后续位移/音乐改动。

- 修改前原生连线回归因大雪透底失败；修改后测试通过，雪量0/0.23/0.5/0.85/1的平均平地混合权重约0/0.197/0.735/0.974/0.993，包含零雪量、单调与25°/50°/垂直面检查。
- 已通过Monolith创建、编译和保存真实材质、函数、Niagara及EffectType；两个GPU Emitter原生编译无错误和警告。
- 最新完整构建、Cook、双客户端阶段同步、原生媒体与GPU结果以本轮`Saved/Validation/VillageGroundSnow20261023`证据为准；未取得的结果不得标记通过。
