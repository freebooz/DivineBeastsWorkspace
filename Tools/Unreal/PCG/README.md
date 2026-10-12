# PCG金标准UE5.8资产创作工具

这里的脚本只能通过真正的UE5.8编辑器创建资源，不创建文本伪uasset/umap，不覆盖已有资源。

RunGoldSpatialProbe.ps1（已保存关卡只读空间探针入口）：仅在没有其它UE编辑器或UE构建进程、且PCGEditor DLL晚于探针源码时，在独立UE5.8进程中执行`-Stage=SpatialProbe（只读空间探针阶段）`；检查真实Gold地图中的Director、11个参与者、3份空间快照图以及道路/农田/桥梁/排除区样本。它不生成网格、不保存地图、不替代G01～G16；运行前可用`Tests/Architecture/VerifyPCGGoldSpatialProbe.py（空间探针静态门禁）`检查探针代码与入口的一致性。

InventoryGoldAssets.py（金标准资产文件审计）：只读核验已真实落盘的17个Definition（数据定义）、3份Realized Graph（实例化生成图）、3份Profile（配置）与1张地图，记录每份文件的长度与SHA-256。可选`--report Saved/Validation/GamePlatformPCG/GoldAssetsInventory_20261010.json`（审计报告）；只证明文件完整性，不替代AssetRegistry（资产注册）、图执行、NavMesh（导航网格）或G01～G16验收。

AuthorGoldLevelDefinitions.py（数据定义生成工具）：为M1测试关卡提供17个资产，包括3个MeshSet（网格集合）、2个SpawnPolicy（生成策略）、3个Layer（生成层）、道路/围栏/农田/连接件等定义。默认只读清单；PCG_GOLD_DEFINITION_MODE=apply仅在UE5.8编辑器中启用。

AuthorGoldLevelMap.py（金标准地图生成工具）：目标是 /Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1，按照G01～G16的场景素材要求放置一个WorldDirector（世界编排器）、道路/小径/田篱样条、闭合地块、森林、资源、桥、门和排除区。默认只读；PCG_GOLD_MAP_MODE=apply仅在UE5.8编辑器中启用。

RunGoldLevelAuthoring.ps1（统一创作入口）：真实UE命令行先创建12个基础模板与7个子图，再创建11类Village（新手村）放置器Blueprint（蓝图），随后依次创建17个测试Definition（定义）、3份实际加权网格生成图及3份配置、独立开发地图和全新UE进程复核。脚本使用不重复的验证日志目录，明确拒绝占用、覆盖和并行UE编辑器写入。

AuthorGoldRealizedGraphs.py（实际网格生成图创建工具）：在Foundation模板和17项Definition真实保存后，使用已加载的MeshSet与GamePlatformData主资产ID调用平台编辑器接口，为森林、岩石和作物创建三份含StaticMeshSpawner（静态网格生成器）的Graph Instance（图实例）与三份Profile（配置定义）。默认只读，正式执行需要UE5.8+PCG_GOLD_REALIZE_MODE=apply；不冒充道路/围栏复杂组合资产已生产。

GamePlatformPCGGoldAssetsCommandlet（平台PCG原生金标准资产命令）：编译在既有`GamePlatformPCGEditor（平台PCG编辑器模块）`中，阶段为`-Stage=Definitions（17项定义）`、`-Stage=Realized（3份实际网格图及配置）`、`-Stage=Map（独立测试地图及空间规则）`、`-Stage=Verify（全新编辑器进程只读回读）`。默认创作入口使用C++原生命令绕开Python插件初始化；需显式`-UsePythonGoldCreation`才用Python兼容方案。所有地图与资源使用UE真实资产格式、不接受任意保存目录。能编译不等于已执行和通过全部阶段。



