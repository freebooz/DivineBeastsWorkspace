# Templates（PCG模板合同）

当前代码已登记模板 ID 和 Template Contract（模板合同），但本轮没有伪造任何 `.uasset`。真实模板必须由 Unreal Editor（虚幻编辑器）创建、保存并通过 Graph Inspection（图检查）。

| 模板ID | 中文说明 | 当前状态 |
| --- | --- | --- |
| TPL_Base | 基础模板 | 合同ID已实现，资产未生成 |
| TPL_ScatterSurface | 曲面散布模板 | 合同ID已实现，M0目标 |
| TPL_BiomeGenerator | 群系生成模板 | 合同ID已实现，M1目标 |
| TPL_LinearDresser | 线性装饰模板 | 合同ID已实现，M1目标 |
| TPL_Enclosure | 围合模板 | 合同ID已实现，M1目标 |
| TPL_EnclosureClosed | 闭合围合语义 | 合同ID已实现；若只需bClosed参数则使用Graph Instance，不复制逻辑图 |
| TPL_RailingAttached | 附着栏杆模板 | 合同ID已实现，M2接口；资产未生成 |
| TPL_Connector | 连接件模板 | 合同ID已实现，M1目标 |
| TPL_GateInsert | 插门模板 | 合同ID已实现，M1目标 |
| TPL_ParcelFill | 地块填充模板 | 合同ID已实现，M1目标 |
| TPL_CropField | 农田模板 | 合同ID已实现，M1目标 |
| TPL_AssemblySpawn | 组合件投放模板 | 合同ID已实现，接口 |
| TPL_InterfaceBand | 界面带模板 | 合同ID已实现，M1目标 |

计划公共子图：SG_ProjectOnLandscape、SG_PriorityCarve、SG_ApplySpawnPolicy、SG_AssignMeshSet、SG_FitPostsToSpline、SG_BreakByIntersection、SG_WriteClosedExclude。本轮只形成合同/文档，不冒充对应资产已经存在。

`ValidateApprovedGraph（批准图检查）`现在支持两条路径：

1. TemplateId为空：继续执行0.1.0固定四节点Legacy Development Fixture（旧开发夹具）检查。
2. TemplateId非空：要求已登记模板、正数TemplateVersion、Schema主版本一致、节点属于M0/M1批准类、CPU执行、无未知节点；`WriteSchemaDefaults（写协议默认值）`与`ValidateSchema（验证协议）`必须各唯一，并且实际满足 `SchemaWriter → SchemaValidator → Output` 可达关系，不能把合规节点放在断开的旁路中骗过合同检查。

HiGen/GPU仍在M2前失败关闭。

当前模板合同仍不等于“模板可以运行”：真实 Template `.uasset`、Definition 参数绑定、MeshSetId→真实 Spawner 资源解析、Bake 输出审查尚未形成闭环，因此 Runtime Service 对 `TemplateId` 非空的请求继续 Fail-Closed（失败关闭）并返回 Unsupported。
