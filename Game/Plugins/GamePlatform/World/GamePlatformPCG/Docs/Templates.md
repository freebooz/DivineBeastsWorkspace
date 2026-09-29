# Templates（PCG模板合同）

当前代码已登记 Template ID（模板ID）、Template Contract（模板合同），并已实现 M0/M1 Foundation Template Generator（基础模板生成器）与 Editor Commandlet（编辑器命令行工具）。生成器使用 UE5.8 真实 `UPCGGraph`、真实节点和真实连线，并设置官方 `bIsTemplate=true`；当前尚未实际执行落盘，因此仍没有把未生成的 `.uasset` 宣称为已交付资产。

| 模板ID | 中文说明 | 当前状态 |
| --- | --- | --- |
| TPL_Base | 基础模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_ScatterSurface | 曲面散布模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_BiomeGenerator | 群系生成模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_LinearDresser | 线性装饰模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_Enclosure | 围合模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_EnclosureClosed | 闭合围合语义 | M0/M1生成器已实现；后续若只差bClosed参数仍优先Graph Instance，不复制逻辑图 |
| TPL_RailingAttached | 附着栏杆模板 | 仅合同ID，M2接口；M0/M1生成器明确不创建 |
| TPL_Connector | 连接件模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_GateInsert | 插门模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_ParcelFill | 地块填充模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_CropField | 农田模板 | M0/M1生成器已实现，资产尚未执行落盘 |
| TPL_AssemblySpawn | 组合件投放模板 | M0/M1生成器已实现，作为接口骨架；资产尚未执行落盘 |
| TPL_InterfaceBand | 界面带模板 | M0/M1生成器已实现，资产尚未执行落盘 |

公共子图生成器源码已经完成：`SG_ProjectOnLandscape（投影到地形）`、`SG_PriorityCarve（优先级挖洞）`、`SG_ApplySpawnPolicy（应用生成策略）`、`SG_AssignMeshSet（分配网格集合）`、`SG_FitPostsToSpline（样条布柱）`、`SG_BreakByIntersection（按交叉标签打断）`、`SG_WriteClosedExclude（写闭合排除）`。其中 ProjectOnLandscape 使用 UE5.8 官方 `UPCGProjectionSettings（PCG投影节点）`并公开 Landscape（地形）输入 Pin；WriteClosedExclude 使用平台统一 `WriteExclude（写排除属性）`节点。当前仍未实际执行 Commandlet 落盘，因此不能把 `.uasset` 写成已交付。

`ValidateApprovedGraph（批准图检查）`现在支持两条路径：

1. TemplateId为空：继续执行0.1.0固定四节点Legacy Development Fixture（旧开发夹具）检查。
2. TemplateId非空：要求已登记模板、正数TemplateVersion、Schema主版本一致、节点属于M0/M1批准类、CPU执行、无未知节点；`WriteSchemaDefaults（写协议默认值）`与`ValidateSchema（验证协议）`必须各唯一，并且实际满足 `SchemaWriter → SchemaValidator → Output` 可达关系，不能把合规节点放在断开的旁路中骗过合同检查。

HiGen/GPU仍在M2前失败关闭。

## 生成方式

编译 `GamePlatformPCGEditor（PCG编辑器模块）` 后，可通过 UnrealEditor-Cmd（虚幻编辑器命令行）显式运行：

```text
F:\UnrealEngine-5.8.0-release\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
E:\poject\feebooz\DivineBeastsWorkspace\Game\DivineBeastsArena.uproject
-run=GamePlatformPCGFoundationTemplates -unattended -nop4 -nosplash
```

命令行工具统一生成 `/Game/Development/Foundation/PCG/Templates/` 与 `/Game/Development/Foundation/PCG/Subgraphs/`：创建前对模板与子图全部目标包做统一占用预检，任一目标已存在则整批拒绝启动；模板先在内存中通过 Template Contract，子图使用已批准的官方/平台节点真实构图，再开始保存。保存阶段若发生磁盘失败，保留现场供人工审查，不自动删除可能已写入的包。

这些资产属于 Development（开发验证）模板，不进入正式世界 ContentPack（内容包）所有权。只有 Gold Level、AssetRegistry/DataValidation、Client/Server Cook 和性能验收完成后，才另行评审正式发布模板的资产归属和 `CanContainContent` 策略。

当前模板合同仍不等于“模板可以运行”：真实 Template `.uasset`、Definition 参数绑定、MeshSetId→真实 Spawner 资源解析、Bake 输出审查尚未形成闭环，因此 Runtime Service 对 `TemplateId` 非空的请求继续 Fail-Closed（失败关闭）并返回 Unsupported。
