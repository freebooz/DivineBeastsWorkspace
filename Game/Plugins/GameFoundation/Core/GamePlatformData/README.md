# GamePlatformData（平台数据插件）

日期：2026-09-21。阶段：Foundation M0 Task02 源码已写入，独立审查发现的调度Critical及外部加载保护已修改，待UE验证；存在下文列出的集成测试覆盖缺口，不声明整体源码验收完成。

## 职责与接入

双端 `GamePlatformData` 提供稳定主资产身份、只读定义、结构版本检查、实例门面及进程需求租约。`GamePlatformDataEditor` 仅参与编辑器目标，提供原生 Data Validation 验证器。依赖仅为 Core、CoreUObject、Engine、AssetRegistry、GamePlatformCore；编辑器另依赖 DataValidation 与 UnrealEd。没有项目、MOBA、UI、后端、流程或VFX实现依赖。

主工程由父任务增量配置一次 `/Script/GamePlatformData.GamePlatformAssetManager`，扫描类型 `GamePlatformDefinition`，基类 `/Script/GamePlatformData.GamePlatformDefinitionBase`，普通定义资产而非蓝图类主资产。扫描目录和烘焙规则归主工程；RequiredDefinitions 是主资产身份引用，不能假设这些ID自动产生软引用烘焙依赖，所有必需定义仍须纳入正确扫描/烘焙范围。插件不替换引擎全局对象、不修改主工程配置。

`UGamePlatformPrimaryDataAsset` 继承 `UPrimaryDataAsset`。其 `LogicalId` 的规范字符串决定 `GamePlatformDefinition:<逻辑身份>`，与资产文件名、路径和具体C++类名无关。非法身份返回无效主资产ID。源标签固定为 `GamePlatformLogicalId`。

`UGamePlatformDefinitionBase` 的 `DataVersion.SchemaVersion` 与 `DataVersion.ContentRevision` 均默认1，可编辑默认值、蓝图只读。可读结构范围由具体类 CDO 的 `GetMinimumReadableSchemaVersion()` / `GetMaximumReadableSchemaVersion()` 代码决定，默认1..1。派生类扩展 `ValidateDefinition()` 时必须调用 Super，不能用资产字段自行扩大可读范围。

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

每个根租约逐项真实加载 `RequiredDefinitions`，依赖复用同一主租约需求键与分组。显式DFS栈检测环和缺失；每条路径最多128层、每次请求最多4096个唯一节点，超出返回 `DependencyGraphLimit`，不靠C++无限递归。每个实际对象检查主资产身份、根预期类型、类CDO结构范围、内容修订和定义扩展校验。任一节点失败释放本根租约的所有已登记需求，其他租约不受影响。

注册表发现未完成时请求保持Loading并延后重试，可以取消；实例创建早于管理器的特殊宿主允许后续申请重新读取同一个引擎管理器，不永久缓存空值。正常启动顺序已只读核对UE5.8源码：`UEngine::InitializeObjectReferences` 创建资产管理器，`UGameEngine::Init` 先调用 `UEngine::Init` 再 `InitializeStandalone` 创建游戏实例，`GetIfInitialized` 实际返回 `GEngine->AssetManager`。

## 源验证与测试

`ResolveUniqueGamePlatformDefinitionSource` 通过 AssetRegistry `GetAssetsByClass(..., true)` 扫描平台主资产及派生类，按源标签独立查重，不依赖已经去重的主资产字典。编辑器候选对象以未保存的当前逻辑身份替换同路径旧标签。运行期还核对唯一源路径与引擎主资产映射一致。

原生 `UEditorValidatorBase` 派生类由编辑器验证子系统发现，使用UE5.8的 `CanValidateAsset_Implementation(const FAssetData&, UObject*, FDataValidationContext&) const` 与对应 `ValidateLoadedAsset_Implementation` 签名。真实验证入口返回Valid/Invalid，并加入错误诊断。编辑器只为验证同步取得依赖源对象，不登记运行期长期租约。

已写入的UE自动化测试均 **未执行**：

- `GamePlatform.Data.Definition.IdentityAndVersion`：稳定身份、结构范围、修订与非法依赖ID。
- `GamePlatform.Data.Editor.SourceDuplicate`：隔离内存包中的不同真实资产对象登记到真实注册表，验证重复身份双方失败。
- `GamePlatform.Data.Editor.DependencyGraph`：真实依赖成功、缺失、子定义版本/修订非法、自环、间接环及必填身份。
- `GamePlatform.Data.Editor.DependencyDepthLimit`：129层实际定义图失败，不无限递归。
- `GamePlatform.Data.Editor.RegisteredValidatorDispatch`：先确认编辑器自动登记平台验证器，再通过真实EditorValidatorSubsystem调度重复身份负例；不手动注册掩盖发现问题。
- `GamePlatform.Data.Runtime.DeferredRequeueZeroDelta` / `NestedTickerNotification`：直接使用真实FTSTicker与生产调度函数，Tick(0)验证持续重排和嵌套提交不能同轮执行工作。持续未发现采用调度层未就绪条件，不是实际AssetRegistry发现过程测试。
- `GamePlatform.Data.Runtime.RealAssetLeases`：真实配置管理器与显式提供的已保存独占开发夹具、两个隔离实例。覆盖申请即取消、引擎已接纳请求分组后/终态前取消、重复释放、错作用域/代次、待加载与成功世界租约销毁、A/B分组并集、A退出保留B、缓存/无新工作、单次终态，以及后来的外部分组/外部卸载保护。必须传 `-GamePlatformDataTestDefinition=GamePlatformDefinition:foundation.probe@1`（本轮父任务探针身份）或其他已存在的独占开发夹具；缺少资产或参数明确失败，不跳过并报告通过。测试使用真实加载服务，不伪造磁盘资产、不替换管理器；结束显式撤销测试自己的外部加载，恢复测试前基线。

