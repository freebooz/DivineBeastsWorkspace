# GamePlatformData（平台数据插件）

更新日期：2026-09-27。GamePlatformData 已完成本轮 Definition 元数据、依赖安全上限、租约诊断与 UE5.8 兼容性补强；原生需求账本 Debug/Release 通过，Runtime／Editor／Client／Server 模块编译通过。UE Automation、真实资产、Cook／Chunk／Server-safe 仍按下文边界单独验收，不宣称生产就绪。

设计文档入口：[插件设计](Docs/Architecture.md)｜[Public API说明](Docs/API.md)｜[测试与验证证据](Docs/TestingAndEvidence.md)。

## 职责与接入

双端 `GamePlatformData` 提供稳定主资产身份、只读定义、结构版本检查、实例门面及进程需求租约。`GamePlatformDataEditor` 仅参与编辑器目标，提供原生 Data Validation 验证器。依赖仅为 Core、CoreUObject、Engine、AssetRegistry、GamePlatformCore；编辑器另依赖 DataValidation 与 UnrealEd。没有项目、MOBA、UI、后端、流程或VFX实现依赖。

主工程由父任务增量配置一次 `/Script/GamePlatformData.GamePlatformAssetManager`，扫描类型 `GamePlatformDefinition`，基类 `/Script/GamePlatformData.GamePlatformDefinitionBase`，普通定义资产而非蓝图类主资产。扫描目录和烘焙规则归主工程；RequiredDefinitions 是主资产身份引用，不能假设这些ID自动产生软引用烘焙依赖，所有必需定义仍须纳入正确扫描/烘焙范围。插件不替换引擎全局对象、不修改主工程配置。

`UGamePlatformPrimaryDataAsset` 继承 `UPrimaryDataAsset`。其 `LogicalId` 的规范字符串决定 `GamePlatformDefinition:<逻辑身份>`，与资产文件名、路径和具体C++类名无关。非法身份返回无效主资产ID。源标签固定为 `GamePlatformLogicalId`。

`UGamePlatformDefinitionBase` 的 `DataVersion.SchemaVersion` 与 `DataVersion.ContentRevision` 均默认1，可编辑默认值、蓝图只读。可读结构范围由具体类 CDO 的 `GetMinimumReadableSchemaVersion()` / `GetMaximumReadableSchemaVersion()` 代码决定，默认1..1。派生类扩展 `ValidateDefinition()` 时必须调用 Super，不能用资产字段自行扩大可读范围。Definition 现在还会向 AssetRegistry 输出 `GamePlatformSchemaVersion`、`GamePlatformContentRevision`、`GamePlatformRequiredDefinitionCount` 三个标签，并在基础校验阶段直接拒绝自依赖。

## 已写入的公开服务

包含 `Interfaces/IGamePlatformDataService.h` 即可调用，不需要私有子系统头：

```cpp
static IGamePlatformDataService* Get(UGameInstance& GameInstance);
FGamePlatformDataLease AcquireDefinition(
    const FPrimaryAssetId& DefinitionId,
    TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass,
    const TArray<FName>& Bundles,
    EGamePlatformDataLifetime Lifetime,
    TWeakObjectPtr<UObject> WeakCaller,
    FGamePlatformDataCompletion Completion,
    FGamePlatformResult& OutResult);
const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease& Lease) const;
FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease& Lease);
EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const;
FGamePlatformDataDiagnostics GetDiagnostics() const;
// Diagnostics现在同时包含当前唯一Definition/Bundle数量、严格幂等释放记录数，
// 以及Accepted/Rejected/Succeeded/Failed/Cancelled累计计数。
```

完成类型为 `TFunction<void(const FGamePlatformDataLease&, const FGamePlatformResult&)>`。全部接口和UObject访问在游戏线程。OutResult只说明是否接纳；成功接纳返回Loading租约，实际终态始终在核心Ticker下一外层调度轮或更晚发布，包括缓存、空分组和无新工作。两阶段回调首次只返回true进入引擎TickedElements，第二次才执行Work；从Ticker外提交可能等待两轮。不能使用零延迟一次性AddTicker假装下一轮，因为UE5.8会在同轮继续消费新添加的回调。实现不依赖时间epsilon、帧号或旧世界计时器，零Delta也能推进。弱调用者销毁后抑制其外部回调并释放资源，不调用悬空所有者。同步参数拒绝返回无效租约，并在回调和调用者有效时延后通知失败。

租约包含 ScopeId、LeaseId、Generation、DefinitionId、去重排序的Bundles和请求状态快照。实时状态必须查询 GetLeaseState，不能用旧副本状态判断。完整身份与分组必须匹配真实签发记录；跨实例、篡改代次和伪造句柄被拒绝。成功通知最多一次，资源持续持有至释放；成功后的释放不再次通知完成。

