# GamePlatformData（游戏平台数据）API说明

> 只描述稳定 Public（公开）契约；Private Subsystem（私有子系统）、需求账本和内部 Reconcile（协调）不是跨插件 API。

## 1. UGamePlatformPrimaryDataAsset（平台主数据资产）

头文件：`Definitions/GamePlatformPrimaryDataAsset.h`。

主要成员：

- `FGamePlatformId LogicalId`。
- `GetPrimaryAssetId()`。
- `GetAssetRegistryTags()`。
- `DefinitionAssetType()`。
- `LogicalIdTag()`。

所有平台 Definition 共享稳定主资产类型 `GamePlatformDefinition`。

## 2. UGamePlatformDefinitionBase（平台定义基类）

头文件：`Definitions/GamePlatformDefinitionBase.h`。

主要成员：

- `FGamePlatformDataVersion DataVersion`。
- `TArray<FPrimaryAssetId> RequiredDefinitions`。
- `GetMinimumReadableSchemaVersion()`。
- `GetMaximumReadableSchemaVersion()`。
- `ValidateDefinition()`。
- `GetAssetRegistryTags()`。
- `SchemaVersionTag()`。
- `ContentRevisionTag()`。
- `RequiredDefinitionCountTag()`。

派生类重写 `ValidateDefinition()` 时必须先调用 `Super::ValidateDefinition()`。

## 3. FGamePlatformDataVersion（平台数据版本）

头文件：`Types/GamePlatformDataVersion.h`。

字段：

- `SchemaVersion`：序列化/结构版本。
- `ContentRevision`：同结构的内容修订。

它不等于 FGamePlatformVersion（平台三段版本）或 FGamePlatformId.LogicalVersion（逻辑身份代次）。

## 4. FGamePlatformDataLease（平台数据租约）

头文件：`Types/GamePlatformDataLease.h`。

重要字段：

- `ScopeId`。
- `LeaseId`。
- `Generation`。
- `DefinitionId`。
- `Bundles`。
- `RequestState`。

`RequestState` 是取得该值时的快照；实时状态使用 `IGamePlatformDataService::GetLeaseState()`。

## 5. EGamePlatformDataLifetime（数据租约期限）

```text
Instance
World
```

Instance 可跨图；World 绑定申请时的实际 UWorld。

## 6. IGamePlatformDataService（游戏平台数据服务）

头文件：`Interfaces/IGamePlatformDataService.h`。

主要接口：

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

const UGamePlatformDefinitionBase* GetLoadedDefinition(
    const FGamePlatformDataLease& Lease) const;

FGamePlatformResult ReleaseDefinition(
    const FGamePlatformDataLease& Lease);

EGamePlatformDataRequestState GetLeaseState(
    const FGamePlatformDataLease& Lease) const;

FGamePlatformDataDiagnostics GetDiagnostics() const;
```

### AcquireDefinition 语义

- OutResult 只表示请求是否被同步接纳。
- 成功接纳后返回 `Loading` 租约。
- 最终成功/失败/取消总是延后通知，禁止同步重入。
- Completion 必须存在。
- Caller 必须属于同一 GameInstance。
- Bundle 集合允许为空，但不能包含 NAME_None。
- 去重后的 Bundle 数不能超过 `MaxBundlesPerLease`。

## 7. FGamePlatformDataDiagnostics（平台数据诊断）

当前包含：

```text
ScopeId
PendingRequests
ActiveLeases
TerminalRequests
UniqueTrackedDefinitions
UniqueRequestedBundles
ReleasedLeaseRecords
TotalAcceptedRequests
TotalRejectedRequests
TotalSucceededRequests
TotalFailedRequests
TotalCancelledRequests
LastResult
```

这些都是当前 GameInstance 作用域的值快照。

## 8. GamePlatformDataLimits（平台数据安全上限）

头文件：`Types/GamePlatformDataLimits.h`。

```text
MaxDependencyDepth = 128
MaxDefinitionsPerRequest = 4096
MaxBundlesPerLease = 64
```

Runtime 与 Editor 必须使用同一常量，禁止分别硬编码。

## 9. UGamePlatformAssetManager（游戏平台资产管理器）

头文件：`Loading/GamePlatformAssetManager.h`。

这是主工程配置的唯一 AssetManager 子类，不是业务插件直接操作的普通服务对象。

业务插件优先通过 `IGamePlatformDataService` 取得租约，而不是直接调用 AddDemand/RemoveDemand；这些接口保持 Private。

## 10. ResolveUniqueGamePlatformDefinitionSource（唯一源校验）

头文件：`Validation/GamePlatformDefinitionValidation.h`。

用于从 AssetRegistry 扫描并确认某个逻辑 Definition 只有一个真实源路径。

运行时 Registry 未就绪时显式返回失败，调用方按异步流程重试；编辑器验证可以等待注册表完成。