尚未补全的测试覆盖：通过真实UGameInstance内部子系统集合初始化并调用公开 `IGamePlatformDataService::Get` 的实例集成；运行期服务递归缺失/环的加载失败回滚（目前是编辑器真实对象图负例和原生账本回滚）；包含真实Core/UI/Server软引用对象的分组保持/GC验证（现有集成断言是引擎分组状态和根定义可读性）；实际磁盘异步发现长期未就绪过程；完整多PIE与真实切图。引擎请求已开始后取消用例不保证实际磁盘IO仍在进行。以上不因已有测试名称而视为完成。

## 本次已执行验证与边界

工作区为 `E:/poject/feebooz/DivineBeastsWorkspace`。源码仅修改本插件目录；按父任务后续明确要求，生成证据放工作区 `Saved/Validation/FoundationM0`。最终使用该目录已有CMake缓存确认的 CMake 4.3.2、Visual Studio 17 2022、MSVC 19.38.33145.0 运行Debug和Release原生账本；这不是UE编译或调度修复运行验收。以下命令从工作区根执行，本次退出码均为0：

```powershell
cmake -S Game/Plugins/GameFoundation/Core/GamePlatformData/Tests -B Saved/Validation/FoundationM0/DataNative
cmake --build Saved/Validation/FoundationM0/DataNative --config Debug
ctest --test-dir Saved/Validation/FoundationM0/DataNative -C Debug -V --output-log E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/FoundationM0/DataNative/DebugCTest.log
cmake --build Saved/Validation/FoundationM0/DataNative --config Release
ctest --test-dir Saved/Validation/FoundationM0/DataNative -C Release -V --output-log E:/poject/feebooz/DivineBeastsWorkspace/Saved/Validation/FoundationM0/DataNative/ReleaseCTest.log
```

结果：Debug、Release各CTest 1/1通过，各报告 `Cases=20 Failed=0`。保留原12个断言，新增多资产依赖需求、失败回滚不影响其他实例、未知调用者、空分组释放与过期租约隔离。生产账本原实现与原CMake入口保留。两份独立证据为工作区 `Saved/Validation/FoundationM0/DataNative/DebugCTest.log` 与 `ReleaseCTest.log`。这些20断言仅验证生产需求账本，不覆盖UE Ticker、UObject或加载适配。

初次误放插件 `Tests/Saved/Native` 的本轮产物，已核对创建时间及CMake源路径、Resolve后源和目标都在授权工作区、目标不存在，再精确Move至工作区 `Saved/Validation/FoundationM0/DataNativeSecondAttempt`，无覆盖或删除其他内容。归档保留原VS2026/MSVC19.51尝试的日志，不作为最终Debug/Release证据。Tests目前只保留测试源码与CMake入口文件，没有编译产物。迁移后的缓存仅作归档，不从旧绝对路径继续构建。

插件JSON解析通过。已按本机 `F:/UnrealEngine-5.8.0-release` 的引擎头/实现人工核对加载签名、空句柄语义、源标签Context路径、验证器签名、初始化顺序及公开UObject pimpl的析构/FVTableHelper构造离线定义。**这些静态核对不证明UHT或编译通过。**

UE反射生成、编辑器/客户端/服务器构建、上述UE自动化、DataValidation命令行、真实资产生成、烘焙、启动和多PIE均未执行。正式UBT仍受父任务确认的三个旧空插件描述文件阻塞；按明确要求保留原位，不绕过。主工程配置、项目探针、根目录规划与总体交付记录由父任务负责集成。

当前限制：已释放租约的完整签发记录保留至游戏实例销毁，保证严格幂等且拒绝伪造，会随长寿命实例的历史申请数线性增长；本次未擅自裁剪而破坏重复释放合同。后续若需要容量上限，应设计可审计的代次/退休记录契约与压力验证。源查重每次申请扫描源类型，尚未做性能测量和带失效通知的索引优化。所有UObject运行分支仍待引擎实测。

## 源码文件清单

- `GamePlatformData.uplugin`：唯一描述文件，运行及编辑器模块依赖/目标。
- `Source/GamePlatformData/GamePlatformData.Build.cs`：双端模块构建规则。
- `Source/GamePlatformData/Public/Definitions/GamePlatformPrimaryDataAsset.h`：主资产身份与源标签。
- `Source/GamePlatformData/Public/Definitions/GamePlatformDefinitionBase.h`：只读版本、依赖和校验接口。
- `Source/GamePlatformData/Public/Types/GamePlatformDataVersion.h`：数据版本值。
- `Source/GamePlatformData/Public/Types/GamePlatformDataLease.h`：租约、期限、请求状态和诊断值。
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
- `Source/GamePlatformDataEditor/Private/Validation/GamePlatformDefinitionValidator.h` / `.cpp`：真实源对象递归验证。
- `Source/GamePlatformDataEditor/Private/Tests/GamePlatformDefinitionValidationTests.cpp`：隔离注册表负例。
- `Tests/CMakeLists.txt`：父任务已有独立原生测试入口，保留。
- `Tests/DataDemandTests.cpp`：原生账本20断言。
- `README.md`：本阶段能力、精确接口、限制与证据。
