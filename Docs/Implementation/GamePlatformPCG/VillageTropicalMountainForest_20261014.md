# SSLM 新手村外围山峰原始热带森林实施记录（2026-10-14）

## 目标、范围与资源归属

保留中心湖泊、湖心三岛、桃花新手村、农田、桑林、村庄和道路。北、东、西外围山峰设计为高大常绿乔木密布、阔叶灌木与蕨类丰富的原始热带雨林，形成隐蔽的新手村外围屏障。不得覆盖玩家出生位置、教学/训练区域及水体瀑布。

统一复用第一层 `GamePlatformPCG`（平台PCG通用机制）；不创建第二套PCG插件，不引入MOBA层依赖。第三层 `DBAWorldPack_Village`（项目世界内容包）持有世界生态参数与原生PCG图；正式地图依旧为 `/DBAWorldPack_Village/Maps/L_Village_Start`，目前未修改此关卡。

## 官方 Electric Dreams 资源

来源：`https://www.fab.com/listings/d79688f5-29be-4fb2-a650-2d4a813f5306`（Epic Games Fab免费示例）。
官方文档：`https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine`。
正式森林示例：`/Game/Levels/PCG/Breakdown_Levels/ElectricDreams_PCGForest`（参数化森林关卡），`/Game/PCG/Graphs/Forest/PCGDemo_Forest`（PCG森林原始示例图）。

此资源为 UE-Only（限虚幻引擎产品）；Epic说明可用于商业虚幻引擎项目。本机Fab/Epic VaultCache内未发现原始示例工程，当前未获得可迁移的热带雨林树木、材质与纹理，不能将配置或工具描述成官方资源已导入。

推荐取得完整样例工程后：Epic/Fab资源库加入样例 → 在Launcher（启动器）创建原始工程 → 独立打开并核对完整依赖 → 通过引擎Migrate（迁移）将选中的网格、材质、贴图、植物风动资源迁至隔离暂存工程 → 在UE5.8中受控迁入 `/DBAWorldPack_Village/PCG/Forest/ElectricDreams/` 并修正引用。不得直接覆盖SSLM已有 `/Game` 内容及正式地图。

## 当前真实制作结果

- `SourceArt/PCG/TropicalMountainForest_v1.json`（雨林分层、山体区域和排除带合同）。
- `Tools/Unreal/PCG/AuthorVillageTropicalMountainForest.py`（UE编辑器三份源点图生成与校验）。
- `Tests/Architecture/ValidateVillageTropicalMountainForest.py`（纯静态资源结构门禁）。
- `Content/PCG/Graphs/PCG_DBA_Village_OuterTropicalCanopy_Source.uasset`（冠层候选点）。
- `Content/PCG/Graphs/PCG_DBA_Village_OuterTropicalUnderstory_Source.uasset`（中层候选点）。
- `Content/PCG/Graphs/PCG_DBA_Village_OuterTropicalFloor_Source.uasset`（林下候选点）。

三张真正UPCGGraph（原生程序化生成图）已由正式UE5.8编辑器创建并保存、调用回读验证。每张图包含 GetLandscape、SurfaceSampler、AttributeFilter、NormalToDensity、DensityFilter 五个节点和七条连线。采用有界地表采样、`LS_ForestSoil`（森林土壤权重）及地面坡度阈值。当前为**PCG候选点**，未连接StaticMeshSpawner（静态网格生成节点），没有树木资产和真正山体布置结果。

### 本轮验证证据

- UE5.8正式编辑器通过Monolith真实创建、保存和反射读取三份PCGGraph。已保存的文件大小：冠层91479字节、中层92111字节、林下91926字节；文件SHA-256短指纹分别为 `d2d4ea08a21e0b35`、`7f3caf92d5bd7bdf`、`d0c17b62e6c9bc27`（按本轮磁盘读取，不作为后续修改后的不变值）。
- `python Tests/Architecture/ValidateVillageTropicalMountainForest.py`（外围雨林静态合同）执行通过。
- `python Tests/Architecture/VerifyPCGGoldAuthoring.py`（平台PCG金标准合同）执行通过。
- `python Tests/Architecture/ValidateVillagePCGTechnicalMeshes.py`（新手村技术网格开发/非发布隔离）执行通过。
- `Build/Validation/VerifyPCGArchitecture.ps1`（PCG三层边界与插件架构）执行通过。
- UE5.8 Editor Automation（编辑器原生测试）已运行：`GamePlatform.PCG.Schema.V1Contract`（元数据Schema）、`GamePlatform.PCG.Spatial.CarveGeometryAndPriority`（空间排斥与优先级）、`GamePlatform.PCG.Metadata.AssignMeshSetToPoints`（逐点网格身份赋值），**3项均通过**。
- 未执行新森林资产的独立冷启动/PCG最终生成/正式地图保存/Cook/性能/人工画面验收；源码与原生源点图的合格不能代表上述任务完成。

