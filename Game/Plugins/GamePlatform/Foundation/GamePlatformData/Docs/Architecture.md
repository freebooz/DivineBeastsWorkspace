# GamePlatformData（游戏平台数据）插件设计

> 版本基线：2026-09-27。
> 正式位置：`Game/Plugins/GamePlatform/Foundation/GamePlatformData/`。
> 本文定义 GamePlatformData（游戏平台数据）的职责、数据身份、加载租约、依赖图、AssetRegistry（资产注册表）、Bundle（资产束）、Cook（烘焙）与端侧边界。真实 Public API 见 [API.md](API.md)，验证证据见 [TestingAndEvidence.md](TestingAndEvidence.md)。

## 1. 定位

GamePlatformData 是 `GamePlatform（游戏平台层）` 中所有 Definition（定义）与 Primary Asset（主资产）身份、加载、租约和校验的唯一通用数据底座。

设计原则：

- 主工程只配置一套 `UGamePlatformAssetManager（游戏平台资产管理器）`。
- 业务插件不得创建第二套 AssetManager 或独立 StreamableManager 作为长期资源所有者。
- 资源身份与物理路径、文件名、具体派生 C++ 类解耦。
- Definition 成功加载后仍由 Lease（租约）持有；释放才允许撤销本调用方的需求。
- Runtime（运行时）和 Editor（编辑器）共用同一套 Definition 身份、版本与依赖边界。
- Data 只管理资源与定义，不拥有 Gameplay（玩法）、Online（在线）、MOBA 或 DivineBeasts（神兽联盟）规则。

## 2. 模块与依赖

### GamePlatformData（运行时模块）

公开依赖仅为：

```text
Core
CoreUObject
Engine
GamePlatformCore
```

Private（私有）使用 `AssetRegistry（资产注册表）`。

### GamePlatformDataEditor（编辑器模块）

在运行时模块基础上增加：

```text
AssetRegistry
DataValidation
UnrealEd
```

运行时模块不得反向依赖 Editor。

## 3. Definition 根体系

正式根继承链：

```text
UPrimaryDataAsset
      ↑
UGamePlatformPrimaryDataAsset
      ↑
UGamePlatformDefinitionBase
      ↑
各领域具体Definition
```

### UGamePlatformPrimaryDataAsset

负责：

- `FGamePlatformId（平台逻辑身份）`。
- 固定 `GamePlatformDefinition（平台定义）` 主资产类型。
- `GetPrimaryAssetId()`。
- AssetRegistry 中的 `GamePlatformLogicalId（平台逻辑ID）` 标签。

### UGamePlatformDefinitionBase

负责：

- `FGamePlatformDataVersion（平台数据版本）`。
- `RequiredDefinitions（必需定义）`。
- SchemaVersion（结构版本）可读范围。
- ContentRevision（内容修订）。
- Definition 基础校验。
- AssetRegistry 中的结构版本、内容修订、直接依赖数量标签。

新增稳定标签：

```text
GamePlatformSchemaVersion
GamePlatformContentRevision
GamePlatformRequiredDefinitionCount
```

这些标签允许 Editor/Commandlet/CI 在不加载 UObject 的情况下先进行索引、筛选和审计。

## 4. Primary Asset（主资产）身份

主资产 ID 固定为：

```text
GamePlatformDefinition:<LogicalId规范字符串>
```

例如：

```text
GamePlatformDefinition:game.character.hero@1
```

稳定身份不依赖：

- 文件名。
- Content Browser（内容浏览器）路径。
- C++ 派生类名称。
- Chunk ID（资源分块编号）。

资产重命名或移动不应改变逻辑身份；发布后修改 LogicalId 必须按正式迁移处理。

## 5. 数据版本

`FGamePlatformDataVersion（平台数据版本）` 包含：

- `SchemaVersion（结构版本）`：从1开始。
- `ContentRevision（内容修订）`：同结构内容修订，从1开始。

