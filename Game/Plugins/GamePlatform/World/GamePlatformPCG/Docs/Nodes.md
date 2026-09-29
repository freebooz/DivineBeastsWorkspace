# Nodes（PCG自定义节点）

所有节点仍位于 `GamePlatformPCG（共享运行模块）`，不拆出新的 Nodes 模块。

| 节点 | 中文名称 | 当前实现 | 如何使用 | 边界 |
| --- | --- | --- | --- | --- |
| WriteSchemaDefaults | 写协议默认值 | M0已实现 | 放在Classify（分类）早期，为点数据补Schema v1默认字段 | 不写P9 Mutable.Id |
| PriorityCarve | 优先级挖洞 | M0点级已实现 | 上游写ExcludeMask，设置Subject/Carver Priority；高优先级且Mask达到阈值时将点Density置0 | 空间Carver几何合并由模板负责 |
| ProjectAlign | 投影对齐 | M0对齐已实现 | SG_ProjectOnLandscape完成投影后，用于KeepVertical保持竖直 | 当前节点本身不执行Landscape射线 |
| ApplySpawnPolicy | 应用生成策略 | M0基础已实现 | 对候选点统一密度和缩放；复杂DA解析由模板/组合层负责 | 不同步加载Definition |
| AssignMeshSet | 分配网格集合 | M0已实现 | 写 `Pcg.Spawn.MeshSetId` 稳定ID | 不直接加载任意网格路径 |
| ValidateSchema | 验证协议 | M0已实现 | 模板输出前或关键边界调用；缺字段则Fail-Safe丢弃输入 | 不修复未知字段 |
| FitPostsToSpline | 样条布柱 | M1点级基础已实现 | 先由模板采样样条，再按PostSpacing过滤候选点 | 不负责原始Spline采样与转角加柱 |
| BreakSpansByTags | 按标签打断跨度 | M1基础已实现 | 对带BlockingTag的数据阻断输出；空间交叉标签由模板产生 | 不自动做道路/河流几何求交 |
| BuildRows | 生成垄线 | M1点级基础已实现 | 地块模板先产生内部候选点，节点按RowSpacing/Yaw吸附成垄 | Polygon裁剪仍归TPL_CropField |
| SelectSpanMeshByLength | 按跨度选择栏片集合 | M1已实现 | 上游写 `Pcg.Rule.SpanLength`，按SpanMeshRule选择最窄覆盖MeshSetId | 输出的是 `Pcg.Spawn.MeshSetId`，不是单个StaticMesh |

> `AssignMeshSet（分配网格集合）`和`SelectSpanMeshByLength（按跨度选择栏片）`当前输出的是稳定目录 ID，不代表已经完成真实 StaticMesh Spawner（静态网格生成器）资源解析。真实 `MeshSetId → Definition Lease（定义租约）→ Mesh Selector/Spawner（网格选择/生成）`闭环是生产模板启用前的 P0 阻断项；在此之前 Template Runtime 保持 Unsupported。

M2的 `SideBySlope（按坡向选择侧）`、M3的 `ApplyMutableState（应用可变态）`、M4的 `DetectConnectorCandidates（自动连接件候选）`只保留规划，不创建空实现。
