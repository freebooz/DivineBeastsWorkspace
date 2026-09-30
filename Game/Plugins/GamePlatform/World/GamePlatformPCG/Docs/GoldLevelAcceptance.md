# GoldLevelAcceptance（PCG金标准关卡验收规范）

## 1. 目的

Gold Level（金标准关卡）用于验证 GamePlatformPCG（游戏平台程序化内容生成插件）的 M0/M1 室外主闭环，不承载神兽联盟正式地图内容，不替代 DBAWorldPack_Village/OpenWorld/MainArena（世界内容包）。

真实地图资产必须由 Unreal Editor（虚幻编辑器）创建和保存；本文只定义验收合同，不能用文本文件冒充 .umap。

## 2. 目标地图

建议开发验证路径：

/Game/Development/Foundation/PCG/Validation/PCG_GoldLevel_M1

该路径属于 Development（开发验证）内容，不是正式世界包，不得直接进入 Shipping（发行）内容清单。

## 3. 必备场景组件

| 组件 | 中文用途 | 原语/阶段 | 最低数量 | 说明 |
| --- | --- | --- | ---: | --- |
| AGamePlatformPCGWorldDirector | 世界编排器 | 全阶段 | 1 | 必须且只能存在一个 |
| AGamePlatformPCGVolumeActor | 森林/岩石/资源体积 | P1/Scatter | 2 | 至少森林区与资源区 |
| AGamePlatformPCGExclusionActor | 人工排除区 | P0/FieldRead | 1 | 验证ManualLock/GameplayExclusion |
| AGamePlatformPCGSplineActor | 道路/小径/围栏 | P2/Networks或Enclosures | 3 | 至少土路、小径、田篱 |
| AGamePlatformPCGPolygonActor | 农田地块 | P4/Parcels | 1 | 闭合多边形 |
| AGamePlatformPCGConnectorActor | 门/手摆桥/打断 | P3/Connectors | 2 | 至少农门与手摆桥/Break |

所有放置器必须显式注册到唯一 WorldDirector（世界编排器）。

## 4. 必备 Definition（定义）

Gold Level 至少需要以下真实 GamePlatformDefinition（平台定义）资产：

- ExecPreset（执行预设）1个。
- PriorityTable（优先级表）1个。
- MeshSet（网格集合）至少3个：乔木、作物、围栏/栏片。
- SpawnPolicy（生成策略）至少2个：森林、作物。
- Layer（层）至少3个：Canopy、Understory或Floor、Crop。
- BiomePreset（群系预设）至少1个。
- ExclusionPreset（排除预设）至少1个。
- RoadProfile（道路配置）至少1个。
- EnclosureProfile（围合配置）至少1个。
- ParcelPreset（地块预设）至少1个。
- CropProfile（作物配置）至少1个。
- ConnectorCatalog（连接件目录）至少1个。

所有 Definition 字段中的有效主资产 ID 必须同时登记在 RequiredDefinitions（必需定义），统一由 GamePlatformData（平台数据）加载。

## 5. 必备模板/子图

M0/M1 Template（模板）：

- TPL_ScatterSurface
- TPL_BiomeGenerator
- TPL_LinearDresser
- TPL_Enclosure
- TPL_Connector
- TPL_GateInsert
- TPL_ParcelFill
- TPL_CropField
- TPL_InterfaceBand

TPL_EnclosureClosed 可通过模板实例/参数表达时，不要求复制第二套逻辑图。

公共 Subgraph（子图）：

- SG_ProjectOnLandscape
- SG_PriorityCarve
- SG_ApplySpawnPolicy
- SG_AssignMeshSet
- SG_FitPostsToSpline
- SG_BreakByIntersection
- SG_WriteClosedExclude

## 6. 功能验收矩阵

| 编号 | 场景 | 期望结果 | 判定 |
| --- | --- | --- | --- |
| G01 | 道路穿过森林 | 道路范围内乔木为0 | 必须通过 |
| G02 | 小径穿过森林 | 小径排除按MinorRoad优先级生效 | 必须通过 |
| G03 | 道路穿过农田 | 道路范围内作物为0 | 必须通过 |
| G04 | 农田边界 | 自动围栏连续、无明显重叠 | 必须通过 |
| G05 | 农门 | 围栏被稳定打断并保留门开口 | 必须通过 |
| G06 | 手摆桥/连接件 | 桥/连接件排除范围内树和作物为0 | 必须通过 |
| G07 | ManualLock | 人工锁定区不被任何低优先级自动系统覆盖 | 必须通过 |
| G08 | Exclusion | 排除体积内部目标实例数量为0 | 必须通过 |
| G09 | 同Seed重生成 | 点数量、关键属性分布和输出指纹稳定 | 必须通过 |
| G10 | 换MeshSet | 不修改模板Graph即可替换表现资源 | 必须通过 |
| G11 | 换SpawnPolicy | 不复制模板即可改变密度/缩放策略 | 必须通过 |
| G12 | 非法Schema | 安全失败/空输出，不崩溃 | 必须通过 |
| G13 | 未声明Definition依赖 | 校验失败，禁止半加载运行 | 必须通过 |
| G14 | 多WorldDirector | Data Validation失败 | 必须通过 |
| G15 | 未注册放置器 | Data Validation失败 | 必须通过 |
| G16 | Runtime Template执行 | 真实运行闭环前返回Unsupported，不断言 | 必须通过 |

## 7. 确定性验收

固定 EngineVersion、PluginVersion、SchemaVersion、TemplateId/TemplateVersion、PriorityTableId/ContentRevision、ExecPresetId/ContentRevision、UserSeed、World/Region/SourceId。

比较实例数量、输出范围、关键属性直方图和 OutputFingerprint（输出指纹）。不承诺不同 UE 小版本之间二进制级完全一致。

## 8. 性能记录

至少记录总实例数、各系统实例数、单次生成耗时、清理耗时、Editor Bake耗时、峰值内存、PCG Component数量和最大单次点数。M0/M1 不以“GPU节点更多”作为性能达标依据。

## 9. Dedicated Server（专用服务器）验收

- Runtime Cosmetic（运行时纯装饰）不在 Dedicated Server 执行。
- 纯装饰 Mesh/材质不因 PCG Definition 硬引用进入 Server Cook。
- 道路/桥/玩法锚点等权威相关结果来自一致 Bake（烘焙）或其他服务器可信数据。
- PCG 插件不产生网络授权或玩家状态权威。

## 10. 当前状态

当前已具备 Schema/Primitive/Domain/Definition源码、M0/M1节点源码、Foundation Template/Subgraph生成器源码、World Validator源码和 Gold Level前置静态门禁。

当前尚未形成 Foundation模板 .uasset、Gold Level .umap、功能矩阵实际运行、UE Automation全通过、Client/Server Cook和性能Profile等真实验收证据。M1在这些证据形成前不得标记为生产完成。
