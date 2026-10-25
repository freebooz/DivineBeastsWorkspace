# SSLM新手村晨间光照与环境雾隔离审核

日期：2026-10-25。工程：`Game/DivineBeastsArena.uproject`，锁定UE5.8.0。状态：**三套隔离原型、20张固定机位原图、12场性能采样和最终独立进程冷重载已完成；P5人工审核及正式合入待批准。正式地图仍保持原阴天。**

后续直接授权的地面泛白修正只将正式地图曝光补偿由+2.5 EV调整到+1.0 EV，详见`VillageOvercastLighting_20261025.md`及`Saved/Validation/VillageGroundExposure20261025`。本报告P0哈希和三套隔离副本属于修正前的历史基线，不再代表当前正式地图字节；晨间光雾副本仍未合入。

本任务依据用户附件《神兽联盟新手村光照与环境雾优化及 Monolith MCP 自动配置调研实施提示词 V1.0》。附件原文位于 `C:/Users/Administrator/.codex/attachments/1571ed80-31a7-4383-b700-436008072411/已粘贴的文本.txt`。本轮目标是清晨7:00—9:00的艺术氛围，不建立天文日夜系统，不复制参考图的阴森浓雾风格。色彩继续遵守[SSLM配色V3](../Art/SSLM全局美术配色策略与色彩设计规范_V3.0.md)，不修改原材质/贴图RGB或使用全屏青色滤镜。

证据总目录：`Saved/Validation/VillageMorningEnvironment20261025`。下文“原生通过”仅指所列对象、字段与版本的真实写入/回读，不代表联机、Cook、GPU预算或人工视觉通过。旧失败记录保留，不以外层请求成功覆盖内部Python异常。

## 1. Monolith版本、连接与工具能力

本轮实际 `monolith_status` 返回版本 **0.23.0**、`server_running=true`、端口 **9316**、**1355 actions / 26 namespaces**、工程 `DivineBeastsArena`，见 `morning-first-0.json`。该返回的 `engine_version=UE5-CL-0` 不足以独立证明具体引擎版本；编辑器实际读取另返回 `5.8.0-0+UE5`，见 `morning-p0-inventory.json`。状态是本轮调用时的连接快照，不保证以后始终在线。

已实际执行能力发现和指南查询：`monolith_discover(mesh)` 返回63个mesh动作；`monolith_guide`返回onboarding；`describe_query(action_schema)`读取 `editor.run_python`、`editor.run_pie_smoke`、`editor.capture_pie_movement_clip` 的真实模式，分别留在 `morning-first-1/2/3.json`、`NativeDiscovery2/3.json`。`run_python`支持 `execute_file/execute_statement/evaluate_statement`；异步PIE入口由真实编辑器帧循环推进，须读取探针与最终报告，不能用工具受理代替运行通过。

| 范围 | 已有实际证据 | 仍需单列的边界 |
| --- | --- | --- |
| 场景/组件查询 | 真实Actor清单、高度雾原生组件字段、完整环境快照 | 单个属性不可读须记录，不能推断不存在 |
| 创建/复用灯、SkyAtmosphere、Cloud、HeightFog、LocalFog、PP | 独立能力地图内真实对象及字段写入/回读 | 不意味着所有相关属性均已测试 |
| 写入、保存、同进程重载 | `P1_Written.json`、`P1_Persisted.json` | 只证明能力地图所列字段 |
| 保存后独立Editor重载 | 最终 `ColdReload.json`：PID55032，5张地图环境参数回读一致，四副本非环境几何一致 | 仅覆盖本轮记录的最终参数和磁盘版本 |
| PIE运行、截图与移动 | 四地图各五机位共20原图；另有薄雾Medium/Low同湖岸原图；12场原生连续移动 | 人工动态视觉、NPC和技能威胁辨识仍待签收 |
| 性能采样 | 12场真实CSV、6段独立ProfileGPU、整GPU显存采样 | `get_region_performance`仅为LOD0保守估算；不得代替GPU实测或生产性能验收 |