可读 SchemaVersion 范围由 Definition 类 CDO（类默认对象）的：

```text
GetMinimumReadableSchemaVersion()
GetMaximumReadableSchemaVersion()
```

决定。

资产不能通过编辑字段自行扩大代码声明的兼容范围。

## 6. Definition 校验

基础校验至少包括：

- LogicalId 合法。
- SchemaVersion 位于类 CDO 声明的可读区间。
- ContentRevision >= 1。
- RequiredDefinitions 必须使用 GamePlatformDefinition 主资产类型。
- RequiredDefinitions 的逻辑身份必须合法。
- 不能直接自依赖。
- 同一直接依赖不能重复。

运行期递归和 Editor Data Validation（数据验证）还会继续检查：

- 缺失定义。
- 间接循环。
- 子定义版本非法。
- 依赖图深度/规模上限。
- 实际主资产映射与唯一源路径一致。

## 7. AssetRegistry（资产注册表）与唯一源

`ResolveUniqueGamePlatformDefinitionSource` 保持稳定公开签名，内部转发到模块私有 `FDefinitionSourceIndex`。唯一性依据仍是平台主资产源元数据，不使用 AssetManager 已去重的字典。

索引维护 `LogicalId → TSet<FSoftObjectPath>`，另有 `FSoftObjectPath → 原LogicalId` 反向表。前者允许明确报告多个物理源占用同一身份；后者在标签改变、删除或重命名时撤销旧身份，不会因事件携带的新ID而遗漏旧源。这里只存值类型与引擎注册表的非拥有引用，没有资产、世界、PIE对象或加载租约。

模块启动仅绑定事件。首次查询必须确认 `IsLoadingAssets()==false` 且 `IsGathering()==false`，之后才调用一次 `GetAssetsByClass(UGamePlatformPrimaryDataAsset,...,true)` 冷建。UE5.8的前者只代表首次扫描，后者还覆盖后续挂载；Cooked运行在注册表已经发现完成时同样按此路径首次建索引，不要求再次收到 `OnFilesLoaded`。生产入口不调用 `SearchAllAssets`、同步等待、资产加载或第二个加载器。

| 变化 | 私有索引处理 |
|---|---|
| `OnAssetAdded / OnAssetUpdated / OnAssetUpdatedOnDisk` | 同路径先撤销旧ID，再按真实定义类资格及合法标签增量加入 |
| `OnAssetRemoved` | 按路径撤销，删除后空的身份集合也移除 |
| `OnAssetRenamed` | 先撤销旧路径，再登记新路径及当前ID |
| `OnScanStarted / OnFilesLoaded / OnKnownGathersComplete` | 清除旧快照，只在下一次发现完成后的查询重建 |
| 带生成类标签的蓝图类资产变化 | 类继承关系可能影响其他源，明确失效后重新取完整快照 |
| `Shutdown` | 取消尚未发布的快照、解除八个委托、清空索引和访问回调；关闭后不自动重启 |

普通资产事件增量处理。连续普通查询只做逻辑身份哈希查询，不重新枚举全部定义。已加载类通过 `GetClass(EResolveClass::No)->IsChildOf` 判定；未加载类通过Registry的递归类过滤筛选单条元数据，不为查重同步加载蓝图类。其他资产即使具有同名LogicalId标签也不具备定义资格。

解析结果继续保持：非法ID为 `InvalidDefinitionId`、未发现为 `MissingDefinition`、多个物理源为 `DuplicateLogicalId`；发现未完成或索引关闭为 `AssetRegistryNotReady`。所有失败清空输出路径。编辑器候选总是先替换自己同路径的注册记录，再按当前身份参与本次查询；覆盖不写回共享索引，候选与其他路径仍可冲突。运行期 `AddDemand` 原有 `PrimaryAssetMappingMismatch` 检查不变，唯一源与AssetManager映射不一致仍拒绝。