Instance租约允许切图，但调用者本身仍须存活；建议跨图调用者使用GameInstance或其拥有对象。World租约绑定申请时调用者的真实世界，只在对应世界清理时撤销。实例关闭只释放自身租约。关闭先撤销请求，再调用引擎清理；旧回调无法复活租约。

## 实际加载、依赖与所有权

进程登记表私有地归属引擎配置的 `UGamePlatformAssetManager`，实际使用父任务提供的生产 `FDemandLedger`，键由资产ID、ScopeId、LeaseId、Generation组成。A需求Core+UI、B需求Core+Server时向引擎提交三者并集；释放A后只保留B和外部基线仍需要的分组。空分组也持有根定义。

所有运行期加载通过引擎 `LoadPrimaryAsset`，没有第二个加载器。每次需求变化生成新的操作Serial；过期完成或取消不能处理最新等待者。UE5.8允许无新工作时返回空句柄，代码不把空句柄直接当失败，而是延后检查实际对象和加载状态。引擎取消与完成均接入同一代次校验。

首次接管资产时保存引擎当前/待加载句柄、分组及对象已加载状态。外部待加载操作先完成后才由本服务调整分组；无本服务需求时恢复并保留外部分组，不卸载外部原先加载的根资产。

独立审查后增加了可执行的外部调用保护：重写引擎 `ChangeBundleStateForPrimaryAssets` 新参数虚函数，原生 `LoadPrimaryAsset` / `LoadPrimaryAssets` 及旧签名最终经过该入口。服务内部应用需求时使用私有调用标记；后来的外部请求进入此入口时，即使分组完全相同、没有新句柄，也将其声明需求保守加入外部基线，并与存活租约并集交给引擎。外部操作前更新Serial，让其可能触发的旧取消回调失效，随后等待新外部句柄再恢复本服务等待者。多资产调用分别使用原生AssetManager修改状态，只用引擎组合句柄合并完成，没有另行实现加载器。

`UnloadPrimaryAssets` 覆盖过滤仍有账本需求的资产，不允许外部卸载抢走存活租约。最后租约释放后，新的明确外部加载/卸载可接管引擎状态，并撤销本插件尚未执行的基线恢复回调。不能推断未登记外部消费者的精确释放时点；存活租约期间外部基线采用保守并集，可能保留多余资源。因此业务仍应统一使用数据租约。直接使用另一套StreamableManager、显式限定调用父类实现或直接操纵引擎内部不在此保护范围，不宣称覆盖任意第三方加载方式。

每个根租约逐项真实加载 `RequiredDefinitions`，依赖复用同一主租约需求键与分组。显式DFS栈检测环和缺失；Runtime 与 Editor 统一使用 `GamePlatformDataLimits.h`：最大依赖深度128、单请求最多4096个唯一Definition、单租约最多64个去重Bundle。超限显式失败，不靠C++无限递归。每个实际对象检查主资产身份、根预期类型、类CDO结构范围、内容修订和定义扩展校验。任一节点失败释放本根租约的所有已登记需求，其他租约不受影响。

注册表发现未完成时请求保持Loading并延后重试，可以取消；实例创建早于管理器的特殊宿主允许后续申请重新读取同一个引擎管理器，不永久缓存空值。正常启动顺序已只读核对UE5.8源码：`UEngine::InitializeObjectReferences` 创建资产管理器，`UGameEngine::Init` 先调用 `UEngine::Init` 再 `InitializeStandalone` 创建游戏实例，`GetIfInitialized` 实际返回 `GEngine->AssetManager`。

## 源验证与测试

`ResolveUniqueGamePlatformDefinitionSource` 通过 AssetRegistry `GetAssetsByClass(..., true)` 扫描平台主资产及派生类，按源标签独立查重，不依赖已经去重的主资产字典。编辑器候选对象以未保存的当前逻辑身份替换同路径旧标签。运行期还核对唯一源路径与引擎主资产映射一致。

原生 `UEditorValidatorBase` 派生类由编辑器验证子系统发现，使用UE5.8的 `CanValidateAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&) const` 与对应 `ValidateLoadedAsset_Implementation` 签名。真实验证入口返回Valid/Invalid，并加入错误诊断。编辑器只为验证同步取得依赖源对象，不登记运行期长期租约。

已写入以下9个完整身份的UE自动化测试，均 **未执行**；集成门禁须逐项存在且Success，不能仅按最低通过数量替代。门禁脚本由父任务维护，本插件未修改其文件：