## 后续质量关口

1. 取得官方样例并完成UE资产迁移、来源/许可/引擎版本和依赖审计。
2. 三层实际热带植物MeshSet接入，依冠层、中层、林下分区设置密度、朝向、风动和最小距离。
3. 在真实新手村三侧山体放置有界PCG区域并接线，按湖岸、道路、建筑出生点、农田、瀑布及危险山崖的空间过滤剔除无效点。
4. 真实UE5.8执行PCG、编辑器烘焙、保存地图、冷启动加载、NavMesh（导航）与碰撞复核。
5. 分别审查客户端和Dedicated Server（专用服务器）Cook的资源剥离；检查HISM（分层实例化）、Nanite（虚拟几何）、HLOD（层级细节）、Lumen（动态光照）与GPU/内存负担。
6. 人工视觉核验：远景山体遮蔽、树冠密度、近景叶片、林下层、树根贴地、瀑布与道路退让。

上述第1～6关口未获实际证明前，不得标记“新手村原始森林已导入并完成”。
## 2026-10-14 Electric Dreams 官方资产迁移安全门禁（增量）

本机实际扫描：`D:/UnrealSamples` 未发现经过 Epic Games Launcher（Epic启动器）创建的原始工程；现有 Epic VaultCache（资源缓存）未发现 Electric Dreams；锁屏状态下不能通过启动器完成 Fab（官方资源市场）账号授权与下载。此事实不影响已存在的三张原生PCG候选点图，但**官方素材未下载、未迁移、未生成新手村原始森林**。

GitHub `freebooz/DivineBeastsWorkspace` 是公开仓库，Fab条目明确为 `UE-Only`（限虚幻引擎产品）素材。项目增加 `.gitignore` 定向隔离，只排除第三方源资源：
- `Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Environment/ThirdParty/ElectricDreams/`（第三层本地持有的获许可雨林资产）；
- `Game/Content/ExternalImports/ElectricDreams/`（独立迁移审查目录，禁止作正式内容）。
项目自有PCG图、数据配置、脚本、中文审核文档仍然版本管理。不得将第三方完整源资源提交到公开仓库，不得重新打包成供独立使用的资源下载，也不得未经许可上传给模型训练。

`Tools/Unreal/PCG/InspectElectricDreamsSample.py`（Electric Dreams离线样例来源预检工具）只读核验官方森林关卡实际UE包头、工程描述、资产数量和上述Git隔离；不读取凭据，不下载/修改第三方二进制，不改变正式 `L_Village_Start`（新手村地图）。对应 `Tests/Architecture/TestElectricDreamsMigrationPreflight.py`（离线安全合同测试）。

```powershell
# 用户先在Fab官方资源库领取，并通过Epic Games Launcher执行Create Project，保存至D:\UnrealSamples。
# 无源工程时应退出2；此结果表示正确阻断而非导入成功。
python Tools/Unreal/PCG/InspectElectricDreamsSample.py --sample-root D:\UnrealSamples --require-source
python Tests/Architecture/TestElectricDreamsMigrationPreflight.py
```

即使样例文件存在，`source_on_disk`（磁盘来源具备必要结构）仍**不**表示 Epic Fab 账号授权已核实或 AssetRegistry 实际加载成功。应在独立的已获授权 Electric Dreams 工程中用UE Editor（虚幻编辑器）读取 `ElectricDreams_PCGForest`（官方参数化森林关卡）及其PCG图、网格、材质和纹理依赖，再通过引擎 `Migrate`（迁移）进行引用闭包迁移；禁止从文件管理器直接剪切/拷贝散落的 `.uasset`。原始 `/Game` 资源应先在独立临时UE工程中进行命名空间整理，避免直接覆盖SSLM `/Game`。在UE编辑器中正确修复软硬引用并确认目标持有者是 `/DBAWorldPack_Village/Environment/ThirdParty/ElectricDreams` 后，才能执行正式迁移。

迁移后才允许填写 `TropicalMountainForest_v1.json`（雨林分层合同）的真实 `meshPaths`（静态网格软引用）与实际来源位置，再创建项目层StaticMeshSpawner（静态网格生成器）、外围山峰有界PCG区域和湖泊/道路/农田/出生点/瀑布排除规则。先完成编辑器烘焙与冷重载，然后分别审计客户端和Dedicated Server（专用服务器）的Cook（资产烘焙）、HLOD（层级细节）、NavMesh（导航）和性能，才能将 `status.releaseReady` 改为真实发布状态。公开仓库无法再分发第三方原文件，协作者应使用各自有权访问的Fab来源或受控内部资产存储完成本地构建。

