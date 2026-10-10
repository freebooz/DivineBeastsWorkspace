# Nodes（PCG自定义节点）

所有节点仍位于 `GamePlatformPCG（共享运行模块）`，不拆出新的 Nodes 模块。

| 节点 | 中文名称 | 当前实现 | 如何使用 | 边界 |
| --- | --- | --- | --- | --- |
| WriteSchemaDefaults | 写协议默认值 | M0已实现 | 放在Classify（分类）早期，为点数据补Schema v1默认字段 | 不写P9 Mutable.Id |
| PriorityCarve | 优先级挖洞 | M0点级已实现 | 上游写ExcludeMask，设置Subject/Carver Priority；高优先级且Mask达到阈值时将点Density置0 | 空间Carver几何合并由模板负责 |
| ProjectAlign | 投影对齐 | M0对齐已实现 | SG_ProjectOnLandscape完成投影后，用于KeepVertical保持竖直 | 当前节点本身不执行Landscape射线 |
| ApplySpawnPolicy | 应用生成策略 | M0基础已实现 | 对候选点统一密度和缩放；复杂DA解析由模板/组合层负责 | 不同步加载Definition |
| AssignMeshSet | 分配网格集合 | M0基础已实现；逐点写入缺陷已修复，UE回归待验证 | 在既有Schema默认属性上逐点写入 `Pcg.Spawn.MeshSetId` 稳定ID | 不同步加载网格；空ID及元数据写入失败则丢弃输出 |
| ValidateSchema | 验证协议 | M0基础实现；2026-10-10补齐核心字段、存储类型及未知字段失败关闭，UE回归待验 | 模板关键边界验证核心Schema、显式字段和真实PCG Metadata类型 | 不修复未知字段；Guid存储验证前拒绝提前写入 |
| FitPostsToSpline | 样条布柱 | M1点级基础已实现 | 先由模板采样样条，再按PostSpacing过滤候选点 | 不负责原始Spline采样与转角加柱 |
| BreakSpansByTags | 按标签打断跨度 | M1基础已实现 | 对带BlockingTag的数据阻断输出；空间交叉标签由模板产生 | 不自动做道路/河流几何求交 |
| BuildRows | 生成垄线 | M1点级基础已实现 | 地块模板先产生内部候选点，节点按RowSpacing/Yaw吸附成垄 | Polygon裁剪仍归TPL_CropField |
| SelectSpanMeshByLength | 按跨度选择栏片集合 | M1已实现 | 上游写 `Pcg.Rule.SpanLength`，按SpanMeshRule选择最窄覆盖MeshSetId | 输出的是 `Pcg.Spawn.MeshSetId`，不是单个StaticMesh |

> `AssignMeshSet（分配网格集合）`和`SelectSpanMeshByLength（按跨度选择栏片）`当前输出的是稳定目录 ID，不代表已经完成真实 StaticMesh Spawner（静态网格生成器）资源解析。真实 `MeshSetId → Definition Lease（定义租约）→ Mesh Selector/Spawner（网格选择/生成）`闭环是生产模板启用前的 P0 阻断项；在此之前 Template Runtime 保持 Unsupported。

2026-10-10补充：AssignMeshSet已增加逐点MeshSetId赋值操作与UE Automation（引擎自动化）源码用例，检查默认空ID覆盖、重复配置和非法空ID。实际 UE 构建与测试仍受引擎编译产物限制，不能以源码存在宣布验证通过。

M2的 `SideBySlope（按坡向选择侧）`、M3的 `ApplyMutableState（应用可变态）`、M4的 `DetectConnectorCandidates（自动连接件候选）`只保留规划，不创建空实现。
## 2026-10-10 P2～P7增量源码

- `UGamePlatformPCGSpatialCarveSettings（空间掩码挖洞节点）`：使用WorldDirector预先提取的二维道路、地块、门桥和人工排除几何；严格高优先级覆盖低优先级，同级来源按稳定GUID排序，非法掩码整体安全失败。先准备Bounds缓存，再逐点计算，防止每点重复扫描所有几何合法性。
- `FGamePlatformPCGSpatialRules（空间算法）`：闭合多边形、折线缓冲区、稳定优先级和有界批量几何；上限256来源、每来源1024点，不能写Landscape或改变导航。
- `FGamePlatformPCGAdvancedSpatialRules（高级空间规则）`：桥跨候选、体腔排除和室内空间图连通性检查。**仅返回待审批的几何/拓扑，不生成真实桥梁网格、挖地形或修改Gameplay。**
- 原有`FitPostsToSpline`、`BreakSpansByTags`及`BuildRows`依然仅实现点级规则；道路路肩、围栏门洞和地块内缩的实际地图资产仍需要真实UE图/地图生产与验收。所有新增UE自动化源码必须在引擎编译后统一运行。