`Map（首次创建地图）`与`RepairMap（受控修复已有地图）`阶段必须显式传入`-BlueprintRoot=已注册包目录`。平台命令没有项目目录默认值，参数缺失、空值、对象路径、非法包名或未挂载根会在创作前失败；目录允许首尾空白、Windows反斜杠和单个可选尾斜杠，经UE5.8的`FPaths::NormalizeDirectoryName`与`FPackageName::IsValidLongPackageName`规范化和校验后用于加载放置器类。`Definitions`、`Realized`、`Verify`、`SpatialProbe`、`GeneratePreview`不要求该参数；中立Gold输出目录、11项领域/形状清单、资源身份与不覆盖规则保持不变。

本项目`RunGoldLevelAuthoring.ps1`仍编排Village固定资产清单，其项目侧`$blueprintRoot`统一提供原`/DBAWorldPack_Village/PCG/Blueprints/`默认值，并同时传给基础蓝图创建和Gold地图阶段。跨项目调用者向平台命令传入自己的已挂载目录；本项目入口没有开放与Village盘点/插件装配不一致的通用覆盖参数。使用原生命令直接创建或修复时需补充该参数，例如`-run=GamePlatformPCGGoldAssets -Stage=Map -BlueprintRoot=/DBAWorldPack_Village/PCG/Blueprints/`。目录存在与包名合法仍不保证所需蓝图存在、父类正确或图资源有效，后续Preflight继续逐项检查真实资源。

路径输入回归位于`GamePlatform.PCG.Editor.GoldAuthoring.BlueprintRoot.Normalization`、`InvalidInputs`与`RequiredStages`三个Editor Automation用例；它们需要真实UE运行证据。`python Tests/Architecture/VerifyPCGGoldAuthoring.py`新增平台无项目挂载点、显式输入链、项目默认注入与阶段范围的静态合同检查，不代替UE编译、资产加载或地图保存重开。

RunGoldSpatialProbe.ps1（PCG金标准空间规则只读探针）：新版编辑器命令新增`-Stage=SpatialProbe`，独立加载真实`PCG_GoldLevel_M1.umap`后读取11个来源Actor和3份已保存的Realized Graph（实例化图），核对道路/小径/作物/门桥/排除区的Mask（空间掩码）优先级、强度、顶点与图节点快照一致性，**不执行PCG Generate（实际生成）或修改地图**。入口先检验UE5.8版本、地图与DLL存在、DLL构建时间晚于源码，并拒绝任何正在运行的UE编辑器或UBT构建；验证退出码和独有成功日志才记录探针成功。

    ./Tools/Unreal/PCG/RunGoldSpatialProbe.ps1 -EngineRoot $env:UE_ROOT
    # 全部Editor C++源码经正式UE5.8编译链接且没有其它编辑器实例时，执行生成预览：
    ./Tools/Unreal/PCG/RunGoldSpatialProbe.ps1 -EngineRoot $env:UE_ROOT -Stage GeneratePreview

`GeneratePreview（实际生成预览）`只对已批准的森林、岩石和作物三份图调用官方PCG，逐组件等待上限60秒，检查已生成ISM（实例化静态网格）的数量上限、碰撞/导航/复制隔离以及空间掩码。执行后必须取消未结束的请求并清理本次拥有的临时生成资源，不保存Gold地图；它也不代替G01～G16完整场景、Server/Client双端Cook或人工画面审核。

`RunGoldDeterminism.ps1（G09双进程确定性验证）`使用上述安全预览入口，在两次独立的UE5.8编辑器进程中读取相同场景与Seed（随机种子），生成并清理森林、岩石、作物，再对三类真实ISM实例数量和网格路径、位置、旋转、缩放的稳定量化指纹做严格对比；不修改/保存地图。它只报告G09在这三份测试图上的结果，不替代其余金标准场景、性能或双端Cook。**必须等最新C++正式Editor DLL编译成功且没有其它编辑器运行后执行**。

    ./Tools/Unreal/PCG/RunGoldDeterminism.ps1 -EngineRoot $env:UE_ROOT

空间数学探针通过不等于G01～G16实际网格生成、Cook（资源烘焙）或专用服务器权威验证通过。