官方项目说明仅用于理解工具范围，安装态及动作数以本机实调为准：[Monolith官方仓库](https://github.com/tumourlove/monolith)。本轮资产操作通过直接Monolith，没有使用HWebCodex或建立平行MCP服务。

## 2. P0正式新手村环境基线

正式包为 `/DBAWorldPack_Village/Maps/L_Village_Start`。`P0_Baseline.json`实际读取 **40个Actor**、**7345个已存实例**：107个宽冠树、3659个BostonFern、3579个WesternSwordFern，共7238个蕨类；这不是运行时全部角色、怪物或自动生成内容的总数。

| 对象/设置 | P0实际读取 |
| --- | --- |
| 环境对象数量 | 1 DirectionalLight、1 SkyLight、1 SkyAtmosphere、1 VolumetricCloud、1 ExponentialHeightFog、3 LocalFogVolume、1 PostProcessVolume |
| 方向光/天光 | 沿用前一任务已保存的阴天基线：强度均2.5、方向光角4°、中性白，天光实时捕获开启 |
| 曝光 | 既有 `VillageMoonlightExposure`，保留Manual固定曝光Bias 2.5；历史Actor名称不是当前月光风格证明 |
| 全局高度雾 | Density约0.02、Falloff约0.20、黑色Inscattering、StartDistance 0，组件的Volumetric Fog关闭 |
| 体积雾全局开关 | `r.VolumetricFog=1`，不能据此将组件关闭的正式雾写成“体积雾已启用” |
| GI/反射 | `r.DynamicGlobalIlluminationMethod=0`、`r.ReflectionMethod=2`；**当前不是Lumen GI基线** |
| 距离场/VSM | `r.GenerateMeshDistanceFields=0`、`r.Shadow.Virtual.Enable=0` |
| LocalFog | `r.LocalFogVolume=1`、`r.LocalFogVolume.RenderIntoVolumetricFog=1`；已有三个对象的实际位置/尺度见P0 |
| 地图GameMode | 原生 `DivineBeastsWorldGameMode`，`enable_world_composition=false` |

后续补证 `P0_LevelStructure.json`：正式地图 `WorldSettings.WorldPartition=None`、`bEnableWorldComposition=false`；Monolith原生Actor属性查询独立得到同样结果，确认此图未使用World Partition。`EditorLevelUtils.get_levels`仅返回正式图PersistentLevel。World的原生 `StreamingLevels`数组被Python保护，未把“一个已加载关卡”扩大声明为全部未加载流送配置已审计。`mesh.manage_sublevel`没有list分支，不能虚构只读枚举动作。

正式地图SHA-256为：

```text
9a84b138a1e800c836cbfb14453286c533ceb885cd019cdffeda67e3af2d9a6f
```

P0、作者各次检查及最终独立PID55032重载时的正式地图哈希均一致。作者工具逐次比较正式包哈希及副本非环境场景快照；未重新运行PCG、移动PlayerStart、改变道路/水体/手工场景、重建地形或覆盖其它任务的改动。

区域位置由真实湖体Bounds、既有湖畔茅屋、桃花材质宽冠树实例及低洼林地实例取得，见 `SpatialLayout.json`。当前基线未确认码头资产、完整村落中心或瀑布/Niagara瀑布。五位置审核使用入口、桃林、湖岸、既有湖畔茅屋和低洼林地，并明确记录上述缺项；不能将代理位置命名为已存在的码头或瀑布。

## 3. 复用架构、覆盖关系与服务器边界

服务器天气仍由 `GamePlatformWeatherRuntime` 唯一服务及Replicator负责；`GamePlatformData`持有生产资源身份与租约；`GamePlatformWeatherClient`负责快照插值、Surface桥与光雾适配。项目 `DBAClient`中的VillageSnow子系统按新手村身份装配地表纹理和既有光雾配置。没有新增Manager、Subsystem、Level Blueprint执行器、Niagara雾系统或运行时常驻轮询。

现有 `FGamePlatformWeatherAtmosphereProfile`是普通C++ struct，`ConfigureAtmosphere`不是UFUNCTION。该配置可管理光强倍率、Tint和雾增量，但没有完整的太阳角度、体积雾、LocalFog和质量字段。`GamePlatformWeatherPresetDefinition`只包含服务器安全天气状态与调度数据；本轮没有向它加入纯客户端渲染参数。

三套晨间参数使用**编辑器作者JSON清单**，由同一工具写入隔离副本的既有原生组件。它是可复用作者输入，不冒充已发布的原生环境DataAsset，也没有为每套预设复制运行时逻辑。后续若需要产品运行时切换完整环境Profile，应另行评审在现有客户端链上扩展反射合同及数据实例；当前原型不代表该能力已经交付。

正式地图配置仍选择 `db.weather.clear@1`。真实Cloudy会使方向光乘0.5125、天光乘0.818，并叠加冷色Tint和雾增量，因此本轮不能只在PIE手调灯，再忽略下一次天气/流送按基线重投影。隔离副本没有伪造正式地图名称，没有改动全局初始Weather映射。

副本位于 `/Game/Development/Weather`，在 `Build/Game/ServerPresentationExclusions.json`既有三服务器排除范围内；WeatherClient本身为ClientOnly且明确排除Server目标。本轮没有修改服务器合同或生产Cook配置。目录排除、模块允许列表和文件存在都不是Cook/Stage容器证明，本轮不宣称新增副本完成发布剥离验收。

## 4. P2隔离地图及自动化实现

所有下列地图均位于 `/Game/Development/Weather/VillageMorning/Maps/`：

| 地图短名 | 用途 |
| --- | --- |
| `L_Monolith_EnvironmentCapability` | 原生环境对象创建、写入、保存/回读能力验证 |
| `L_Village_Baseline_Overcast` | 正式阴天基线副本，P3/P4比较对照 |
| `L_Village_Morning_Clear` | 清晨晴朗候选 |
| `L_Village_Morning_LightFog` | 清晨薄雾主要候选，尚未人工选定 |
| `L_Village_Morning_Mist` | 清晨晨雾候选 |

四份Village副本通过引擎SaveAs建立并显式加载，逐项核对非环境Actor、地形/场景组件、实例变换及出生点。保存证据为 `Baseline_Saved.json`、三个 `Village_Morning_*_Saved.json`、`CopyGeometry.json`。仅归一化经核实的World外层路径及临时水面MID名字，不能忽略真实位置、网格、材质或实例变化。

主要工具：

- `Tools/Unreal/World/ReviewVillageMorningEnvironmentUE.py`：只读P0、保护快照、能力/副本检查、作者应用、保存、冷重载审计。
- `Tools/Unreal/World/VillageMorningPresets.json`：共享光照、三候选、三级质量的作者输入。
- `Tools/Unreal/World/VillageMorningPIEReviewUE.py`：白名单PIE一次性引导、八纹理/MID绑定、相机、原生移动和运行快照。
- `Saved/Validation/VillageMorningEnvironment20261025/AnalyzeFogPerformance.py`及同目录说明：只读CSV分析，保留缺列/缺值，不把测试算法样本作为实测。

复用已有 `AuthorVillageOvercastLightingUE.py`、`AuthorVillageMistUE.py`的保护及字段经验；这些旧工具包含正式地图路径与历史规则，不直接当作本轮副本入口。旧 `Tests/Assets/ValidateVillageMistUE.py`仍断言历史月光值，旧Ambient校验也仍含六纹理/旧草地路径，未将它们误用为本轮通过证明。

## 5. 最终作者参数与三级降级

本节是最终已保存候选的参数摘要，唯一机器可读作者输入为 `VillageMorningPresets.json`，实际保存值见对应 `*_Saved.json`。参数仍属于待人工审核候选，不是性能或视觉批准值。

| 共享参数 | 当前候选 |
| --- | --- |
| 太阳高度角 | 12°，原生Pitch -12°；Yaw/Roll取基线并保持 |
| 方向光 | 3.5，光源角3°，白色LightColor、启用6250K温度，体积散射强度0.7 |
| 天光 | 强度2.5，沿用原生捕获设置 |
| 曝光/后期 | Bias 2.5；保留原PP白平衡、饱和度、对比度、AO、泛光等，不套全局滤镜 |
| 雾散射颜色 | 作者sRGB输入(68,79,82)，明确IEC sRGB→线性，最终线性约(0.057805,0.078187,0.084376) |
| 体积雾 | Scattering 0.2、ExtinctionScale 0.45、Distance 12000cm、Start 600cm、NearFade 400cm、Emissive 0 |
| 体积雾Albedo | 最终原生回读R235/G241/B242/A255。已修复FColor的BGRA位置参数问题，使用显式r/g/b/a并断言回读；最终采样使用修复后的资产 |
| 第二层雾 | Falloff 0.5，Offset取真实湖面放置基准；Density随候选 |
| LocalFog | 三处复用对象，位置按湖面/桃树根部/低洼林地，统一缩放；Start 900cm、Falloff 2000、G 0.2、Emissive 0 |
| 天气风 | 原Clear定义0.70、0°、阵风0.20、0.10Hz；网络量化后MPC目标约0.7019608 |

| 候选 | Fog Density | Falloff | Start/cm | MaxOpacity | 第二层Density | Local径向/高度消光 | Volumetric组件 |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| Morning_Clear | 0.008 | 0.20 | 1500 | 0.38 | 0.002 | 0 / 0；保留对象，以零消光停用局部层 | 开启 |
| Morning_LightFog | 0.012 | 0.22 | 1200 | 0.48 | 0.004 | 0.012 / 0.08 | 开启 |
| Morning_Mist | 0.016 | 0.25 | 900 | 0.56 | 0.006 | 0.022 / 0.14 | 开启 |

| 质量 | 既有Scalability档 | r.VolumetricFog | GridPixelSize | GridSizeZ | 保底 |
| --- | ---: | ---: | ---: | ---: | --- |
| High | 2 | 1 | 16 | 64 | 完整候选体积雾＋有界局部层 |
| Medium | 1 | 1 | 24 | 48 | 降低体素采样，仍需实测闪烁与移动稳定性 |
| Low | 0 | 0 | 32 | 32 | 保留高度雾/局部空气透视，不承诺体积光束 |

PIE引导对全部Scalability组使用同一档，再显式应用上表体积雾参数，故各档之间不只改变雾。候选必须与**同档Baseline**比较，不跨档把总体质量变化归因于雾。雾自身开销还需GPU事件或受控开关对照确认。

## 6. 实际成功、错误和替代路径

| 发现/操作 | 实际结果与处理 |
| --- | --- |
| 原生组件写入及保存 | 能力地图形成P1_Written/P1_Persisted，后者记录同进程重载一致及正式包未变 |
| SkyAtmosphere字段 | 当前引擎实际Python拼写为 `aerial_pespective_view_distance_scale`，不能自行改成英语上更正确的Perspective拼法 |
| Rotator位置参数 | 位置参数顺序造成太阳旋转错误；已改为显式 `pitch/yaw/roll`，保存前断言Pitch -12及原Yaw/Roll，三个候选已重新保存 |
| generic设置CDO | `unreal.DivineBeastsVillageSnowSettings`属性不存在；改用完整脚本类路径 `load_class`＋`get_default_object`，按CDO真实原生字段名访问 |
| SoftObjectPath转换 | 本机 `SystemLibrary.break_soft_object_path`实际不存在，不能因C++ UFUNCTION存在就宣称Python可调；改用实际 `export_text()`对象路径加载 |
| Landscape Nanite属性 | snake_case属性缺失，原生 `SourceLandscapeComponents`又被Python保护，均保留失败记录；改由 `mesh_query`原生反射读取真实源组件顺序，存入NativeNaniteBindings |
| PIE相机生成 | 试用的 `begin_deferred_actor_spawn_from_class`未暴露；改为复用副本既有CameraActor，运行后仍需核对ViewTarget实际归属 |
| 已有脚本输出None | `execute_file`的None是Python返回值，不代表内部探针成功；逐项读取 `python_ok`、错误文本、MID回读及相机探针 |

失败证据保存在 `morning-bootstrap*.json`；Bootstrap前几轮有Python异常，即使外层JSON的 `ok=true`也不计为该能力通过。成功替代后的关键证据为 `Bootstrap6_Setup.json`、`Bootstrap6_Runtime.json`、`RefinedRotation_Setup.json`、`RefinedRotation_Runtime.json`，并以更晚实际运行记录覆盖其相机/参数版本范围。

旧PID46180的冷重载早于最终旋转/颜色修复，未用作最终证据。已在独立新PID55032重新打开能力图、Baseline和三候选：`ColdReload.json`记录五图环境一致、四副本几何一致、正式包未变。`NativeNaniteProvenance.json`保存最终地图SHA及四份Monolith原生查询原文的出处，64项实际源顺序逐一与审核引导匹配。

## 7. 地表与运行时可比性

副本不符合正式Village装配身份过滤，故不会自动运行全部VillageSnow、苔藓、鸟群、足迹及其它项目表现；本轮明确记录 `production_village_assembly_active=false`，没有伪造地图名或开放生产权限。

为避免将缺失的运行地表贴图误当光照差异，PIE引导从实际VillageSnow设置CDO读取8张草地/雪贴图，写入现有Landscape MID，设置 `VillageDryColor=(1,1,1,1)`、`VillageWetColor=(0.55,0.60,0.52,1)`及Clear状态雪量/湿度0，逐项回读。`RefinedRotation_Setup.json`记录 **64个实际MID、64个实际Nanite材质槽**。Nanite使用 `NativeNaniteBindings.json`的原生源组件顺序，未按Actor数组猜测映射。

正式生产链使用GamePlatformData异步World租约；当前审核是显式Editor同步加载并保留引用，随后只修改PIE对象。它证明本轮地表参数和Nanite显示槽的绑定结果，不证明生产租约的取消、重连、流送或打包生命周期，也不代表副本已完整复现生产Village。

审核引导显式应用既有Clear定义，保留0.70风。该审核调用使用编辑器加载对象，不纳入生产Data租约证明。所有截图/性能轮次均需记录天气、风MPC、MID/槽数、质量、实际Viewport、ViewTarget及地图包，避免将默认风0、旧光雾值或SpectatorPawn视角混入最终比较。

## 8. P3固定机位原图

[打开本地四方案对照图集](../../Saved/Validation/VillageMorningEnvironment20261025/ReviewGallery.html)。图集只展示未改像素的UE5.8原生PNG，原图及SHA见[VisualEvidence.json](../../Saved/Validation/VillageMorningEnvironment20261025/VisualEvidence.json)。统一2257×1261输出、FOV75、Manual Exposure Bias2.5；未调色、未锐化、未AI重绘。正式图的保护副本作为原效果对照，所有副本使用同一八纹理/MID审核引导。

| 真实位置 | 原阴天 | 清晨晴朗 | 清晨薄雾 | 清晨晨雾 |
| --- | --- | --- | --- | --- |
| 入口 | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Baseline_High/frame_001.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Clear_High/frame_001.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_High/frame_001.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Mist_High/frame_001.png) |
| 桃花林 | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Baseline_High/frame_003.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Clear_High/frame_003.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_High/frame_003.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Mist_High/frame_003.png) |
| 湖岸 | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Baseline_High/frame_005.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Clear_High/frame_005.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_High/frame_005.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Mist_High/frame_005.png) |
| 现有湖畔茅屋 | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Baseline_High/frame_007.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Clear_High/frame_007.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_High/frame_007.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Mist_High/frame_007.png) |
| 低洼林地 | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Baseline_High/frame_009.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Clear_High/frame_009.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_High/frame_009.png) | [原图](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_Mist_High/frame_009.png) |

