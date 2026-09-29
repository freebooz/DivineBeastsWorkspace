# DataDefinitions（PCG数据定义）

> 统一加载规则：PCG Definition（定义）字段中的 `FPrimaryAssetId（主资产ID）`如果有效，必须同时列入基类 `RequiredDefinitions（必需定义）`。运行时只通过 `GamePlatformData（平台数据）`递归租约加载，不建立第二套 PCG 资源加载器；缺少声明时 Definition 校验失败。
> 资源分组规则：`UGamePlatformPCGMeshSetDefinition（网格集合定义）`中的真实 Mesh（网格）软引用进入 `PCGGeneration（PCG生成）` Asset Bundle（资产分组），与现有 Profile 的 Graph/OutputMesh 使用同一分组；运行服务申请该分组时才允许把相关生成资源一起纳入租约。

所有正式配置继续继承 `UGamePlatformDefinitionBase（平台定义基类）`，由 `GamePlatformData（平台数据插件）`统一处理稳定身份、版本、依赖和 Lease（租约）。

| Definition | 中文名称 | 当前状态 | 主要用途 |
| --- | --- | --- | --- |
| UGamePlatformPCGProfileDefinition | PCG兼容配置定义 | 现有并扩展 | 0.1.0运行请求兼容入口、Template Contract头 |

> Profile（配置）兼容规则：`TemplateId（模板ID）`为空的 0.1.0 Legacy（旧四节点）Profile 仍要求单一 `OutputMesh（输出网格）`；1.0 Template Profile 可以不填写 `OutputMesh`，由后续受控 MeshSet/Spawner（网格集合/生成器）合同完成 Realize（实例化）。
| UGamePlatformPCGExecPresetDefinition | 执行预设 | M0已实现 | Grid、预算、Runtime/HiGen/Net策略；M0/M1拒绝HiGen与ServerAuth |
| UGamePlatformPCGMeshSetDefinition | 网格集合 | M0已实现 | 网格、权重、碰撞意图、阴影、剔除距离 |
| UGamePlatformPCGSpawnPolicyDefinition | 生成策略 | M0已实现 | 密度、缩放、坡度、高度、KeepVertical、自修剪 |
| UGamePlatformPCGLayerDefinition | 生成层 | M0已实现 | Layer、MeshSet、SpawnPolicy与Children |
| UGamePlatformPCGBiomePresetDefinition | 群系预设 | M0已实现 | BiomeId、层目录和种子 |
| UGamePlatformPCGExclusionPresetDefinition | 排除预设 | M0已实现 | Hard/Soft/DensityScale排除 |
| UGamePlatformPCGPriorityTableDefinition | 优先级表 | M0已实现 | 版本化PriorityCarve层级 |
| UGamePlatformPCGRoadProfileDefinition | 道路配置 | M1基础已实现 | 宽度、路肩、边沟、坡度/曲率约束 |
| UGamePlatformPCGEnclosureProfileDefinition | 围合配置 | M1基础已实现 | 高度、柱距、贴地、门目录、跨度网格规则 |
| UGamePlatformPCGParcelPresetDefinition | 地块预设 | M1基础已实现 | 用途、内缩、边围合、内部系统引用 |
| UGamePlatformPCGCropProfileDefinition | 作物配置 | M1基础已实现 | 垄距、株距、朝向、季节MeshSet |
| UGamePlatformPCGConnectorCatalogDefinition | 连接件目录 | M1基础已实现 | Gate/Bridge/Intersection/Break统一目录 |
| UGamePlatformPCGBakeManifest | 烘焙清单 | 现有并扩展 | Source/Output Fingerprint及Schema/Template/Stage等生产证据 |

River/Rail/Utility/Building/SpaceGraph/State（河流/铁路/管线/建筑/空间图/状态）不因外部方案存在就创建空类型；按M2～M4真实消费者出现后再实施。

Source Fingerprint（源指纹）除 Profile/Graph/Legacy OutputMesh 外，还会解析 Profile 的直接 `RequiredDefinitions（必需定义）`主资产路径并纳入来源记录。更深层嵌套 Definition 的完整闭包在生产 Bake（烘焙）启用前仍须结合 GamePlatformData 的递归租约结果补齐验证，不得把当前直接依赖覆盖误写为最终闭包证明。