ValidateGoldLevelAssets.py（独立编辑器重开校验工具）：在全新UE编辑器进程中校验12模板、7子图、11蓝图、17定义、3份真实Spawner图实例、3份Profile和1张UWorld地图的实际存在、反射类型和最低参与者数量。检查脚本只认真实AssetRegistry（资产注册）结果，不声称已执行G01～G16空间生成验收。统一入口第6阶段调用该工具。

InventoryFoundationAssets.py（已有基础资产完整性工具）：核对12模板、7子图、11个真实蓝图文件的批准名称、长度、SHA-256；默认只读，可使用`--report Saved/Validation/GamePlatformPCG/FoundationInventory_20261010.json`（证据路径）保存报告。统一入口启用`-ResumeApprovedPCGAssets`（受控续接）或`-RepairGeneratedFoundations`（受控修复）时会先执行该检查，拒绝覆盖已有资产。该清单不能取代独立UE Editor对资源类型和执行行为的验收。

使用命令（EngineRoot为固定UE5.8路径）：

    $env:UE_ROOT = 'F:\UnrealEngine-5.8.0-release'
    ./Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1 -EngineRoot $env:UE_ROOT
    ./Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1 -EngineRoot $env:UE_ROOT -Apply

成功创建资产仍不等于Gold Level实际PCG生成/导航/碰撞/G01～G16/专用服务器和Client/Server Cook（双端资源烘焙）验收通过。以上验证必须在真实编辑器独立重开后单独执行，且只能对本脚本自己创建的资产进行受控修改。
### Village PCG正式资源准入

`VerifyVillagePCGProductionSources.py（新手村PCG生产来源审查）`仅只读检查：正式`gaea_2_3_export（Gaea 2.3真实导出）`、`projectionVersion=2（投影第二版）`、显式道路来源，以及六类正式静态网格映射。通过也不替代UE资产注册、G01～G16或双端Cook。

六个必需领域：`Forest.Canopy（森林乔木）`、`Rock.Scatter（岩石散布）`、`Agri.Crop（作物）`、`Encl.FieldFence（农田围栏）`、`Connector.Gate（门）`、`Connector.Bridge（桥梁）`。前三者允许`ClientCosmetic（客户端纯装饰）`；后三者必须`StaticWorld（静态世界碰撞）`并接受服务器权威与导航审查。

项目美术批准的清单路径：`DBAWorldPack_Village/SourceArt/PCG/VillagePCGMeshBindings_v1.json`，每项须包含`domainId（领域ID）`、`meshAssetPath（真实Village挂载路径）`、`sha256（来源校验值）`、`usage（用途）`。必须指向真实、非空、已批准的UE静态网格资产，不得用Foundation或Engine BasicShapes占位、空清单或假uasset绕过。

运行命令：`python Tools/Unreal/PCG/VerifyVillagePCGProductionSources.py --report Saved/Validation/GamePlatformPCG/VillageProductionSourceReadiness_20261012.json`。

2026-10-12实际执行返回退出码2，按预期拦截技术校准来源、尚无已审定道路来源和缺少六类项目网格。门禁失败意味着**正式PCG生产资源尚未满足最低条件**，不应进入Shipping或Server Cook；报告不是UE实际生成验收。
### 不需要Gaea的Village PCG技术网格源与UE编辑器导入

项目技术模型生成器：`GenerateVillagePrototypeMeshes.py（技术三维模型源生成）`，首次`--apply`只允许新目录，后续`--verify`严格复核6份厘米制OBJ、1份MTL和1份JSON来源清单SHA-256。模型包含乔木、岩石、作物、田篱、农门和木桥，只有技术样板质量，不能当作3A正式美术或Gaea输入。

```
python Tools/Unreal/PCG/GenerateVillagePrototypeMeshes.py --verify
# 以下默认只读检查，不启动编辑器：
./Tools/Unreal/PCG/RunVillagePrototypeImport.ps1 -Mode Inspect
```