**当前结果：** 静态准入脚本与5项离线回归通过，Git第三方资源隔离通过；真实示例工程缺失的正式迁移门禁返回退出码2。没有任何官方原始资源被宣称已导入，不触碰正式关卡或其它会话未提交文件。
### 2026-10-15 Electric Dreams 森林源资产UE原生依赖审计工具（待官方工程解锁后运行）

`Tools/Unreal/PCG/AuditElectricDreamsForestSourceUE58.py`（独立官方样例工程内的只读资产注册表盘点器）使用真实UE的AssetRegistry（资产注册表）对 `/Game/Levels/PCG/Breakdown_Levels/ElectricDreams_PCGForest`（官方森林示例小关卡）与 `/Game/PCG` 内现有Forest图进行软硬引用闭包扫描，分别记录静态网格、材质、纹理、PCG子图及外部插件依赖。明确禁止在SSLM主工程作为源项目运行；不拷贝二进制资产、不修改原样例、不下载、不证明Fab账号授权。源工程必须先真实下载到 `D:/UnrealSamples`。

```powershell
# 只读查看工具目标，普通Python可运行，不产生任何UE资产
python Tools/Unreal/PCG/AuditElectricDreamsForestSourceUE58.py --mode inspect
# 正式审计在已安装的ElectricDreamsEnv独立UE工程内部通过-ExecutePythonScript执行：
# AuditElectricDreamsForestSourceUE58.py --mode audit
# 写入源工程Saved/Validation/ElectricDreamsSource/下独立JSON报告，仅盘点真实AssetRegistry引用。
python Tests/Architecture/TestElectricDreamsRegistryAudit.py
```

新增加的 `Tests/Architecture/TestElectricDreamsRegistryAudit.py`（离线四场景边界回归）只证明合法/非法包路径及森林图匹配等代码逻辑正确，不能替代实际官方样例加载。实测SSL​​M开发机仍为Windows锁屏且 `D:/UnrealSamples` 为空，真实迁移、StaticMeshSpawner连接、外山体 PCG Actor、正式森林Bake、双Cook与人工视觉均保持未完成。
### 2026-10-15 官方MigratePackages（原生迁移）隔离执行入口

`StageElectricDreamsForestUE58.py`（Electric Dreams森林源工程迁移编排器）采用UE5.8真实 `AssetTools.migrate_packages`（资产包与依赖迁移），**只允许源工程 → 独立空临时工程**，禁止把样例直接拷贝或迁移至 `DivineBeastsWorkspace/Game/Content`。固定输入为 `ElectricDreams_PCGForest`（森林示例关卡）和 `PCGDemo_Forest`（森林生成主图），引擎自动解析并保留两者依赖。迁移策略 `AssetMigrationConflict.SKIP`（冲突时跳过），若临时工程已经存在UE资产则拒绝执行，避免不完整覆盖。

一旦官方示例通过Epic Games Launcher（Epic启动器）真正下载，先用UE空工程模板创建 `D:/UnrealSamples/SSLM_ElectricDreams_Staging/SSLM_ElectricDreams_Staging.uproject`（迁移隔离工程，路径只是约定，不代表已创建），再在已获授权且已备份的 **Electric Dreams 源工程编辑器**中执行以下参数：

```powershell
# 离线只读规划，无需UE运行：
python Tools/Unreal/PCG/StageElectricDreamsForestUE58.py --mode inspect
# 真正UE5.8源工程内执行的脚本参数示例，需通过编辑器ExecutePythonScript调用：
# StageElectricDreamsForestUE58.py --mode apply --stage-project D:/UnrealSamples/SSLM_ElectricDreams_Staging/SSLM_ElectricDreams_Staging.uproject --acknowledge-ue-only-license
# 在源工程中执行的只读阶段核实：
# StageElectricDreamsForestUE58.py --mode verify --stage-project D:/UnrealSamples/SSLM_ElectricDreams_Staging/SSLM_ElectricDreams_Staging.uproject
# 普通Python安全合同测试：
python Tests/Architecture/TestElectricDreamsForestStaging.py
```

迁移脚本检查源、目标均为独立UE工程，源工程包含官方森林地图及Graph的实际AssetRegistry记录，迁移后检查两份UE包头；引擎 `migrate_packages` 没有可靠成功返回值，禁止仅凭函数返回或CLI退出码标记真实资源导入。真正成功迁移后仍需打开临时工程独立验证，审核授权、重定向资源、风动材质与所有依赖，在引擎中重定位到 `DBAWorldPack_Village`（新手村项目世界内容包），并按原合同执行双Cook、PCG烘焙与人工验收。

**当前事实：** 源样例未下载，以上引擎 `apply` 和 `verify` 尚未在真实样例执行；新脚本的6项离线边界测试通过，并不能替代实际迁移。
