# 通用雪景与苔藓PBR集成记录（2026-10-12）

## 三层逻辑和资产所有权

第一层 `GamePlatformSurface`（通用环境表面插件，ClientOnly／Editor）保留唯一状态结构、8项MPC（材质参数集合）合同、积雪/苔藓/湿润表面遮罩以及可选真实材质资产。本次新增 `M_GP_Surface_SnowMoss_PBR`（通用积雪苔藓母材质）制作链，不新建天气权威、不引入玩法属性、不扩充服务器复制消息。

第一层 `GamePlatformWeather`（平台天气插件）继续产生并复制权威天气快照；`GamePlatformVFX`（平台视觉特效插件）继续负责近景、远景和地面雪粉Niagara（粒子系统），不重复实现VFX播放器或世界状态。

第二层 `MobaCommon`（MOBA通用层）无本任务独立规则，不机械新增模块。第三层 `DBAPresentationPack_Core`（项目通用纯表现包）仅允许 `MI_DBA_MossSnow_Cliff`（神兽联盟项目材质实例）调整覆盖密度与坡度参数，不保存平台算法。共享 `DBAWorldPack_Village`（新手村世界包）保持服务器安全基础材质，不向专用服务器Cook（资源烘焙）直接添加客户端Surface/VFX硬引用。

## 源资源和实现

`Tools/Unreal/Surface/GenerateMossPBRSourceArt.py`（苔藓PBR源纹理生成器）以固定随机种子生成5张无缝源PNG：2048像素基础色、法线、ORM通道、微高度和1024像素宏观覆盖；清单 `MossPBRSourceManifest.json`（资产源文件校验清单）记录所有权、尺寸和SHA-256。已有不同素材禁止覆盖。

`Tools/Unreal/Surface/AuthorMossSnowPBRAssets.py`（UE5.8真实资产导入及Shader构图工具）只在正式工程与编辑器中工作，默认inspect（只读规划），apply（实际制作），verify（真实引擎资源回读）。新母材质使用世界XY宏观采样、几何顶点法线坡度遮罩和雪量连续阈值；颜色、法线、粗糙度、环境遮蔽完整混合，金属度为0，湿润只改变光学表现。现有 `MF_GP_MossLayer`（苔藓材质函数）保留旧调用方，不就地覆盖旧资源，增量提供独立的写实高质量母材质。

## 执行与验收边界

源图通过SHA和边界测试，仅代表源美术资源有效。真实Texture2D（引擎纹理）、Material（母材质）、MI（材质实例）须原生UE5.8导入、Shader编译、保存与重载验收。最终仍需对雪量0/0.23/0.85、垂直/25度/50度岩面、晴雪转换、苔藓岩缝覆盖、湿润光泽、地面雪粉与风向、不同画质的GPU过度绘制进行人工视觉及性能审查。

本次不主动覆盖正式Village共享地图、旧雪MI、旧材质函数及其他会话的PCG、UI、VFX修改。正式Server Cook不应含新增Surface纹理或项目纯表现材质；交付前需独立检查Cook闭包。

## 本次实际执行证据

- 平台苔藓源纹理5张：`GenerateMossPBRSourceArt.py --create`（生成）及 `--verify`（只读校验）均输出 `MOSS_SOURCE_QA_PASS maps=5`，包括尺寸、SHA-256及平铺接缝。
- UE5.8真实引擎资源：已导入5张Texture2D，创建 `/GamePlatformSurface/Materials/M_GP_Surface_SnowMoss_PBR`（含68个材质表达式）及 `/DBAPresentationPack_Core/Materials/Environment/MI_DBA_MossSnow_Cliff`（项目MI），引擎报告 `MOSS_SNOW_UE_ASSETS_VERIFIED ... nodes 68`，Commandlet结果为0。
- 独立编辑器审核地图：已创建 `/Game/Development/MossSnow/Maps/L_DBA_MossSnowReview`，引擎报告 `MOSS_SNOW_REVIEW_MAP_SAVED`，地图文件已存在；包含平岩、25°/50°坡面、垂直岩壁、天气审核控制器和默认光照。
- 数值回归：`Tests/Assets/ValidateMossSnowCoverageResponse.py` 返回 `MOSS_SNOW_COVERAGE_RESPONSE_PASS`；Python脚本编译及默认只读规划均通过。
- **未签收**：客户端实际PIE截图及操作体验、真实GPU性能预算、全工程Editor编译、Client/Server Cook闭包与真实游戏主城/新手村效果。源图和地图存在不代表这些测试已通过。
 
- 独立UE5.8进程二次只读回读：`MOSS_SURFACE_MODE=verify` 重新加载插件母材质与项目MI，输出 `MOSS_SNOW_UE_ASSETS_VERIFIED ... nodes 68`，UE Commandlet返回0。
- 插件目标平台静态隔离检查通过：`GamePlatformSurface`（ClientOnly/Editor）和 `DBAPresentationPack_Core`（仅Game/Client/Editor），结果为 `MOSS_SNOW_PLUGIN_ISOLATION_STATIC_PASS`。该检查不等同于Server Cook实际闭包。
- 当前组合母材质为桌面端／高品质可选方案：低画质或移动端应继续使用 `M_GP_Surface_Lite`（轻量母材质）或后续受控静态开关版本；不要将本组合材质硬编码到全项目材质。

## 2026-10-12 后续执行范围

已扩展 `Build/Game/ServerPresentationExclusions.json`（服务器表现资源剥离真源）覆盖苔藓纹理、项目雪苔MI和独立开发审核图；随后通过受管配置生成器同步三个服务器配置及专服打包规则。运行 `ServerPresentationPolicy.py check` 和 `TestServerPresentationPolicy.py`：已验证策略无漂移且25个测试通过。