所有入口与修改仅GameThread。快照先在局部构建，资产事件、重新发现或同步关闭推进内部代次；代次变化后整批取消，不发布部分或过期结果。索引没有后台任务、Ticker、世界指针和跨PIE请求状态，租约取消仍由原Data服务负责。独立回归直接验证枚举期间失效/Shutdown不会发布旧源；实际编辑器回归另外核对委托已解除及临时资产源已撤销。

`QueryCount / FullScanCount / RebuildCount` 是模块私有观测值，分别表示查询、实际源枚举和完整快照发布次数。测试用这些计数证明重复查询不重扫；本次未实测耗时、CPU、内存或性能百分比。

## 8. Lease（租约）模型

公开服务是 GameInstance（游戏实例）作用域：

```text
IGamePlatformDataService
```

租约身份由：

```text
ScopeId
LeaseId
Generation
DefinitionId
Bundles
```

共同构成。

规则：

- 跨实例、篡改代次、篡改身份或篡改 Bundle 的句柄不能释放真实租约。
- Instance（实例）租约可跨地图，但调用者本身必须存活。
- World（世界）租约绑定申请时世界，世界清理自动释放。
- 成功只代表 Definition 已可读，资源继续持有到显式 Release。
- 失败回滚本租约已登记的所有资源需求，不影响其他租约。
- 重复释放真实签发过的同一完整句柄保持幂等。

## 9. 依赖图安全上限

运行时和 Editor 共用：

```text
GamePlatformDataLimits.h
```

当前统一安全上限：

```text
MaxDependencyDepth = 128
MaxDefinitionsPerRequest = 4096
MaxBundlesPerLease = 64
```

目的不是表达产品性能预算，而是防止错误或不可信数据造成无界递归、内存增长和异常资产请求。

这些值不是“3A性能指标”；后续调整必须有真实内容规模和性能测试证据。

## 10. Asset Bundle（资产束）

AcquireDefinition 允许调用方申请 Bundle，并进行：

- 去重。
- 规范排序。
- `NAME_None` 拒绝。
- 单租约唯一 Bundle 数量上限。

多租约对同一 Definition 的 Bundle 需求取并集。

例如：

```text
Lease A: Core + UI
Lease B: Core + Server
最终引擎需求: Core + UI + Server
```

释放 A 后，只撤销 A 独有的 UI；B 和外部基线仍然保留。

Bundle 名本身是资源加载分组，不等于客户端/服务器安全证明。

## 11. 单一 AssetManager 与外部需求保护

GamePlatformData 不创建第二个加载器。

所有正式主资产加载最终通过引擎 `UAssetManager`。

平台 AssetManager 还会：

- 记录本服务租约需求。
- 保存首次接管时的外部基线。
- 合并后来发生的外部 Bundle 请求。
- 拒绝卸载仍被平台租约持有的资产。
- 最后一个平台租约释放后恢复/保留外部基线。

它不能证明所有第三方代码都使用该入口；绕过 UAssetManager 的独立 StreamableManager 不属于本插件可保证的所有权边界。

## 12. Cook / Chunk / ContentPack 边界

非常重要：

`RequiredDefinitions` 是运行期 Definition 身份关系，**不是 Cook 软引用替代品**。

正式内容仍必须通过以下机制进入 Cook：

- AssetManager 扫描目录。
- Primary Asset Rules（主资产规则）。
- UPROPERTY SoftObjectPtr（软引用）和 AssetBundles 元数据。
- ContentPack（内容包）注册与构建配置。
- Chunk / DLC / Patch（资源分块/可下载内容/补丁）规划。

当前主工程 `GamePlatformDefinition` 仍扫描开发目录 `/Game/Development/Foundation/Definitions`，且 `CookRule=Unknown`、`ChunkId=-1`。因此当前状态是开发验证基线，不代表正式生产 Cook/Chunk 已完成。

