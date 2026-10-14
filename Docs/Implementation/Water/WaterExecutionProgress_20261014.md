# SSLM水体系统 P0—P10 工程执行记录（2026-10-14）

本报告只按当前远程工作区已核实的文件、执行日志和实际状态列项。它不是最终视觉验收证明，不自动触发编译/关卡保存。

## 一、已完成的源码与资源制作

- P0（现状审查）：已确认 UE5.8 原生 Water/WaterAdvanced 存在。已有 GamePlatformWeather（天气）、GamePlatformSurface（材质表面）、GamePlatformPCG（程序化生成）、GamePlatformVFX（视觉特效）应复用。
- P1（双端水域）：已建立 GamePlatformWaterRuntime（平台水域运行）、GamePlatformWaterClient（水体客户端）和 GamePlatformWaterEditor（编辑器）三个模块。含地图静态水域Actor、Box范围深度/速度查询、涉水状态低频复制及项目角色的 WaterInteractor（涉水组件），没有服务器GPU流体仿真。新增加了失效WeakObject水域清理、同优先级时按深度和稳定RegionId字典序选择的顺序保障。
- P2（材质）：远程文件实查10个材质函数（含已有ShoreFoam）、5个水体母材质、10个材质实例，合计25个原生水材质资产；另有4张1024x1024 RGBA PNG纹理原稿（水面法线、岸线泡沫、瀑布流向、水下焦散），由 `Tools/Water/GenerateWaterSourceTextures.py`（纹理原稿生成器）产生并附SHA256清单，**PNG尚未通过Monolith导入为Texture2D**。
- P3（瀑布/水面特效）：已有4个保存的NiagaraSystem（Niagara特效系统）和1个 `BP_GP_WaterfallVisual`（瀑布表现蓝图）原生资产。其组件材质设置和实际场景可见性尚需编辑器回读；资产存在不能替代Shader编译与视觉验证。
- P6（客户端/服务器涉水）：已有权威水域状态复制、客户端水波预算与QualityTier（画质档位）；新增从UE视觉特效设置读取初始质量档位，不采用Tick轮询。项目 `FDivineBeastsPresentationProjectCatalog::BuildWaterVFXFragment`（水体语义目录生成）将 `Presentation.Water.Ripple.VFX`（涟漪）及 `Presentation.Water.Splash.VFX`（飞溅）分别映射到 `platform.water.ripple@1` 和 `platform.water.splash@1`（逻辑定义），`ActivateWaterVisuals`（项目内容包激活）仅按GamePlatformData真实定义的预载成功与否决定是否发布目录。两份正式VFX定义尚未生成，不能声称角色涉水已可见。
- P8（预算）：高/中/低/电影档位代码已经存在；性能指标尚无真实GPU帧时或移动设备数据，不能标成达到2ms等目标。
- P10（文档）：`WaterAssetRegistry_V1.json`（资产目录）、`WaterReviewSceneManifest_V1.json`（待制作审核地图布置合同）、`WaterVFXDefinitionAuthoring_V1.json`（待制作VFX定义合同）、本报告和插件README已写入。

## 二、现场验证结果

1. 首次远程静态审核确认30个原生水体资产（10函数+5母材质+10实例+4Niagara+1瀑布蓝图）存在、插件服务器目标隔离无明显违例，静态检查0问题。
2. 增加4张PNG源图及SHA256记录后，远程 `ValidateGamePlatformWaterAssets.py`（水体资源静态检查）复测结果为 `34个原生资产及PNG源图，0问题`。这是文件与静态依赖层的检查，**不是UE编译或原生纹理导入**。
3. `git diff --check`（空白字符差异检查）在相关已跟踪文件中未报告补丁格式错误，仅提示Windows工作区LF与CRLF的行尾转换警告；新建未跟踪文件仍需最终审核。
4. 恢复Editor所必需的水体模块最小编译 `wc_job_Kq4Nngusr6eMLoCB`（1000s）与单动作重试 `wc_job_1Ja1tOnpZytu-_3F`（900s）均实际超时，没有成功退出码。UBT只输出部分GamePlatformWaterRuntime和Presentation阶段的编译/链接动作，不能声称三个模块DLL已完整生成。
5. Monolith 0.23.0服务进程状态显示healthy（服务已注册），但真实 `monolith_status`（编辑器端状态查询）返回“Unreal Editor not running”。工作区实查仅有WaterEditor DLL，Runtime与Client可加载DLL缺失；P4地图不能安全创建。
6. 由于Monolith编辑器实际不可调用，未使用脚本写出虚构的`.uasset`（原生资产）和`.umap`（原生地图）。