- `GamePlatform.Data.Definition.IdentityAndVersion`：稳定身份、结构范围、修订与非法依赖ID。
- `GamePlatform.Data.Editor.SourceDuplicate`：隔离内存包中的不同真实资产对象登记到真实注册表，验证重复身份双方失败。
- `GamePlatform.Data.Editor.DependencyGraph`：真实依赖成功、缺失、子定义版本/修订非法、自环、间接环及必填身份。
- `GamePlatform.Data.Editor.DependencyDepthLimit`：129层实际定义图失败，不无限递归。
- `GamePlatform.Data.Editor.RegisteredValidatorDispatch`：先确认编辑器自动登记平台验证器，再通过真实EditorValidatorSubsystem调度重复身份负例；不手动注册掩盖发现问题。
- `GamePlatform.Data.Runtime.DeferredRequeueZeroDelta`：直接使用真实FTSTicker与生产调度函数，Tick(0)验证持续重排不能同轮执行工作。持续未发现采用调度层未就绪条件，不是实际AssetRegistry发现过程测试。
- `GamePlatform.Data.Runtime.NestedTickerNotification`：在真实Ticker回调中提交生产延后通知，Tick(0)验证本轮不执行、下一外层调度轮执行。
- `GamePlatform.Data.Runtime.RealAssetLeases`：真实配置管理器与显式提供的已保存独占开发夹具、两个隔离实例。通过 `NewObject<UGameInstance>` → `InitializeStandalone` → 自动子系统集合 → `IGamePlatformDataService::Get` 实际取得门面，不手动创建数据子系统。覆盖申请即取消、引擎已接纳请求分组后/终态前取消、重复释放、错作用域/代次、待加载与成功世界租约销毁、A/B分组并集、A退出保留B、缓存/无新工作、单次终态，以及后来的外部分组/外部卸载保护。必须传 `-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1`（本轮父任务探针身份）或其他已存在的独占开发夹具；缺少资产或参数明确失败，不跳过并报告通过。测试使用真实加载服务，不伪造磁盘资产、不替换管理器；结束清理世界、引擎世界上下文和Shutdown实例，并显式撤销测试自己的外部加载、恢复测试前基线。
- `GamePlatform.Data.Runtime.RecursiveFailureRollback`（编辑器模块承载，测试运行期服务）：四个唯一GUID路径下的真实内存定义通过AssetCreated加入注册表，再使用实际 `ScanPathsForPrimaryAssets`，逐一核对主资产映射；经真实GI门面申请共享依赖和两种错误根，断言 `MissingDefinition` / `DependencyCycle`、仅一次终态、无等待遗留、失败根不可读取、其他成功租约仍可读，以及失败图的UI需求撤销后Core/Server仍保留。只创建内存包，不调用SavePackage；结束使用精确GUID路径撤销主资产/扫描路径、注册表源对象和实例。静态依据为UE5.8 `SearchAssetRegistryPaths` 编辑器非Cook分支 `bIncludeOnlyOnDiskAssets=false`，不使用动态资产或模拟加载器替代实际扫描。

尚未补全的测试覆盖：包含真实Core/UI/Server软引用对象的分组保持/GC验证（现有集成断言是引擎分组状态和根定义可读性）；实际磁盘异步发现长期未就绪过程；完整多PIE与真实切图。引擎请求已开始后取消用例不保证实际磁盘IO仍在进行。没有为此扩大本轮四个正式资产范围。以上不因已有测试名称而视为完成；新GI门面与运行期错误图测试也只代表已有源码，不代表已经执行。

## 本次已执行验证与边界

2026-09-27 使用当前工作空间 `E:/work/2026/DivineBeastsWorkspace` 重新验证。原生需求账本使用 CMake 3.31.6-msvc6、VS2022 BuildTools、MSVC 19.44.35228、Windows SDK 10.0.26100.0；Debug／Release 均 1/1 CTest 通过，账本测试内部仍为20个断言。

UE5.8 锁定路径为 `D:/UnrealEngine-5.8.0-release`。通过引擎 `Build.bat` 实际完成：

- `DivineBeastsArenaEditor -Module=GamePlatformData`：Succeeded。
- `DivineBeastsArenaEditor -Module=GamePlatformDataEditor`：首次发现历史测试使用 UE5.8 已不存在的 `PKG_Transient`；改为符合新建未保存内存包语义的 `PKG_NewlyCreated` 后 Succeeded。
- `DivineBeastsArenaClient -Module=GamePlatformData`：Succeeded。
- `DivineBeastsArenaServer -Module=GamePlatformData`：Succeeded。

当前锁定源码引擎仍没有可启动的 `UnrealEditor.exe` / `UnrealEditor-Cmd.exe`，所以本轮没有实际运行 UE Automation。 `GamePlatform.Data.Runtime.RealAssetLeases` 还要求真实已保存 Definition 测试资产，而当前工程真实 `.uasset/.umap` 为0，因此不能伪造通过。