另留[薄雾Medium](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_Medium/frame_002.png)及[薄雾Low](../../Saved/Validation/VillageMorningEnvironment20261025/Visual_LightFog_Low/frame_002.png)同湖岸机位降级图。中低档探针额外实际回读CameraActor_0、ScreenPercentage100及DynamicRes0。

截图验证证明真实场景可运行、参数生效及相机对照，人工视觉签收仍为false。现有107棵宽冠桃树是低模原型，现有CSM/无Lumen基线也保留；光雾优化不能宣称替代模型精度或已达到电影级美术完成度。需人工继续检查曝光跳变、时间重投影拖影、局部雾边界、道路/NPC/技能辨识。未制作缺失的码头、完整村落中心或瀑布。

## 9. P4真实性能结果及边界

4份等价场景副本×3质量，12场各60秒；按真实FrameTime累计剔除30秒预热，保留段每场约30.48—30.51秒。引擎原生MoveComponentTo在第29秒开始连续移动，12份起止快照均确认位移超过1000cm。截图与性能会话分开，无Python逐帧相机驱动。性能会话仍包含CSV记录、一次保留段MCP轮询及边界探针，结果是匹配审核引导下的单轮观测，不是无人干预的独立游戏基准。

设备：RTX4090 24564MiB、驱动610.88、i9-14900K、D3D12。12场实际Viewport均2257×1261，FOV75、VSync0、风定义0.70/MPC量化约0.7019608。各场原生日志实际回显ScreenPercentage100、DynamicRes0，见[逐场CVar补证](../../Saved/Validation/VillageMorningEnvironment20261025/FinalPerformance/ConsoleRenderSettingsEvidence.json)；没有把Cmd文本当回读。原始快照未直接含这两个字段的事实保留；内部Primary/Secondary渲染像素尺寸未独立完整证明。

