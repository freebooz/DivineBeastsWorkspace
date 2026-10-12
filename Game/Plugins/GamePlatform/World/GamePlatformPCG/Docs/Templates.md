# Templates（PCG模板合同）

截至2026-10-12，当前工作区已经存在真实UE格式的12份Foundation Template（基础模板）`.uasset`及7份公共Subgraph（子图）`.uasset`，并经过磁盘名称、非空长度和SHA-256审计。生成器使用UE5.8的`UPCGGraph`、官方节点及`bIsTemplate=true`。**文件落盘不代表独立UE编辑器重开、节点可执行、Spawner实例或正式Cook已通过。**

| 模板ID | 中文说明 | 当前状态 |
| --- | --- | --- |
| TPL_Base | 基础模板 | `.uasset`文件已落盘；待独立UE重开验证节点 |
| TPL_ScatterSurface | 曲面散布模板 | `.uasset`文件已落盘；待实际Spawner生成 |
| TPL_BiomeGenerator | 群系生成模板 | `.uasset`文件已落盘；待场景验证 |
| TPL_LinearDresser | 线性装饰模板 | `.uasset`文件已落盘；道路/围栏生成仍待实证 |
| TPL_Enclosure | 围合模板 | `.uasset`文件已落盘；栏柱/门洞与网格生成待实证 |
| TPL_EnclosureClosed | 闭合围合语义 | `.uasset`文件已落盘；若只差bClosed参数仍优先Graph Instance（图实例） |
| TPL_RailingAttached | 附着栏杆模板 | 仅合同ID，M2接口；M0/M1生成器明确不创建 |
| TPL_Connector | 连接件模板 | `.uasset`文件已落盘；桥/门静态资产尚待项目审核 |
| TPL_GateInsert | 插门模板 | `.uasset`文件已落盘；真门洞仍待G05验证 |
| TPL_ParcelFill | 地块填充模板 | `.uasset`文件已落盘；实际地块生成待验证 |
| TPL_CropField | 农田模板 | `.uasset`文件已落盘；实际作物图/道路互斥待验证 |
| TPL_AssemblySpawn | 组合件投放模板 | `.uasset`文件已落盘，仅原型骨架，项目美术/碰撞仍待接入 |
| TPL_InterfaceBand | 界面带模板 | `.uasset`文件已落盘；环境真实生成待验证 |

公共子图生成器源码已经完成：`SG_ProjectOnLandscape（投影到地形）`、`SG_PriorityCarve（优先级挖洞）`、`SG_ApplySpawnPolicy（应用生成策略）`、`SG_AssignMeshSet（分配网格集合）`、`SG_FitPostsToSpline（样条布柱）`、`SG_BreakByIntersection（按交叉标签打断）`、`SG_WriteClosedExclude（写闭合排除）`。其中 ProjectOnLandscape 使用 UE5.8 官方 `UPCGProjectionSettings（PCG投影节点）`并公开 Landscape（地形）输入 Pin；WriteClosedExclude 使用平台统一 `WriteExclude（写排除属性）`节点。当前上述7份子图已经在磁盘落盘；仍须由独立UE5.8进程核验节点、引脚、Schema及生成输出，不得仅凭文件完整性视为生产可用。

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
### 2026-10-10 P1～P7代码补齐入口

- Foundation散布/群系/界面带/组合件模板添加UE5.8原生CreatePointsGrid（网格点采样），道路线性/农田模板添加官方SplineSampler（样条采样）；M0/M1批准节点保持CPU执行。
- `CreateFoundationRealizedGraph（生成已绑定网格的图）`及`CreateDevelopmentRealizedGraphAsset（创建真实图资产）`使用已加载且在RequiredDefinitions登记的MeshSet，在Editor内加入加权StaticMeshSpawner并严格检查碰撞/导航用途。当前仅批准散布、群系、界面带、农田及简单组合模板；道路/围栏按ID映射多个网格尚不能声称完成。
- `ConfigureRealizedGraphSpatialMasks（配置空间掩码）`从WorldDirector读取已验证几何快照，绑定非模板Graph Instance；不会扫描全世界Actor、同步加载材质或更改专用服务器权威地形。
- `CreatePCGPlacementBlueprints（创建PCG放置器蓝图）`由正式UE Editor创作11类派生平台基类的真实Blueprint，必须显式指定已经注册的`/…/PCG/Blueprints/`内容包挂载点，不覆盖现有包。示例编辑器命令：`-run=GamePlatformPCGFoundationTemplates -BlueprintRoot=/DBAWorldPack_Village/PCG/Blueprints/`（仅当项目编辑器编译与所有必需模块加载成功后执行）。蓝图默认值只包含领域/原语/阶段，项目网格及图实例仍需单独绑定。
- 当前12份Foundation模板、7份公共子图及金标准地图/定义等已有真实UE资源文件；但最新PCG C++、实际Graph Generate、独立重开、G01～G16和Client/Server干净Cook尚未闭环。按既定顺序先完成代码、蓝图与资源，再统一自动化；不以资产存在代替运行验收。
