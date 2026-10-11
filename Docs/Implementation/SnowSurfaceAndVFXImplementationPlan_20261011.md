# 神兽联盟真实积雪材质与风吹雪粉实施计划（2026-10-11）

## 基线与交付边界

- 唯一工作空间为 DivineBeastsWorkspace，工程 Game/DivineBeastsArena.uproject，锁定 UE5.8。
- 依据 AGENTS.md、解决方案总体规划、插件开发规范；保留当前有改动的用户界面、落叶 Niagara、DBAClient 文件，不清理、不覆盖、不重置。
- 实际已存在 GamePlatformSurface 的 MPC/7 个材质函数/2 个母材质/5 个天气表面贴图，GamePlatformVFX 的 3 个降水粒子系统，DBAWorldPack_Village 的 8 个天气预设。存在文件不等于加载/编译/运行通过。
- 仅在 GamePlatformSurface（第一层通用材质及客户端桥接）、GamePlatformVFX（第一层客户端表现）、GamePlatformWeather（第一层服务器天气事实和客户端只读反馈）、DBAPresentationPack_Core（第三层项目公共纯表现包）增量扩展。第二层 MobaCommon 无独立职责，本次不增设类或依赖。
- 纹理 SourceArt 并非 UE Texture2D；禁止脚本/工具生成伪 .uasset/.umap；UE 资产必须通过真实编辑器创建、保存及回读。不要在仍有其他会话编辑的原始资产上直接重写。
- Dedicated Server 仅保持已存在的天气服务器权威和复制；不得依赖 Surface/VFX 或 Cook 风吹雪粉、粒子材质、审查地图。
- 禁止全局逐帧 Tick 来更新材质、服务端雪片网络同步。积雪遮罩依据世界 MPC 值更新，Particle 视效在客户端表现层。

## 分步执行与门禁

| 阶段 | 目标 | 真实验收 |
| --- | --- | --- |
| P0 项目基线 | 核验现有插件边界、锁定引擎、未提交文件、现有资源和编译入口 | 路径/资源清单与障碍留证 |
| P1 源纹理优先 | 生成雪面微高度、宏观覆盖噪声、贴地雪粉 Sprite；固定种子和 SHA-256，生成前禁止覆盖不同内容 | PNG 实体、像素通道/尺寸、无缝边界、Alpha 和 manifest 核验通过 |
| P2 UE 材质资源 | 导入三张纹理；创建独立的 M_GP_SnowCover_Detailed 母材质和 M_GP_VFX_SnowGroundPuff 粒子材质，连接 Surface MPC，法线和粗糙度与雪遮罩一致；创建项目公共 MI_DBA_Snow_Detailed 实例 | 原生 UMaterial/UMaterialInstanceConstant/Texture2D 保存回读和 Shader 错误为零 |
| P3 蓝图/Niagara | 使用已有 NS_GP_Weather_Snow 和 WeatherClient 的中立语义播放；补充近地风吹雪粉系统/场景参数，按资产编译和表现可见性判断 | Niagara 有真实发射器/Renderer及材质绑定、蓝图绑定有效且无引用缺失 |
| P4 独立审查地图 | 在不修改正式 L_Village_* 地图下生成 L_DBA_SnowReview，布置水平地面、垂直石面、坡面；连接不同的积雪参数和雪粉 | 重启编辑器后仍可加载，材质与天气切换可见 |
| P5 验证 | 源素材形态门禁、Python 静态检查、模块编译、UE资产校验、UE自动化、客户端/服务器 Cook 和进图检查 | 逐条记录命令、退出码、日志和未执行事项 |
| P6 人工验收 | 雪层形态、真实材质细节、冷暖光、坡度、融雪、风吹雪粉和性能人工检查 | 需真人观察并确认，不以自动化测试代替人工通过 |

## 技术参数与降级

1. 采用世界位置 XY 周期取样避免网格 UV 不一致；Material Parameter Collection（材质参数集合）沿用 GP_Surface_GlobalSnowAmount、GlobalSnowHeightCm、GlobalWetness 等固定合同。
2. 坡度使用 VertexNormalWS（世界空间顶点法线）避免 Normal 图回环；雪遮罩由强度 × 坡度 × 高度 × 周期噪声形成，防止墙面全白。
3. 雪微高度只参与光学细节/遮罩，不把实验性 Nanite Tessellation 强行写入正式材质。完整/简化版本按原有 Surface 母材质合同分离。
4. 近地风吹雪粉仅供客户端选择性启用；最大粒子数、覆盖半径和渲染距离需要实际 GPU/设备证据再确定。
5. 天气不决定人物移动摩擦/技能伤害，MPC 只修改表现。失效时沿用无雪/现有材质回退，不阻断世界权威。
6. 任务期间若 UE Editor 正被其他会话占用，保留唯一正在工作的编辑器实例，不杀进程；无法安全生成 UE 资产时只写入待执行的资产制作脚本并如实标记阻断。