UEditorEngine在t.MaxFPS0且关闭平滑时仍存在120 FPS夹取分支。经锁定引擎源码核实，测试临时使用t.MaxFPS1000绕过该隐式120；原生采样后已恢复0，未写入项目INI或驱动设置。1000是测试高上限，不称无界帧率。

| 方案 | 画质 | 平均FPS | GPU帧均值/ms | 帧P99/ms | GPU雾事件CSV均值/ms | 整GPU显存峰值/MiB |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Baseline | High | 127.52 | 7.005 | 9.784 | 无该列 | 7372 |
| Baseline | Medium | 148.68 | 5.883 | 8.585 | 无该列 | 7356 |
| Baseline | Low | 177.00 | 4.251 | 7.321 | 无该列 | 7026 |
| Clear | High | 126.34 | 7.055 | 9.915 | 0.0664 | 7468 |
| Clear | Medium | 149.82 | 5.795 | 8.229 | 0.0449 | 7396 |
| Clear | Low | 179.66 | 4.150 | 6.877 | 0.0000 | 7065 |
| LightFog | High | 125.25 | 7.145 | 10.090 | 0.0789 | 7474 |
| LightFog | Medium | 148.85 | 5.918 | 8.572 | 0.0444 | 7348 |
| LightFog | Low | 173.65 | 4.239 | 7.556 | 0.0000 | 7055 |
| Mist | High | 125.92 | 7.104 | 9.952 | 0.0569 | 7453 |
| Mist | Medium | 148.39 | 5.867 | 8.583 | 0.0529 | 7383 |
| Mist | Low | 171.50 | 4.263 | 7.737 | 0.0000 | 7062 |