`ImportVillageTechnicalPrototypes.py（真实StaticMesh导入和回读）`只能在真正UE5.8 Editor运行，`VILLAGE_PCG_MESH_IMPORT_MODE=apply/verify`，仅指向项目挂载点`/DBAWorldPack_Village/Development/PCG/Meshes`。命名采用`SM_DBA_PCG_<Role>_Tech（项目PCG技术网格）`，拒绝已有资产覆盖、未知类型、尺度异常及缺少LOD0；`AuthorVillagePrototypeReviewMap.py（技术模型审查地图）`提供独立非正式地图`/DBAWorldPack_Village/Development/PCG/Maps/L_DBA_PCG_TechMeshReview`的真实UE创建/回读代码。需要共享Editor/UBT完全空闲方可执行，不从普通Python写假uasset/umap。

正式Cook在`DefaultGame.ini`明确排除`/DBAWorldPack_Village/Development/PCG`，生产来源审查拒绝`Development/`和`_Tech`网格，即使技术源文件存在并哈希正确也不得标记已获美术批准。正式Village仍需真实高品质模型、地形来源、服务器碰撞/导航、人工审查和双端Cook。

技术导入器将通过UE5.8 `StaticMeshEditorSubsystem（静态网格编辑器子系统）`核对/清除OBJ导入器可能自动产生的简单碰撞；独立审查地图中的六个技术Actor统一设置`NoCollision（无碰撞）`。尤其是门洞和木桥禁止被一个完整包围盒阻挡。源定义中的`StaticWorld（未来静态世界用途）`**不表示**已完成服务端碰撞、NavMesh（导航网格）、分段围栏和桥面权威验证；这些必须用正式Gameplay碰撞资源单独验收。

同一个受控脚本还提供`-Mode ReviewApply（创建真实技术审查地图）`和`-Mode ReviewVerify（全新UE进程回读独立地图）`，仅在六项技术StaticMesh均已导入、没有编辑器/UBT占用时运行。前者拒绝已存在目标地图，后者拒绝缺失目标地图，不触碰正式新手村地图或既有GoldLevel。

```powershell
# UE5.8独占环境下顺序执行，任一步失败即停止
./Tools/Unreal/PCG/RunVillagePrototypeImport.ps1 -EngineRoot $env:UE_ROOT -Mode Apply
./Tools/Unreal/PCG/RunVillagePrototypeImport.ps1 -EngineRoot $env:UE_ROOT -Mode Verify
./Tools/Unreal/PCG/RunVillagePrototypeImport.ps1 -EngineRoot $env:UE_ROOT -Mode ReviewApply
./Tools/Unreal/PCG/RunVillagePrototypeImport.ps1 -EngineRoot $env:UE_ROOT -Mode ReviewVerify
```
### 一键无Gaea开发验证顺序（只执行已有资源，不修改正式Village）

`RunNoGaeaPCGDevelopment.ps1（无Gaea PCG开发入口）`默认只读：先核对六份真实OBJ源、12份基础模板/7份子图/11份蓝图和17份Gold定义/3图/3配置/1地图。`-Apply`仅在锁定UE5.8、没有其他编辑器/UBT时按**资源优先、验收最后**的顺序执行：六网格OBJ→真实StaticMesh导入与回读→独立技术地图创建/重开→Gold SpatialProbe（空间探针）→GeneratePreview（官方真实PCG生成）→G09两独立进程确定性；最后可选`-NativeTests（原生PCG测试）`。

```powershell
# 默认只读审核，退出码0仅表示技术源/Gold文件准备
./Tools/Unreal/PCG/RunNoGaeaPCGDevelopment.ps1
# 无其他Editor/UBT占用时执行真正UE创建和验证
./Tools/Unreal/PCG/RunNoGaeaPCGDevelopment.ps1 -EngineRoot $env:UE_ROOT -Apply
```

每项都有独立退出码和日志，综合报告写入`Saved/Validation/GamePlatformPCG/NoGaeaDevelopment/<新运行ID>/result.json`。安全门禁、资源导入、空间探针或实际生成任一失败均停止后续步骤。**即使全部技术步骤通过仍退出2**，明确G01～G16完整矩阵、正式Client/Server Cook、性能/导航和Gaea/3A美术尚需独立验收，不可用技术开发完成代替生产完成。