主工程当前 `GamePlatformDefinition` 仍扫描 `/Game/Development/Foundation/Definitions`，`CookRule=Unknown`、`ChunkId=-1`；这只是开发验证配置，不代表正式生产 Cook／Chunk 已完成。Server-safe 也必须由真实 AssetRegistry 依赖审计、Server Cook/Stage 证明，而不能由 Data 插件自报。

当前限制：已释放租约的完整签发记录仍保留至游戏实例销毁，以保证严格幂等释放；长寿命实例的记录会线性增长，本轮新增 `ReleasedLeaseRecords` 诊断字段显式观测该风险，但未擅自改变既有幂等合同。源查重仍可能扫描 AssetRegistry 类型，尚未做带失效通知的索引缓存和性能量化。Owner弱引用存活检查仍使用 CoreTicker，待真实规模性能数据决定是否调整检查频率。

详细证据见 [Docs/TestingAndEvidence.md](Docs/TestingAndEvidence.md)。

## 源码文件清单

- `GamePlatformData.uplugin`：唯一描述文件，运行及编辑器模块依赖/目标。
- `Source/GamePlatformData/GamePlatformData.Build.cs`：双端模块构建规则。
- `Source/GamePlatformData/Public/Definitions/GamePlatformPrimaryDataAsset.h`：主资产身份与源标签。
- `Source/GamePlatformData/Public/Definitions/GamePlatformDefinitionBase.h`：只读版本、依赖和校验接口。
- `Source/GamePlatformData/Public/Types/GamePlatformDataVersion.h`：数据版本值。
- `Source/GamePlatformData/Public/Types/GamePlatformDataLease.h`：租约、期限、请求状态和诊断值。
- `Source/GamePlatformData/Public/Types/GamePlatformDataLimits.h`：Runtime／Editor统一依赖图与Bundle安全上限。
- `Source/GamePlatformData/Public/Interfaces/IGamePlatformDataService.h`：游戏实例公开服务。
- `Source/GamePlatformData/Public/Loading/GamePlatformAssetManager.h`：工程配置所需反射类，登记实现保持私有。
- `Source/GamePlatformData/Public/Validation/GamePlatformDefinitionValidation.h`：唯一源验证契约。
- `Source/GamePlatformData/Private/GamePlatformDataModule.cpp`：运行模块入口。
- `Source/GamePlatformData/Private/Definitions/GamePlatformDefinitionBase.cpp`：身份、标签及版本实现。
- `Source/GamePlatformData/Private/Loading/DataNextTick.h`：游戏线程延后调度。
- `Source/GamePlatformData/Private/Loading/GamePlatformAssetManager.cpp`：进程需求聚合、基线保留与引擎加载。
- `Source/GamePlatformData/Private/Ownership/DataDemandLedger.h`：父任务已有生产账本，原文件保留并实际复用。
- `Source/GamePlatformData/Private/Subsystems/GamePlatformDataSubsystem.h` / `.cpp`：实例租约、依赖图与生命周期。
- `Source/GamePlatformData/Private/Validation/GamePlatformDefinitionValidation.cpp`：独立源资产扫描。
- `Source/GamePlatformData/Private/Tests/GamePlatformDefinitionTests.cpp`：定义行为自动化测试。
- `Source/GamePlatformData/Private/Tests/GamePlatformDataServiceTests.cpp`：真实服务潜伏集成测试。
- `Source/GamePlatformData/Private/Tests/GamePlatformDataSchedulingTests.cpp`：真实FTSTicker零Delta与嵌套调度回归。
- `Source/GamePlatformDataEditor/GamePlatformDataEditor.Build.cs`：编辑器构建规则。
- `Source/GamePlatformDataEditor/Private/GamePlatformDataEditorModule.cpp`：编辑器模块入口。
- `Source/GamePlatformDataEditor/Private/Validation/GamePlatformDataDefinitionValidator.h` / `.cpp`：GamePlatformData内部真实源对象递归验证；名称与DeveloperTools公开通用Definition验证器区分。
- `Source/GamePlatformDataEditor/Private/Tests/GamePlatformDefinitionValidationTests.cpp`：隔离注册表负例。
- `Source/GamePlatformDataEditor/Private/Tests/GamePlatformRuntimeDependencyTests.cpp`：内存源经真实扫描后的运行期租约递归失败与共享需求回滚。
- `Tests/CMakeLists.txt`：父任务已有独立原生测试入口，保留。
- `Tests/DataDemandTests.cpp`：原生账本20断言。
- `Docs/Architecture.md`：插件正式设计、Cook/Chunk/Server-safe与生命周期边界。
- `Docs/API.md`：Public API说明。
- `Docs/TestingAndEvidence.md`：本轮真实验证证据与未执行项。
- `README.md`：本阶段能力、精确接口、限制与证据。
