# PCG金标准UE5.8资产创作工具

这里的脚本只能通过真正的UE5.8编辑器创建资源，不创建文本伪uasset/umap，不覆盖已有资源。

AuthorGoldLevelDefinitions.py（数据定义生成工具）：为M1测试关卡提供17个资产，包括3个MeshSet（网格集合）、2个SpawnPolicy（生成策略）、3个Layer（生成层）、道路/围栏/农田/连接件等定义。默认只读清单；PCG_GOLD_DEFINITION_MODE=apply仅在UE5.8编辑器中启用。

AuthorGoldLevelMap.py（金标准地图生成工具）：目标是 /Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1，按照G01～G16的场景素材要求放置一个WorldDirector（世界编排器）、道路/小径/田篱样条、闭合地块、森林、资源、桥、门和排除区。默认只读；PCG_GOLD_MAP_MODE=apply仅在UE5.8编辑器中启用。

RunGoldLevelAuthoring.ps1（统一创作入口）：真实UE命令行先创建12个基础模板与7个子图，再创建11类Village（新手村）放置器Blueprint（蓝图），随后依次创建17个测试Definition（定义）、3份实际加权网格生成图及3份配置、独立开发地图和全新UE进程复核。脚本使用不重复的验证日志目录，明确拒绝占用、覆盖和并行UE编辑器写入。

AuthorGoldRealizedGraphs.py（实际网格生成图创建工具）：在Foundation模板和17项Definition真实保存后，使用已加载的MeshSet与GamePlatformData主资产ID调用平台编辑器接口，为森林、岩石和作物创建三份含StaticMeshSpawner（静态网格生成器）的Graph Instance（图实例）与三份Profile（配置定义）。默认只读，正式执行需要UE5.8+PCG_GOLD_REALIZE_MODE=apply；不冒充道路/围栏复杂组合资产已生产。

ValidateGoldLevelAssets.py（独立编辑器重开校验工具）：在全新UE编辑器进程中校验12模板、7子图、11蓝图、17定义、3份真实Spawner图实例、3份Profile和1张UWorld地图的实际存在、反射类型和最低参与者数量。检查脚本只认真实AssetRegistry（资产注册）结果，不声称已执行G01～G16空间生成验收。统一入口第6阶段调用该工具。

使用命令（EngineRoot为固定UE5.8路径）：

    $env:UE_ROOT = 'F:\UnrealEngine-5.8.0-release'
    ./Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1 -EngineRoot $env:UE_ROOT
    ./Tools/Unreal/PCG/RunGoldLevelAuthoring.ps1 -EngineRoot $env:UE_ROOT -Apply

成功创建资产仍不等于Gold Level实际PCG生成/导航/碰撞/G01～G16/专用服务器和Client/Server Cook（双端资源烘焙）验收通过。以上验证必须在真实编辑器独立重开后单独执行，且只能对本脚本自己创建的资产进行受控修改。