GamePlatformData 不硬编码项目 Chunk ID，防止平台层绑定《神兽联盟》内容分包策略。

## 13. Server-safe（服务器安全）边界

不能通过一个 `bServerSafe=true` 字段自证资源适合 Dedicated Server（专用服务器）。

真正的 Server-safe 需要：

- AssetRegistry 依赖图审计。
- ClientOnly（仅客户端）模块/资源依赖审计。
- Server Cook 清单。
- 实际 Server Stage（服务器暂存产物）验证。

Data 插件只提供统一 Definition 身份、AssetRegistry 元数据和加载边界；最终资源安全门禁由 GamePlatformDeveloperTools（游戏平台开发工具）及真实 Cook/Stage 共同证明。

## 14. Diagnostics（诊断）

`FGamePlatformDataDiagnostics` 保持实例级，不暴露整个进程其他 GameInstance 的资源需求。

新增可观测字段：

- `UniqueTrackedDefinitions`。
- `UniqueRequestedBundles`。
- `ReleasedLeaseRecords`。
- `TotalAcceptedRequests`。
- `TotalRejectedRequests`。
- `TotalSucceededRequests`。
- `TotalFailedRequests`。
- `TotalCancelledRequests`。

这些值可由 Debug/Telemetry 上层读取，但 Data 不反向依赖 Telemetry。

`ReleasedLeaseRecords` 保留诊断字段兼容，目前恒为0。2026-09-30以实例秘密签发完整租约身份摘要，保留两个秘密与单调代次即可验证重复释放，不再维护已释放记录表。此摘要不替代网络认证，句柄不可跨进程或实例恢复。

## 15. 当前已知限制

1. 活跃及未显式释放的失败终态仍需调用方结束用途后释放；移除已释放历史并不取消调用方的清理责任。
2. 源身份唯一性现由模块私有索引维护；首次发现/后续挂载/蓝图类变化后的冷建仍需枚举定义元数据。普通查询与普通资产事件避免全扫描，但真实耗时、内存及Cooked发现仍待实测。
3. Owner（所有者）弱引用存活检查当前使用 CoreTicker（核心Ticker）；尚未基于实测调整检查频率。
4. 资源与内容验收依赖正式主工程实际资产；本轮源码修复没有新建或修改资产，也不能替代干净Cook/Chunk/Server产物审计。
5. 本轮已找到锁定UE5.8.0的编辑器可执行文件；历史Automation未运行记录保持为历史证据，本轮动态结果统一见修复执行记录。

## 16. 当前验证状态

- Native Demand Ledger Debug：1/1 Passed。
- Native Demand Ledger Release：1/1 Passed。
- UE5.8 Editor Runtime 模块：通过。
- UE5.8 Editor 模块：通过。
- UE5.8 Client Runtime 模块：通过。
- UE5.8 Server Runtime 模块：通过。
- 三层架构边界：需随本次最终回归重新验证。
- UE Automation：未运行。
- Cook / Stage / Chunk：未验证。

详细证据见 [TestingAndEvidence.md](TestingAndEvidence.md)。
## 2026-09-30 普通资源租约

IGamePlatformDataService新增AcquireResources/ReleaseResources。普通资源沿用真实软路径，不改变既有HeroDefinition、BehaviorTree、Blackboard、Widget主资产身份；在同一实例请求账本使用同一ScopeId、LeaseId、Generation、IssuerProof与弱调用者清理协议。资源加载复用唯一AssetManager的StreamableManager；每个请求只释放自己持有的引擎句柄，不按路径全局Unload，跨世界与多实例需求不互相撤销。

原始路径数先受统一节点安全上限约束，再去重排序。它是防异常输入的界限，不是CPU/GPU性能预算。成功后调用者持租约直到真实用途结束；失败回滚加载，取消与世界销毁只发布一次请求终态。Definition递归依赖仍走现有DFS，不把普通资源路径视为新的Definition依赖树。
