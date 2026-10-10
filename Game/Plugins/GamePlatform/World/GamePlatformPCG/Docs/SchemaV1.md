# SchemaV1（PCG属性协议 v1）

## 1. 定位

Schema v1 是 `GamePlatformPCG（游戏平台程序化内容生成插件）`内部稳定的跨模板属性合同。协议由代码级 `FGamePlatformPCGSchema（PCG字段注册表）`和 `FGamePlatformPCGAttr（PCG属性访问器）`统一维护，不建立可由项目内容随意修改的第二套 Schema Data Asset（协议数据资产）。

## 2. 当前字段

| 字段 | 逻辑类型 | 中文说明 | 当前用途 |
| --- | --- | --- | --- |
| `Pcg.Biome.Id` | FName | 群系稳定标识 | Biome（群系）分类 |
| `Pcg.Biome.Weight` | float | 群系融合权重 | 混合群系 |
| `Pcg.Biome.Priority` | int32 | 群系优先级 | 分类/决胜 |
| `Pcg.Layer.Name` | FName | 当前生成层名称 | 核心必需字段 |
| `Pcg.Layer.Index` | int32 | 层序号 | 稳定层次排序 |
| `Pcg.Spawn.MeshSetId` | FName | 网格集合目录ID | Realize（实现）阶段资源解析 |
| `Pcg.Spawn.ClassId` | FName | Actor类目录ID | 预留生成类解析；不传任意SoftClassPath |
| `Pcg.Spawn.Flags` | int32 | 生成开关位 | 碰撞/阴影等策略映射 |
| `Pcg.Rule.Slope` | float | 坡度 | 规则过滤 |
| `Pcg.Rule.Height` | float | 海拔/高度 | 规则过滤 |
| `Pcg.Rule.Wetness` | float | 湿度 | 场规则 |
| `Pcg.Rule.SpanLength` | float | 线性剩余跨度 | SelectSpanMeshByLength（按跨度选网格） |
| `Pcg.Exclude.Mask` | float | 0保留、1完全排除 | PriorityCarve（优先级挖洞），核心必需 |
| `Pcg.Exclude.Source` | FName | 排除来源 | 道路/门/人工排除等追踪 |
| `Pcg.Exec.GridBand` | int32 | 网格执行带 | M2 HiGen（分层生成）预留 |
| `Pcg.Exec.LodBand` | int32 | 运行时细节带 | M2 RuntimeDetail（运行时细节）预留 |
| `Pcg.Exec.Seed` | int32 | 确定性种子 | 核心必需字段 |
| `Pcg.Encl.Kind` | uint8语义 | 围合类型 | Fence/Railing/Wall等 |
| `Pcg.Connector.Type` | uint8语义 | 连接件类型 | Gate/Bridge/Intersection/Break |
| `Pcg.Mutable.Id` | FGuid语义 | 可变对象稳定ID | M3状态系统；当前只保留协议 |

## 3. 使用规则

1. 平台源码不得散落手写未知 `Pcg.*` 字符串；新增字段必须先修改 Schema。
2. Graph（图）不直接携带项目任意 SoftPath（软路径）；资源属性优先保存稳定 ID。
3. `WriteSchemaDefaults（写协议默认值）`补齐 M0/M1 基础字段。
4. `ValidateSchema（验证协议）`始终验证 Layer.Name、Exclude.Mask、Exec.Seed 等核心字段；显式必需清单只能增加约束，不能绕过核心检查。已注册的 `Pcg.*` 字段必须匹配UE5.8 Metadata（元数据）的真实值类型，未知 `Pcg.*`、缺失字段、类型错误均安全丢弃输入并记录 Warning（警告），不崩溃。
5. `Pcg.Mutable.Id` 在 M3 真正写入前必须针对 UE5.8 PCG Metadata（PCG元数据）FGuid 存储能力补运行证据；当前代码不伪装已完成写入。
6. 当前 UInt8（8位枚举语义）接受已采用的 PCG Integer32（32位整数）写入，或UE5.8原生 Byte（字节）元数据；Guid（全局唯一标识）实际存储尚未验证，若提前写入 Pcg.Mutable.Id 一律拒绝执行。2026-10-10已增加对应C++验证路径与自动化测试源码，但尚缺真实UE构建/测试通过证据。

## 4. 版本

当前：Major=1、Minor=0。Major 变化表示不兼容协议升级；1.x 只允许向后兼容增加字段和能力。