平均FPS为1000×帧数/总FrameTime。High薄雾相对原效果FPS -1.77%；Medium约+0.11%。Low晨雾约-3.11%，该档体积雾已经关闭，不能把差异全归因于体积雾。12场均未观察到>50ms帧，最大单帧34.034ms；单轮30秒有效段不能证明长时稳定或无闪烁。

CSV仅精确采用GPU/VolumetricFog列，Baseline缺列保持null，Low候选在全局关闭时真实为零；这不等于全部高度雾/LocalFog成本为零。该CSV事件为graphics queue 0的exclusive Busy+Wait口径。另行ProfileGPU单帧ComputeVolumetricFog的Graphics Inclusive Busy：Clear High0.061ms、LightFog High0.066ms、Mist High0.069ms、LightFog Medium0.034ms；Baseline组件关闭、Low全局关闭，无此节点。Inclusive包含子节点；不跨队列求和、不将CSV和单帧值混为同一指标。原始行及源码依据见[单帧GPU证据](../../Saved/Validation/VillageMorningEnvironment20261025/GPUProfileSummary.md)。

显存是nvidia-smi整GPU设备占用，包含其它进程、编辑器和资源缓存。按各场Setup UTC+30—60秒近似窗口取29/30个采样：均值7023—7461MiB，全场峰值7474MiB；不是独占游戏进程显存，也不能直接断言雾新增了某个分配量。

