# SSLM 新手村 Landscape Grass 原生接入说明（2026-10-28）

## 目标与范围

在 UE5.8 正式项目中采用引擎原生 LandscapeGrassType + LandscapeGrassOutput，在 LS_Grass 图层范围内生成草本及野花；采用地形实例化草系统而不是 PCG/StaticMeshFoliage 冒充 Landscape Grass。花草 StaticMesh 最终由人工在编辑器内选择。此项只影响表现资源，不定义游戏碰撞、伤害或服务端权威实体。

## 唯一实施入口

- 脚本：Tools/Unreal/Terrain/AuthorVillageLandscapeGrassUE58.py；需要编辑器 Python、Landscape 和 FoliageEdit 模块。
- 目标母材质：/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge。
- 两份类型：/DBAWorldPack_Village/Terrain/LandscapeGrass/GrassTypes/LGT_DBA_Village_Grass 与 LGT_DBA_Village_Flowers。
- 图层输入：LandscapeLayerSample(LS_Grass) -> LandscapeGrassOutput(VillageGrass)；同一图层乘 VillageFlowerCoverage（初始0.30）后接 VillageFlowers。
- 首次每类型只有一个 Variety，Grass Mesh 为空；以后重复运行只读校验而绝不覆盖人工选择。
- 草本密度初始100/10平方米，Cull开始2600cm/结束5800cm；野花18/10平方米，Cull开始1800cm/结束4400cm；两类均采用网格采样、0.65抖动、0.8～1.2随机缩放、关闭动态和接触阴影。参数仅为测试初始值，实景 GPU 测试后调整。

## 人工选择操作

打开 UE5.8 内容浏览器，定位 /DBAWorldPack_Village/Terrain/LandscapeGrass/GrassTypes，依次打开 LGT_DBA_Village_Grass 与 LGT_DBA_Village_Flowers。展开 Grass Varieties[0]，在 Grass Mesh 中选择本人认可的本项目花草 StaticMesh；确认其授权来源、卡片数、LOD、双面材质、风动和透明裁剪。保存两份类型。然后检查正式 Landscape 的 LS_Grass 绘制层及对应 LayerInfo，若未绘制，不会生成可见草花；人工决定是否在受控范围内绘制，不自动涂满整个新手村。

## 安全边界和验收

脚本启动前核查编辑器脏包。写正式地形母材质前将原文件按 SHA-256 备份在 Saved/Validation/LandscapeGrass20261028。只追加 GrassOutput 支路，不替换已有雪/湿润/地表纹理等节点，不保存或重建 L_Village_Start，不变更高度、碰撞、导航、PlayerStart。失败不自行删除旧资源或清理缓存。

只在 Editor 中完成 GrassType/表达式保存和回读不代表视觉通过。必须另行验证 LS_Grass 权重、可见实例生成与刷新、不同可视距离、低/中/高画质、GPU开销，以及 Client/Server Cook 隔离。选择带客户端专有内容的 Mesh 时特别注意 Server 依赖闭包，不得将纯表现内容硬带入专用服务器。人工认可前不得发布。

## 真实执行证据

作者输出：Saved/Validation/LandscapeGrass20261028/AuthorResult.json（需核对 Status、MaterialAction、GrassTypeAssets、MeshSlots、BackupSHA256）。此文件只记录当前执行，不可作为 Cook/GPU 或人工视觉通过证据。可通过 Engine Output Log 搜索 SSLM_LANDSCAPE_GRASS_AUTHOR_PASS。
