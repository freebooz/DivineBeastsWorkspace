// SSLM项目开发目录：真实插件/资产发现、中央Data租约及完整快照发布，限游戏线程。
// 本实例拥有请求代次和租约；失败保留旧快照与诊断，不加载生产流程或创建第二资产管理器。
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Catalog/DivineBeastsDemoCatalogSnapshot.h"
#include "Scenarios/DivineBeastsDemoAdapterRegistry.h"
#include "Scenarios/DivineBeastsDemoSampleAdapters.h"
#include "Scenarios/DivineBeastsDemoCoreIdentityScenario.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Definitions/DivineBeastsDemoCatalogDefinition.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

/** 私有实例状态：已发布快照与候选租约分离，晚回调只释放自己的请求。 */
struct FDivineBeastsDemoCatalogPrivateState
{
    DivineBeasts::Demo::FCatalogSnapshot Snapshot;
    FGamePlatformDataLease ActiveLease;
    FGamePlatformDataLease PendingLease;
    TArray<FDivineBeastsDemoScenario> DiagnosticMaps;
    FGamePlatformResult Result;
    uint64 Generation = 0;
    bool bClosing = false;
    DivineBeasts::Demo::FAdapterRegistry Adapters;
    FName PendingScenarioId;
    FName PendingMap;
};

namespace
{
/** 分类只读取真实插件根；Content旧枚举保留序列化兼容，实际架构仍属Project。 */
bool ResolveLayer(const FString& Relative, EDivineBeastsDemoLayer& Out)
{
    if (Relative.StartsWith(TEXT("DivineBeasts/ContentPacks/"))) { Out = EDivineBeastsDemoLayer::Content; return true; }
    if (Relative.StartsWith(TEXT("DivineBeasts/"))) { Out = EDivineBeastsDemoLayer::Project; return true; }
    if (Relative.StartsWith(TEXT("MobaCommon/"))) { Out = EDivineBeastsDemoLayer::Moba; return true; }
    if (Relative.StartsWith(TEXT("GamePlatform/"))) { Out = EDivineBeastsDemoLayer::Platform; return true; }
    return false;
}
/** 完整签发身份比较，不只比较可复用槽位；回调RequestState可由Loading转终态。 */
bool SameLease(const FGamePlatformDataLease& A, const FGamePlatformDataLease& B)
{
    return A.IsValid() && B.IsValid() && A.ScopeId == B.ScopeId && A.LeaseId == B.LeaseId &&
        A.Generation == B.Generation && A.IssuerProof == B.IssuerProof &&
        A.DefinitionId == B.DefinitionId && A.Bundles == B.Bundles && A.ResourcePaths == B.ResourcePaths;
}
/** 追加中文预检阻塞；所有条件都检查，便于一次展示缺插件/资产/服务差别。 */
void Block(FDivineBeastsDemoPreflightResult& Out, const TCHAR* Code, const FString& Message)
{
    if (Out.BlockingReasons.IsEmpty()) Out.Result = FGamePlatformResult::Failure(FName(Code), Message);
    Out.BlockingReasons.Add(FText::FromString(Message));
}
}

bool UDivineBeastsDemoCatalogSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return Super::ShouldCreateSubsystem(Outer) && FDivineBeastsPluginDemoMode::IsEnabled();
}
void UDivineBeastsDemoCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    PrivateState = MakeShared<FDivineBeastsDemoCatalogPrivateState>();
    // 三种样板有真实职责和公开消费者；竞技扩展仍由可选组合根单向注入。
    RegisterScenarioAdapter(TEXT("ResourceBar"), [](UWorld&) { return DivineBeasts::Demo::CreateResourceBarScenario(); });
    RegisterScenarioAdapter(TEXT("BirdFlock"), [](UWorld&) { return DivineBeasts::Demo::CreateBirdFlockScenario(); });
    RegisterScenarioAdapter(TEXT("DataLease"), [](UWorld&) { return DivineBeasts::Demo::CreateDataLeaseScenario(); });
    RegisterScenarioAdapter(TEXT("CoreIdentity"), [](UWorld&) { return DivineBeasts::Demo::CreateCoreIdentityScenario(); });
    RefreshCatalog();
}
void UDivineBeastsDemoCatalogSubsystem::Deinitialize()
{
    if (PrivateState)
    {
        PrivateState->bClosing = true;
        ++PrivateState->Generation;
        // 先失效目录/事件，再撤销本实例两份需求，避免释放取消回调重入发布。
        PrivateState->Snapshot.Reset();
        PrivateState->Adapters.Reset();
        OnCatalogChanged.Clear();
        const auto Active = PrivateState->ActiveLease;
        const auto Pending = PrivateState->PendingLease;
        PrivateState->ActiveLease = {}; PrivateState->PendingLease = {};
        if (auto* Data = IGamePlatformDataService::Get(*GetGameInstance()))
        {
            if (Pending.IsValid()) Data->ReleaseDefinition(Pending);
            if (Active.IsValid()) Data->ReleaseDefinition(Active);
        }
    }
    PrivateState.Reset(); Plugins.Reset(); Scenarios.Reset(); LastOperationMessage = FText::GetEmpty();
    Super::Deinitialize();
}
void UDivineBeastsDemoCatalogSubsystem::RefreshCatalog()
{
    check(IsInGameThread());
    if (!PrivateState || PrivateState->bClosing || !FDivineBeastsPluginDemoMode::IsEnabled()) return;
    ++PrivateState->Generation;
    const uint64 Generation = PrivateState->Generation;
    auto* Data = IGamePlatformDataService::Get(*GetGameInstance());
    const auto PreviousPending = PrivateState->PendingLease;
    PrivateState->PendingLease = {};
    if (Data && PreviousPending.IsValid()) Data->ReleaseDefinition(PreviousPending);

    TArray<FDivineBeastsDemoPlugin> FoundPlugins;
    FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir());
    FPaths::MakeStandardFilename(Root); Root.RemoveFromEnd(TEXT("/"));
    const FString Prefix = Root + TEXT("/");
    TSet<FName> Seen;
    for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
    {
        FString Directory = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
        FPaths::MakeStandardFilename(Directory);
        // ResolveLayer（所属层解析）虽在成功时写入，仍初始化明确默认值，避免跨编译器未初始化告警。
        EDivineBeastsDemoLayer Layer = EDivineBeastsDemoLayer::Platform;
        if (!Directory.StartsWith(Prefix, ESearchCase::IgnoreCase) || !ResolveLayer(Directory.RightChop(Prefix.Len()), Layer)) continue;
        const FName Id(*Plugin->GetName());
        if (Seen.Contains(Id))
        {
            PrivateState->Result = FGamePlatformResult::Failure(FName(TEXT("Demo.DuplicatePlugin")), TEXT("实际发现重复插件身份，拒绝刷新。"));
            LastOperationMessage = FText::FromString(PrivateState->Result.Message); OnCatalogChanged.Broadcast(); return;
        }
        Seen.Add(Id);
        FDivineBeastsDemoPlugin Item;
        Item.PluginId = Id; Item.DisplayName = FText::FromString(Plugin->GetDescriptor().FriendlyName);
        Item.Description = Plugin->GetDescriptor().Description; Item.Layer = Layer; Item.bEnabled = Plugin->IsEnabled();
        FoundPlugins.Add(MoveTemp(Item));
    }
    FoundPlugins.Sort([](const auto& A, const auto& B) { return A.PluginId.ToString() < B.PluginId.ToString(); });
    Plugins = MoveTemp(FoundPlugins);

    PrivateState->DiagnosticMaps.Reset();
    auto& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    TArray<FAssetData> Assets;
    Registry.GetAssetsByPath(FName(TEXT("/Game/Development")), Assets, true);
    TSet<FName> Maps;
    for (const auto& Asset : Assets)
    {
        if (Asset.AssetClassPath != UWorld::StaticClass()->GetClassPathName() || Maps.Contains(Asset.PackageName)) continue;
        // 门户不是可选择的业务示例；不把开发目录外或同名资产作为诊断场景。
        const FString Package = Asset.PackageName.ToString();
        if (!Package.StartsWith(TEXT("/Game/Development/")) ||
            Package == FDivineBeastsPluginDemoMode::GetHubMapPackage()) continue;
        Maps.Add(Asset.PackageName);
        FDivineBeastsDemoScenario Item;
        Item.ScenarioId = Asset.PackageName;
        Item.PluginId = TEXT("GamePlatformDeveloperTools"); // 诊断分组，不宣称地图所有者。
        Item.Title = FText::FromString(Asset.AssetName.ToString()); Item.MapAsset = Asset.ToSoftObjectPath();
        Item.bDiagnosticOnly = true; Item.bAvailable = true;
        Item.VerificationStatus = FText::FromString(TEXT("未编目开发地图，仅供诊断；不计正式功能覆盖或人工通过。"));
        PrivateState->DiagnosticMaps.Add(MoveTemp(Item));
    }
    RebuildScenarioProjection();
    if (!Data)
    {
        PrivateState->Result = FGamePlatformResult::Failure(FName(TEXT("Demo.DataUnavailable")), TEXT("中央Data服务尚未初始化；保留发现结果，可在世界就绪或人工刷新时重试。"));
        LastOperationMessage = FText::FromString(PrivateState->Result.Message); OnCatalogChanged.Broadcast(); return;
    }
    FGamePlatformId LogicalId;
    if (!FGamePlatformId::TryCreate(TEXT("divinebeasts.demo"), TEXT("catalog"), 1, LogicalId)) return;
    const FPrimaryAssetId Id(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*LogicalId.ToString()));
    FGamePlatformResult Accepted;
    TWeakObjectPtr<UDivineBeastsDemoCatalogSubsystem> WeakThis(this);
    PrivateState->PendingLease = Data->AcquireDefinition(Id, UDivineBeastsDemoCatalogDefinition::StaticClass(), {},
        EGamePlatformDataLifetime::Instance, this,
        [WeakThis, Generation](const auto& Lease, const auto& Result)
        {
            if (auto* Self = WeakThis.Get()) Self->OnCatalogDefinitionReady(Generation, Lease, Result);
        }, Accepted);
    PrivateState->Result = Accepted.IsSuccess() ? FGamePlatformResult() : Accepted;
    LastOperationMessage = FText::FromString(Accepted.IsSuccess()
        ? TEXT("目录定义正在由中央Data服务加载，功能列表暂不发布。") : Accepted.Message);
    OnCatalogChanged.Broadcast();
}
void UDivineBeastsDemoCatalogSubsystem::OnCatalogDefinitionReady(uint64 Generation,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    auto* Data = IGamePlatformDataService::Get(*GetGameInstance());
    if (!PrivateState || PrivateState->bClosing || Generation != PrivateState->Generation)
    {
        if (Data && Lease.IsValid()) Data->ReleaseDefinition(Lease);
        return;
    }
    // 同步拒绝可通知无效句柄；成功必须与本次完整租约一致。
    FGamePlatformResult Published = Result;
    if (Result.IsSuccess())
    {
        const auto* Definition = Data && SameLease(Lease, PrivateState->PendingLease)
            ? Cast<UDivineBeastsDemoCatalogDefinition>(Data->GetLoadedDefinition(Lease)) : nullptr;
        Published = Definition ? PrivateState->Snapshot.Publish(*Definition, Plugins)
            : FGamePlatformResult::Failure(FName(TEXT("Demo.CatalogLeaseInvalid")), TEXT("目录加载完成但完整租约或定义类型不匹配。"));
        if (Published.IsSuccess() && Definition)
            for (auto& Plugin : Plugins)
                for (const auto& Entry : Definition->Plugins)
                    if (Plugin.PluginId == Entry.PluginId) Plugin.FeatureAuditStatus = Entry.FeatureAuditStatus;
    }
    PrivateState->PendingLease = {};
    if (Published.IsSuccess())
    {
        const auto Old = PrivateState->ActiveLease;
        PrivateState->ActiveLease = Lease;
        if (Data && Old.IsValid()) Data->ReleaseDefinition(Old);
    }
    else if (Data && Lease.IsValid()) Data->ReleaseDefinition(Lease);
    PrivateState->Result = Published;
    LastOperationMessage = FText::FromString(Published.IsSuccess()
        ? TEXT("完整功能目录已发布；各案例仍需独立预检和实际运行。") : Published.Message);
    RebuildScenarioProjection();
    OnCatalogChanged.Broadcast();
}
void UDivineBeastsDemoCatalogSubsystem::RebuildScenarioProjection()
{
    Scenarios.Reset();
    for (auto& Plugin : Plugins) Plugin.DiscoveredMapCount = 0;
    if (!PrivateState) return;
    TSet<FName> RegisteredMaps;
    for (const auto& Definition : PrivateState->Snapshot.GetAllScenarios())
    {
        FDivineBeastsDemoScenario Item;
        Item.ScenarioId = Definition.ScenarioId; Item.Title = Definition.Title; Item.MapAsset = Definition.MapAsset;
        for (const auto& Feature : PrivateState->Snapshot.GetAllFeatures())
            if (Feature.FeatureId == Definition.FeatureId) { Item.PluginId = Feature.PluginId; break; }
        const auto Check = PreflightScenario(Item.ScenarioId);
        Item.bAvailable = Check.bCanStart;
        Item.VerificationStatus = FText::FromString(Check.bCanStart ? TEXT("预检通过，尚未运行/人工核验。") : Check.Result.Message);
        RegisteredMaps.Add(FName(*Item.MapAsset.GetLongPackageName()));
        Scenarios.Add(MoveTemp(Item));
    }
    for (const auto& Diagnostic : PrivateState->DiagnosticMaps)
        if (!RegisteredMaps.Contains(FName(*Diagnostic.MapAsset.GetLongPackageName()))) Scenarios.Add(Diagnostic);
    for (auto& Plugin : Plugins)
    {
        TSet<FName> UniqueMaps;
        for (const auto& Scenario : Scenarios)
            if (Scenario.PluginId == Plugin.PluginId) UniqueMaps.Add(FName(*Scenario.MapAsset.GetLongPackageName()));
        Plugin.DiscoveredMapCount = UniqueMaps.Num();
    }
}
TArray<FDivineBeastsDemoScenario> UDivineBeastsDemoCatalogSubsystem::GetScenarios(FName PluginId) const
{
    TArray<FDivineBeastsDemoScenario> Result;
    for (const auto& Item : Scenarios) if (Item.PluginId == PluginId) Result.Add(Item);
    return Result;
}
TArray<FDivineBeastsDemoFeatureEntry> UDivineBeastsDemoCatalogSubsystem::GetFeatures(FName PluginId) const
{
    return PrivateState ? PrivateState->Snapshot.GetFeatures(PluginId) : TArray<FDivineBeastsDemoFeatureEntry>();
}
TArray<FDivineBeastsDemoScenarioDefinition> UDivineBeastsDemoCatalogSubsystem::GetFeatureScenarios(FName FeatureId) const
{
    return PrivateState ? PrivateState->Snapshot.GetFeatureScenarios(FeatureId) : TArray<FDivineBeastsDemoScenarioDefinition>();
}
bool UDivineBeastsDemoCatalogSubsystem::FindScenarioDefinition(FName ScenarioId, FDivineBeastsDemoScenarioDefinition& Out) const
{
    const auto* Found = PrivateState ? PrivateState->Snapshot.FindScenario(ScenarioId) : nullptr;
    if (!Found) { Out = {}; return false; }
    Out = *Found; return true;
}
FGamePlatformResult UDivineBeastsDemoCatalogSubsystem::GetCatalogResult() const
{
    return PrivateState ? PrivateState->Result : FGamePlatformResult();
}
TArray<FDivineBeastsDemoReviewCaseDefinition> UDivineBeastsDemoCatalogSubsystem::GetReviewCases() const
{
    return PrivateState ? PrivateState->Snapshot.GetReviewCases() : TArray<FDivineBeastsDemoReviewCaseDefinition>();
}
FString UDivineBeastsDemoCatalogSubsystem::GetSourceFingerprint() const
{
    return PrivateState ? PrivateState->Snapshot.GetSourceFingerprint() : FString();
}
FDivineBeastsDemoPreflightResult UDivineBeastsDemoCatalogSubsystem::PreflightScenario(FName ScenarioId) const
{
    FDivineBeastsDemoPreflightResult Out;
    FDivineBeastsDemoScenarioDefinition Scenario;
    if (!FindScenarioDefinition(ScenarioId, Scenario))
    {
        Block(Out, TEXT("Demo.ScenarioNotRegistered"), TEXT("案例未登记或目录尚未完整就绪。")); return Out;
    }
    if (!FDivineBeastsPluginDemoMode::IsEnabled()) Block(Out, TEXT("Demo.ModeDisabled"), TEXT("未启用受控开发演示入口。"));
#if UE_EDITOR
    const FName Target(TEXT("Editor"));
#elif UE_CLIENT
    const FName Target(TEXT("Client"));
#else
    const FName Target(TEXT("Game"));
#endif
    if (!Scenario.Targets.Contains(Target)) Block(Out, TEXT("Demo.TargetNotAllowed"), TEXT("当前目标不在案例允许列表。"));
    for (FName Required : Scenario.RequiredPlugins)
    {
        const auto* Plugin = Plugins.FindByPredicate([Required](const auto& Item) { return Item.PluginId == Required; });
        if (!Plugin || !Plugin->bEnabled) Block(Out, TEXT("Demo.PluginDisabled"), TEXT("必需插件未启用：") + Required.ToString());
    }
    if (!GetGameInstance() || !IGamePlatformDataService::Get(*GetGameInstance()))
        Block(Out, TEXT("Demo.DataUnavailable"), TEXT("中央Data服务不可用。"));
    TArray<FSoftObjectPath> Assets = Scenario.DefinitionAssets;
    if (!Scenario.MapAsset.IsNull()) Assets.Add(Scenario.MapAsset);
    if (!Scenario.WidgetAsset.IsNull()) Assets.Add(Scenario.WidgetAsset);
    for (const auto& Asset : Assets)
        if (!FPackageName::DoesPackageExist(Asset.GetLongPackageName()))
            Block(Out, TEXT("Demo.AssetMissing"), TEXT("必需资产包缺失：") + Asset.GetLongPackageName());
    if (!PrivateState || !PrivateState->Adapters.Contains(Scenario.AdapterId))
        Block(Out, TEXT("Demo.AdapterUnavailable"), TEXT("案例适配器尚未注册：") + Scenario.AdapterId.ToString());
    Out.bCanStart = Out.BlockingReasons.IsEmpty();
    if (Out.bCanStart) Out.Result = FGamePlatformResult::Success();
    return Out;
}
bool UDivineBeastsDemoCatalogSubsystem::OpenScenario(FName InputId)
{
    if (!PrivateState || !FDivineBeastsPluginDemoMode::IsEnabled() || !GetWorld()) return false;
    FName Resolved;
    const auto Resolution = PrivateState->Snapshot.ResolveIdentity(InputId, Resolved);
    const auto* Item = Resolution.IsSuccess() ? Scenarios.FindByPredicate([Resolved](const auto& Row) { return Row.ScenarioId == Resolved; }) : nullptr;
    if (!Item)
    {
        // 保留原诊断地图入口，但显式旧路由歧义必须拒绝，不能退回地图直接启动。
        if (Resolution.Code == FName(TEXT("Demo.ScenarioSelectionRequired")))
        { LastOperationMessage = FText::FromString(Resolution.Message); return false; }
        Item = Scenarios.FindByPredicate([InputId](const auto& Row) { return Row.bDiagnosticOnly && Row.ScenarioId == InputId; });
    }
    if (!Item || !Item->bAvailable || (!Item->MapAsset.IsNull() && !FPackageName::DoesPackageExist(Item->MapAsset.GetLongPackageName())))
    { LastOperationMessage = FText::FromString(Item ? Item->VerificationStatus.ToString() : Resolution.Message); return false; }
    const auto Preflight = Item->bDiagnosticOnly ? FDivineBeastsDemoPreflightResult() : PreflightScenario(Item->ScenarioId);
    if (!Item->bDiagnosticOnly && !Preflight.bCanStart)
    { LastOperationMessage = FText::FromString(Preflight.Result.Message); return false; }
    PrivateState->PendingScenarioId = Item->bDiagnosticOnly ? NAME_None : Item->ScenarioId;
    PrivateState->PendingMap = Item->MapAsset.IsNull() ? FName(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName())) : FName(*Item->MapAsset.GetLongPackageName());
    if (auto* Runner = GetWorld()->GetSubsystem<UDivineBeastsDemoWorldSubsystem>(); Runner && Runner->HasActiveScenario())
        return Runner->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("切换案例，先清理当前运行。")));
    return ContinuePendingNavigation();
}
bool UDivineBeastsDemoCatalogSubsystem::ReturnToHub()
{
    if (!FDivineBeastsPluginDemoMode::IsEnabled() || !GetWorld()) return false;
    const FString Map = FDivineBeastsPluginDemoMode::GetHubMapPackage();
    if (!FPackageName::DoesPackageExist(Map))
    { LastOperationMessage = FText::FromString(TEXT("总览地图资产缺失。")); return false; }
    if (!PrivateState) return false;
    PrivateState->PendingScenarioId = NAME_None; PrivateState->PendingMap = FName(*Map);
    if (auto* Runner = GetWorld()->GetSubsystem<UDivineBeastsDemoWorldSubsystem>(); Runner && Runner->HasActiveScenario())
        return Runner->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("返回总览，先清理本案例。")));
    return ContinuePendingNavigation();
}