完整原CSV哈希、统计、实际物理地图及9组有几何保护的跨图对比见[性能摘要](../../Saved/Validation/VillageMorningEnvironment20261025/FinalPerformance/Summary.md)、[场景组比较](../../Saved/Validation/VillageMorningEnvironment20261025/FinalPerformance/SceneGroupComparisons.json)、[运行条件](../../Saved/Validation/VillageMorningEnvironment20261025/FinalPerformance/RuntimeConditions.json)和[设备显存窗口](../../Saved/Validation/VillageMorningEnvironment20261025/FinalPerformance/DeviceMemoryWindows.csv)。保留了物理map身份，没有为匹配Baseline改写成同名地图。未测打包客户端、完整生产Village装配、联机NPC/技能并发及长时间多轮重复，不声明Production Ready。

## 10. 修改清单、研究依据与P5门禁

本轮原生修改仅为能力地图和四份隔离副本及其引擎SaveAs产生的合法附属数据；实际组件字段由 `P1_Written/P1_Persisted`、`*_Saved.json`逐项列出。原正式地图、材质RGB、出生点、PCG、共享天气/战斗合同未因本轮候选而改写。源码工具及参数清单见第4节；完整新增源码/原生地图清单见`ChangeList.json`，图像、采样和审计产物按证据总目录及VisualEvidence/PerformanceManifest登记；未改其它任务的内容。