## 三、尚未完成的正式交付

- P2：把4张PNG源图经UE引擎正式导入`GamePlatformSurface`（通用表面材质资源），检查线性/sRGB及压缩分组，完成纹理与Shader视觉审核。
- P3：在实际UE编辑器内回读瀑布蓝图Mesh、材质和4套Niagara系统，必要时修正组件属性/裁剪距离。
- P4：按`WaterReviewSceneManifest_V1.json`（水体审核场景合同）使用Monolith创建`/Game/Development/Water/Maps/L_GP_WaterReview`（水体审核关卡），并放置湖、水域权威体积、河流、瀑布、岸线测试对象。**目前.umap尚不存在**。
- P5：正式新手村`L_Village_Start`（村落地图）禁止不经审核直接修改；必须在P4视觉、性能门禁通过后按Gaea高程与PCG岸线配置移植主湖/3—6瀑布。
- P6：创建两份真实VFXDefinition（平台特效定义数据资产）、项目客户端水体Preset（表现预设资产），通过GamePlatformData预载和中立表现服务将视觉与权威涉水链接，并做双客户端检查。
- P7：Fluid Flux尚未安装，本期主路线不使用；未来只允许PC角色选择/创建前端可选适配，且不带入正式World/专服。
- P8—P10：真实画质档GPU/CPU分析、Editor/Client/Server三目标完整构建、Client+Server最小Cook、多人和反复世界卸载、正式人工视觉验收均没有通过结果。

## 四、当前阻塞与处理顺序

首要阻塞是共享UE5.8编辑器无法加载新水体模块。两次最小模块构建超时，需先复核UBT/UBA（Unreal Build Accelerator，本机构建执行器）的队列、缓存及编译资源竞争；不得未经授权终止其他会话的进程，也不要通过强制清理`Engine/Intermediate`（引擎中间产物）掩盖问题。

**剩余执行顺序：** 恢复正确的UE5.8编辑器及Monolith → 正式导入Texture2D和两个VFXDefinition → 修复瀑布蓝图 → 创建独立水体审核地图并保存回读 → 视觉审核及性能基线 → 拓展Village正式水体 → 最后统一Editor/Client/Server编译、Cook和联机验收。

仅在获得对应工具的真实成功日志后，才把任务状态从“源码/资源已存在”更新为“已运行/已验证”。

## 五、本地前后端重新编译时的水体Cook修复

2026-10-14本轮正式Editor、Client、Server的Win64 Development全目标已实际退出0；Editor使用锁定UE5.8预编译引擎依赖，项目模块仍由UBT编译链接。Editor证据为`c1257527-9652-4360-af35-fb99f25304de`，Client/Server证据为`fa6eace4-ce07-4709-9af6-dda5e2bb62fa`。这些结果更新当前构建状态，不代表水体P1—P10的资源、联机或性能整体验收。

服务器Cook `14a79ea4-1667-4136-84ab-19d9178a2267`实际退出25：新手村水体材质实例和通用瀑布表现蓝图仍被Cook，引用了已NeverCook的Surface材质。Stage阶段整包剥离不足以修复Cook的依赖错误，原失败日志保留。

在既有服务器表现排除真源中增加`/GamePlatformVFX/Water`、`/DBAWorldPack_Village/Water/Materials`与`/DBAWorldPack_Village/Water/Blueprints`。第二次实际Cook进一步发现三项项目视觉预设硬引用已排除的材质/VFX，日志的SourceInstigator均为整个内容目录强制Cook。水面视觉预设不承担`GamePlatformWaterRuntime`的权威Region与深度查询，当前地图也未通过这些待迁入预设提供权威几何，因此仅允许这两个已核验叶子；Village包根、Water父目录、地图、权威水域定义与权威技能仍禁止剥离。权威Region不能新增在该视觉Blueprints目录。通过既有生成器同步三份服务器Game配置和专服Pak规则，未修改二进制资产、客户端水材质或权威水域运行代码。新增回归先观察到真实漏项失败，修复后完整策略测试31/31通过，漂移检查通过；这些静态结果不替代下一轮真实Cook/Stage。