## 当前阶段状态

- P0：已识别工作区、现有基础资产、用户未提交界面与落叶特效修改和并发 UE Editor 会话。
- P1—P6：以真实操作及每个验收动作的证据更新，不使用“预计完成”冒充“已完成”。

### 2026-10-11 实际执行阶段状态与证据

- P0完成：核查正式工作区、既有Surface/Weather/VFX资产和用户未提交的UI、落叶工作；没有清理原文件。
- P1完成：2048x2048雪微高度、2048x2048宏观遮罩、512x512 RGBA雪粉3张原创源PNG，以及SHA256 Manifest，确定性--verify通过。
- P2完成：真实UE5.8导入3张Texture2D，生成53节点平台写实母材质、6节点Niagara雪粉材质、第三层神兽联盟MI，并在真实Editor内编译、保存、回读。
- P3完成：基于原NS_GP_Weather_Snow复制创建NS_GP_Weather_Snow_Detailed，追加NE_GP_Snow_GroundDrift；18粒/秒、生命周期0.55-1.55秒、局部空间发射和水平风漂移。修复Sprite用途后Niagara验证0错误0警告、valid=true，三个Emitter脚本均UpToDate。
- P4完成开发审核接入：独立L_DBA_SnowReview关卡真实保存，包含地面/坡面/直立墙体/光照、PlayerStart和既有BP_DBA_WeatherReviewController蓝图。补齐平台实际雨雪VFX Definition，并将platform.weather.snow@1绑定增强NS，保留旧NS回退；正式Village地图尚未改动。
- P5资产门禁完成：VerifySnowDetailedAssets.py在真实UE Editor检查了8类资源、父子材质、Niagara标志及天气Definition，结果SNOW_UE_ASSET_VALIDATION_PASS。完整编辑器编译、客户端与Dedicated Server Cook、平台原生自动化测试以单独执行结果记入验收，不凭资源存在假定通过。
- P6待真人视觉验收：检查雪面高度遮罩、倾角、融雪湿润度、雪粉触地、材质亮度、粒子遮挡、不同客户端画质与GPU；自动化测试不代替人工验收。
- 当前局部雪粉速度固定为可配置矢量，尚未实现服务器风向逐帧驱动；禁止将此升级称为动态风场已验收。

### 附加门禁结果与可视化局限

- Tests/Architecture/ValidateSnowDetailSourceArt.py：SNOW_SOURCE_QA_PASS textures=3，PNG尺寸、无缝边缘、透明度及SHA256正确。
- Tests/Architecture/ValidateSnowCookScopes.py：SNOW_3TIER_STATIC_PASS、SNOW_CLIENT_SERVER_COOK_RULES_PASS，证明静态资源归属与Cook配置已写入，不等于完成实机Client/Server Cook。
- Tests/Architecture/ValidateWeatherSourceArt.py：WEATHER_SOURCE_QA_PASS textures=9 audio=3；ValidateWeatherAuthoringIntegration.py：WEATHER_AUTHORING_STATIC_PASS scripts=10 systems=3 emitters=5。GamePlatformWeatherIsolation.Tests.ps1：Pester5个测试均通过。
- Tools/Unreal/Weather/AuthorSnowDetailedAssets.py等9个Python生成和资产验收脚本py_compile通过；DBAPresentationPack_Core.uplugin通过真正UTF-8 JSON解码验证。
- Game/Saved/Screenshots/SnowReview目录下已由UE生成1张材质技术预览及3帧Niagara预览，但人工查看发现材质预览无雪、Niagara离屏帧近乎全黑；这是默认场景参数/脱离真实PIE环境的限制，**不能用这些图签收视觉质量**。
- 详细真人验收步骤、天气蓝图Actor设置与问题单见Docs/Implementation/SnowSurfaceManualReview_20261011.md；仍需正式PIE/真实Client运行、大雪/小雪/无雪切换、过度绘制分析与实测GPU数据。