研究采用一手资料：

- [Epic UE5.8 Local Fog Volumes](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine)：局部雾为有径向衰减的球形区域；非均匀缩放不支持。启用体积雾后参与体积合成，需重新评估重叠与成本。
- [Epic UE5.8 Volumetric Fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine)：体素分辨率、可视距离及阴影影响成本；时间重投影可能留下快速光照拖影。文档给出的其它硬件历史数值不作为本机预算。
- [Monolith官方仓库](https://github.com/tumourlove/monolith)：只作能力与工作流背景，动作注册/参数模式及成功结果均采用本机证据。
- 本机锁定源码：`EditorEngine.cpp:2523`、`UnrealEngine.cpp:12193`核对帧率限制；`LandscapeProxy.h:1043`、`LandscapeComponent.h:1017`、`GamePlatformLandscapeMaterialBridge.cpp`核对MID与Nanite绑定。源码可用性不等于PythonAPI已经暴露，失败与替代见第6节。

用户附件P5原文明确要求：

> 测试副本完成后，提交视觉截图、参数清单、性能数据及风险说明，供人工审核。
>
> 审核通过后，再将已验证的配置合并到正式新手村关卡。

因此，**P5人工审核尚未通过，禁止把三套晨间候选合入正式新手村**。当前待完成：人工动态视觉与NPC/技能辨识签收；缺失实际区域的范围确认；人工选择候选与正式应用授权。最终版本独立冷重载、20原图、12场CSV及6段GPU Profiler已留证。批准后仍须保留正式原始配置和回退点，按实际修改范围验证Cook/运行，不能把本报告或隔离副本保存成功当作正式上线验收。
