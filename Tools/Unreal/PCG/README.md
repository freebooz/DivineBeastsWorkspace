# PCG金标准UE5.8资产创作工具

这里的脚本只能通过真正的UE5.8编辑器创建资源，不创建文本伪uasset/umap，不覆盖已有资源。

RunGoldSpatialProbe.ps1（已保存关卡只读空间探针入口）：仅在没有其它UE编辑器或UE构建进程、且PCGEditor DLL晚于探针源码时，在独立UE5.8进程中执行`-Stage=SpatialProbe（只读空间探针阶段）`；检查真实Gold地图中的Director、11个参与者、3份空间快照图以及道路/农田/桥梁/排除区样本。它不生成网格、不保存地图、不替代G01～G16；运行前可用`Tests/Architecture/VerifyPCGGoldSpatialProbe.py（空间探针静态门禁）`检查探针代码与入口的一致性。

InventoryGoldAssets.py（金标准资产文件审计）：只读核验已真实落盘的17个Definition（数据定义）、3份Realized Graph（实例化生成图）、3份Profile（配置）与1张地图，记录每份文件的长度与SHA-256。可选`--report Saved/Validation/GamePlatformPCG/GoldAssetsInventory_20261010.json`（审计报告）；只证明文件完整性，不替代AssetRegistry（资产注册）、图执行、NavMesh（导航网格）或G01～G16验收。

AuthorGoldLevelDefinitions.py（数据定义生成工具）：为M1测试关卡提供17个资产，包括3个MeshSet（网格集合）、2个SpawnPolicy（生成策略）、3个Layer（生成层）、道路/围栏/农田/连接件等定义。默认只读清单；PCG_GOLD_DEFINITION_MODE=apply仅在UE5.8编辑器中启用。

AuthorGoldLevelMap.py（金标准地图生成工具）：目标是 /Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1，按照G01～G16的场景素材要求放置一个WorldDirector（世界编排器）、道路/小径/田篱样条、闭合地块、森林、资源、桥、门和排除区。默认只读；PCG_GOLD_MAP_MODE=apply仅在UE5.8编辑器中启用。

RunGoldLevelAuthoring.ps1（统一创作入口）：真实UE命令行先创建12个基础模板与7个子图，再创建11类Village（新手村）放置器Blueprint（蓝图），随后依次创建17个测试Definition（定义）、3份实际加权网格生成图及3份配置、独立开发地图和全新UE进程复核。脚本使用不重复的验证日志目录，明确拒绝占用、覆盖和并行UE编辑器写入。

AuthorGoldRealizedGraphs.py（实际网格生成图创建工具）：在Foundation模板和17项Definition真实保存后，使用已加载的MeshSet与GamePlatformData主资产ID调用平台编辑器接口，为森林、岩石和作物创建三份含StaticMeshSpawner（静态网格生成器）的Graph Instance（图实例）与三份Profile（配置定义）。默认只读，正式执行需要UE5.8+PCG_GOLD_REALIZE_MODE=apply；不冒充道路/围栏复杂组合资产已生产。

GamePlatformPCGGoldAssetsCommandlet（平台PCG原生金标准资产命令）：编译在既有`GamePlatformPCGEditor（平台PCG编辑器模块）`中，阶段为`-Stage=Definitions（17项定义）`、`-Stage=Realized（3份实际网格图及配置）`、`-Stage=Map（独立测试地图及空间规则）`、`-Stage=Verify（全新编辑器进程只读回读）`。默认创作入口使用C++原生命令绕开Python插件初始化；需显式`-UsePythonGoldCreation`才用Python兼容方案。所有地图与资源使用UE真实资产格式、不接受任意保存目录。能编译不等于已执行和通过全部阶段。

RunGoldSpatialProbe.ps1（PCG金标准空间规则只读探针）：新版编辑器命令新增`-Stage=SpatialProbe`，独立加载真实`PCG_GoldLevel_M1.umap`后读取11个来源Actor和3份已保存的Realized Graph（实例化图），核对道路/小径/作物/门桥/排除区的Mask（空间掩码）优先级、强度、顶点与图节点快照一致性，**不执行PCG Generate（实际生成）或修改地图**。入口先检验UE5.8版本、地图与DLL存在、DLL构建时间晚于源码，并拒绝任何正在运行的UE编辑器或UBT构建；验证退出码和独有成功日志才记录探针成功。

    ./Tools/Unreal/PCG/RunGoldSpatialProbe.ps1 -EngineRoot $env:UE_ROOT

空间数学探针通过不等于G01～G16实际网格生成、Cook（资源烘焙）或专用服务器权威验证通过。

ValidateGoldLevelAssets.py（独立编辑器重开校验工具）：在全新UE编辑器进程中校验12模板、7子图、11蓝图、17定义、3份真实Spawner图实例、3份Profile和1张UWorld地图的实际存在、反射类型和最低参与者数量。检查脚本只认真实AssetRegistry（资产注册）结果，不声称已执行G01～G16空间生成验收。统一入口第6阶段调用该工具。

InventoryFoundationAssets.py（已有基础资产完整性工具）：核对12模板、7子图、11个真实蓝图文件的批准名称、长度、SHA-256；默认只读，可使用`--report Saved/Validation/GamePlatformPCG/FoundationInventory_20261010.json`（证据路径）保存报告。统一入口启用`-ResumeApprovedPCGAssets`（受控续接）或`-RepairGeneratedFoundations`（受控修复）时会先执行该检查，拒绝覆盖已有资产。该清单不能取代独立UE Editor对资源类型和执行行为的验收。

使用命令（EngineRoot为固定UE5.8路径）：

    $env:UE_ROOT = 'F:\UnrealEngine-5.8.0-release'
    ./Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1 -EngineRoot $env:UE_ROOT
    ./Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1 -EngineRoot $env:UE_ROOT -Apply

成功创建资产仍不等于Gold Level实际PCG生成/导航/碰撞/G01～G16/专用服务器和Client/Server Cook（双端资源烘焙）验收通过。以上验证必须在真实编辑器独立重开后单独执行，且只能对本脚本自己创建的资产进行受控修改。