在 `DivineBeastsPresentationClient`（项目客户端表现模块）新增条件式岩石雪苔组件桥，只在真实Village客户端世界通过GamePlatformData异步租约加载MI，对具有 `DBA.Surface.MossSnow` 标签的UMeshComponent（网格组件）进行可回退材质覆盖。没有主动标记的网格全部保持原材质；共享Village世界Map/中性LandscapeMaterial不加对Surface/Presentation资源的硬引用。项目默认软资源路径、角色生命周期清理和回归测试同步扩展。新增客户端Cook目录为 `/GamePlatformSurface/Textures/Moss`、`/DBAPresentationPack_Core/Materials/Environment`；开发客户端另加入独立审核图。

新增 `Tests/Architecture/ValidateVillageMossSnowIntegration.py`（静态三层和资源隔离回归）、`Tests/Assets/ValidateMossSnowCookedContainer.py`（真实Client/Server IoStore审计）、`Tools/Unreal/Surface/CaptureMossSnowGPUReview.py`（UE三态SceneCapture GPU捕获）及 `Tests/Assets/ValidateMossSnowGPUFrames.py`（像素有效性校验）。已以UE5.8真正运行 `VerifyVillageLandscapeBridge.py`（共享Village资源/AssetRegistry原生审计），重新生成依赖报告并复跑旧静态门禁通过。GPU原图必须由实际RHI生成，不能用文生图替代验收；人工签字待用户确认。
- GPU黑图根因证据：在 `UnrealEditor-Cmd`（命令行编辑器）中首次脚本遇到UE5.8反射别名差异，已改为 `MaterialLibrary` 和标准数学LookAt。实际使用 `-ExecutePythonScript -d3d12` 成功输出三张1280×720 PNG，但离线像素报告首图中央区域平均值约0.10/255、标准差约0.29（近黑帧），且三张文件字节数完全一致。因像素QA失败不认定具备GPU可见雪景；另新增 `RunMossSnowGUIReview.py`（Slate预热回调工具），独立GUI启动尝试未获得可核验图，必须进一步排查GUI/RHI上下文。
- `ValidateMossSnowCookedContainer.py`（真实IoStore雪苔审计脚本）已编写及通过Python语法检查，但尚未获得包含当前新增客户端C++版本的新Client/Server Cook包和UnrealPak CSV，因此未执行容器实证；不能借用2026-10-11的旧打包结论。


## 本轮原生资产与脚本实际门禁

- `Tests/Foundation/Scripts/TestServerPresentationPolicy.py`（服务器纯表现剥离测试）25项全部通过；`ServerPresentationPolicy.py check`（受管配置防漂移检查）通过。
- `Tests/Architecture/ValidateVillageMossSnowIntegration.py`（雪苔项目桥三层静态门禁）返回 `VILLAGE_MOSS_SNOW_3TIER_STATIC_PASS` 与 `VILLAGE_MOSS_SNOW_COOK_SCOPE_STATIC_PASS`。`ValidateMossSnowCoverageResponse.py`（遮罩端点数值回归）通过；五张PNG完整性复核通过。
- 原生UE5.8只读 `VerifyVillageLandscapeBridge.py`（真实地图/材质依赖审核）已重跑，产生 `Game/Saved/Validation/VillageSnowBridgeAssetAudit_20261011.json`；包含1个Landscape（地形）、48节点中性母材质与0个客户端专属资产违规硬引用。随后旧 `ValidateVillageSnowBridgeIntegration.py`（历史雪桥集成测试）重新通过。
- 独立UE Editor实际生成 `Saved/Validation/MossSnowVisual_20261012` 中 Clear/LightSnow/HeavySnow 三张图，但原始图全部近乎全黑且像素无区分；`ValidateMossSnowGPUFrames.py` 正确判定失败，**不能记作视觉验收**。完整 D3D12 命令行渲染环境与GUI异步预热仍需进一步排查，脚本与失败原始图保留供回归，不得把它们放入正式成果展示。
- 正式Client完整构建发现当前源码版引擎尝试789个编译动作，并与另一个独立工作树同时构建。为不侵占并行任务资源，主动停止**仅本轮自身**低效的构建Job，保留源码。改用 `-UsePrecompiled`（复用引擎预编译清单）后UE明确报告该单体Client目标缺少多处预编译manifest，快捷构建未通过；此处不是关闭引擎模块可规避的问题。随后试用 `-SingleFile=DivineBeastsVillageMossSurface.cpp -NoUBA`（单文件原生编译），但Build.bat明确报告已有构建脚本正在运行，等待约5分钟仍未开始该项编译；主动停止的仍仅为本会话Job，其他会话构建未触碰。故本轮新增C++无最新完整编译/链接通过证据，后续必须在编译独占窗口复验。
- 本轮**尚未签收**：新增雪苔MI在新手村真实标记岩石的可见性（现有根地图未保证具备标记组件）、Client/Server正式完整Build、双方最新Cook+IoStore审计、真实三态GPU画面/帧耗、双客户端联机及人工视觉验收。

## 接续安全执行顺序

首先完成 `DivineBeastsVillageMossSurface.cpp`（Village标签式苔藓适配）与原雪层桥的原生编译，修复真实诊断错误。随后在单独真实UE5.8编辑器中验证新增材质MI、标签网格与雪量/湿润MPC，解决SceneCapture纹理流送/渲染帧黑图；最后对正式Client与Server分开进行构建、Cook、Stage与UnrealPak容器检查，并提交人工效果与GPU对照。并行PCG/UI/VFX变更保留，不在本轮重置、清理或强制提交远程仓库。