FDivineBeastsDemoAdapterRegistrationHandle UDivineBeastsDemoCatalogSubsystem::RegisterScenarioAdapter(FName Id, FDivineBeastsDemoAdapterFactory Factory)
{
    if (!PrivateState || PrivateState->bClosing) return {};
    const auto Handle = PrivateState->Adapters.Register(Id, MoveTemp(Factory));
    if (Handle.IsValid()) { RebuildScenarioProjection(); OnCatalogChanged.Broadcast(); }
    return Handle;
}
FGamePlatformResult UDivineBeastsDemoCatalogSubsystem::UnregisterScenarioAdapter(FDivineBeastsDemoAdapterRegistrationHandle Handle)
{
    if (!PrivateState) return FGamePlatformResult::Failure(FName(TEXT("Demo.CatalogClosed")), TEXT("目录实例已退出。"));
    const FName Id = PrivateState->Adapters.FindIdentity(Handle);
    const auto Result = PrivateState->Adapters.Unregister(Handle);
    if (Result.IsSuccess())
    {
        if (auto* Runner = GetWorld() ? GetWorld()->GetSubsystem<UDivineBeastsDemoWorldSubsystem>() : nullptr;
            Runner && Runner->HasActiveScenario() && Runner->GetActiveAdapterId() == Id)
            Runner->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("适配提供者已撤销。")));
        RebuildScenarioProjection(); OnCatalogChanged.Broadcast();
    }
    return Result;
}
TUniquePtr<IDivineBeastsDemoScenarioAdapter> UDivineBeastsDemoCatalogSubsystem::CreateScenarioAdapter(FName Id, UWorld& World) const
{
    return PrivateState && !PrivateState->bClosing && World.GetGameInstance() == GetGameInstance()
        ? PrivateState->Adapters.Create(Id, World) : nullptr;
}
FName UDivineBeastsDemoCatalogSubsystem::GetPendingScenarioId() const { return PrivateState ? PrivateState->PendingScenarioId : NAME_None; }
bool UDivineBeastsDemoCatalogSubsystem::ContinuePendingNavigation()
{
    if (!PrivateState || !GetWorld() || PrivateState->PendingMap.IsNone()) return false;
    const FName Map = PrivateState->PendingMap;
    const FName Current(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName()));
    PrivateState->PendingMap = NAME_None;
    if (Current == Map && !PrivateState->PendingScenarioId.IsNone())
    { StartPendingScenario(*GetWorld()); return true; }
    LastOperationMessage = FText::FromString(TEXT("清理完成，已提交地图请求，目标世界准备仍待确认。"));
    UGameplayStatics::OpenLevel(GetWorld(), Map); return true;
}
void UDivineBeastsDemoCatalogSubsystem::StartPendingScenario(UWorld& World)
{
    if (!PrivateState || PrivateState->PendingScenarioId.IsNone() || World.GetGameInstance() != GetGameInstance()) return;
    FDivineBeastsDemoScenarioDefinition Scenario;
    if (!FindScenarioDefinition(PrivateState->PendingScenarioId, Scenario)) return;
    if (!Scenario.MapAsset.IsNull() && FName(*Scenario.MapAsset.GetLongPackageName()) !=
        FName(*UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()))) return;
    const auto Check = PreflightScenario(Scenario.ScenarioId);
    if (!Check.bCanStart) { LastOperationMessage = FText::FromString(Check.Result.Message); return; }
    auto* Runner = World.GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
    if (Runner && Runner->BeginScenario(Scenario)) PrivateState->PendingScenarioId = NAME_None;
}
void UDivineBeastsDemoCatalogSubsystem::OnScenarioStopped(UWorld& World, const FDivineBeastsDemoRunSnapshot& Snapshot)
{
    if (!PrivateState || World.GetGameInstance() != GetGameInstance()) return;
    if (auto* Runner = World.GetSubsystem<UDivineBeastsDemoWorldSubsystem>(); Runner && Runner->HasActiveScenario()) return;
    if (!Snapshot.CleanupResult.IsSuccess())
    {
        PrivateState->PendingMap = NAME_None; PrivateState->PendingScenarioId = NAME_None;
        LastOperationMessage = FText::FromString(Snapshot.CleanupResult.Message); OnCatalogChanged.Broadcast(); return;
    }
    ContinuePendingNavigation();
}
