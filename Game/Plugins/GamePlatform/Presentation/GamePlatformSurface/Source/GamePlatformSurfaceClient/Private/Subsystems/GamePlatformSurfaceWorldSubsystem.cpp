// 本文件属于GamePlatform平台层 GamePlatformSurface，负责生产合同/实现。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// Surface世界服务：事件驱动缓存全局表现状态，并批量桥接到MPC。
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/App.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "Types/GamePlatformSurfaceParameterNames.h"

DEFINE_LOG_CATEGORY_STATIC(LogGamePlatformSurface, Log, All);

namespace
{
bool IsSupportedSurfaceWorldType(const EWorldType::Type WorldType)
{
    return WorldType == EWorldType::Game ||
           WorldType == EWorldType::PIE ||
           WorldType == EWorldType::GamePreview;
}
}

bool UGamePlatformSurfaceWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return IsValid(World) &&
           IsSupportedSurfaceWorldType(World->WorldType) &&
           World->GetNetMode() != NM_DedicatedServer &&
           !IsRunningCommandlet();
}

void UGamePlatformSurfaceWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bClosing = false;
    Revision = 0;
    CurrentState = FGamePlatformSurfaceEnvironmentState();

    // 默认状态也需要写入一次，避免PIE或世界重建后沿用上一世界的MPC实例值。
    if (ResolveMaterialBinding())
    {
        PushCurrentStateToMaterialParameters();
    }
}

void UGamePlatformSurfaceWorldSubsystem::Deinitialize()
{
    bClosing = true;
    StateChanged.Clear();
    ReleaseMaterialBinding();
    Super::Deinitialize();
}

FGamePlatformSurfaceUpdateResult UGamePlatformSurfaceWorldSubsystem::ApplyEnvironmentState(
    const FGamePlatformSurfaceEnvironmentState& State)
{
    check(IsInGameThread());

    FGamePlatformSurfaceUpdateResult Result;
    Result.Revision = Revision;

    UWorld* World = GetWorld();
    if (bClosing || !IsValid(World) || World->bIsTearingDown)
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::InvalidWorld;
        return Result;
    }
    if (!State.IsFinite())
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::InvalidState;
        return Result;
    }

    const FGamePlatformSurfaceEnvironmentState Sanitized = State.GetClamped();
    const bool bStateChanged = !CurrentState.IsNearlyEqual(Sanitized);
    if (bStateChanged)
    {
        CurrentState = Sanitized;
        ++Revision;
    }

    if (!bStateChanged && BoundInstance.IsValid() && LastBindingResult.Status == EGamePlatformSurfaceUpdateStatus::Applied)
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::Unchanged;
        Result.Revision = Revision;
        return Result;
    }

    // 普通状态事件复用绑定；只有显式Refresh命令重读配置，避免每次状态变化重复装载。
    if (ResolveMaterialBinding()) Result = PushCurrentStateToMaterialParameters();
    else
    {
        Result = LastBindingResult;
        Result.Revision = Revision;
    }
    if (bStateChanged)
    {
        const auto Snapshot = CurrentState; const int32 SnapshotRevision = Revision;
        StateChanged.Broadcast(Snapshot, SnapshotRevision);
    }
    return Result;
}

const FGamePlatformSurfaceEnvironmentState& UGamePlatformSurfaceWorldSubsystem::GetEnvironmentState() const
{
    return CurrentState;
}

int32 UGamePlatformSurfaceWorldSubsystem::GetRevision() const
{
    return Revision;
}

FGamePlatformSurfaceUpdateResult UGamePlatformSurfaceWorldSubsystem::RefreshMaterialBinding()
{
    check(IsInGameThread());

    FGamePlatformSurfaceUpdateResult Result;
    Result.Revision = Revision;

    // Refresh是显式重解析命令，配置即便更换或清空也不能沿用旧集合实例。
    ReleaseMaterialBinding();
    bBindingAttempted = false;
    bLoggedBindingFailure = false;
    if (bClosing || !IsValid(GetWorld()) || !ResolveMaterialBinding())
    {
        Result = LastBindingResult;
        Result.Revision = Revision;
        return Result;
    }

    return PushCurrentStateToMaterialParameters();
}

FDelegateHandle UGamePlatformSurfaceWorldSubsystem::AddStateChangedHandler(
    const FGamePlatformSurfaceStateChanged::FDelegate& Handler)
{
    check(IsInGameThread());
    return Handler.IsBound() ? StateChanged.Add(Handler) : FDelegateHandle();
}

void UGamePlatformSurfaceWorldSubsystem::RemoveStateChangedHandler(const FDelegateHandle Handle)
{
    check(IsInGameThread());
    if (Handle.IsValid())
    {
        StateChanged.Remove(Handle);
    }
}

bool UGamePlatformSurfaceWorldSubsystem::ResolveMaterialBinding()
{
    if (BoundCollection && BoundInstance.IsValid()) return true;
    if (bBindingAttempted) return false; // 缺失/失败保持诊断，普通天气事件不能同步反复查资产。
    bBindingAttempted = true;
    LastBindingResult.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    const auto* Settings = GetDefault<UGamePlatformSurfaceSettings>();
    const FSoftObjectPath Path = Settings ? Settings->GlobalParameterCollection.ToSoftObjectPath() : FSoftObjectPath();
    if (bClosing || !World || World->bIsTearingDown || !Data || !Path.IsValid()) return false;
    const int64 RequestedGeneration = ++BindingGeneration;
    const TWeakObjectPtr<UGamePlatformSurfaceWorldSubsystem> WeakThis(this);
    FGamePlatformResult Accepted;
    BindingLease = Data->AcquireResources({Path}, EGamePlatformDataLifetime::World, this,
        [WeakThis, RequestedGeneration](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
        {
            if (auto* Self = WeakThis.Get()) Self->HandleMaterialBindingLoaded(RequestedGeneration, Lease, Result);
        }, Accepted);
    if (Accepted.IsSuccess() && BindingLease.IsValid())
        LastBindingResult.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingPending;
    return false;
}

void UGamePlatformSurfaceWorldSubsystem::HandleMaterialBindingLoaded(const int64 RequestedGeneration,
    const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr;
    if (bClosing || !World || World->bIsTearingDown || RequestedGeneration != BindingGeneration ||
        !Lease.IsValid() || Lease.LeaseId != BindingLease.LeaseId || Lease.Generation != BindingLease.Generation ||
        Lease.ScopeId != BindingLease.ScopeId || Lease.IssuerProof != BindingLease.IssuerProof || Lease.ResourcePaths != BindingLease.ResourcePaths) return;
    LastBindingResult.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
    if (Result.IsSuccess() && Data && Data->GetLeaseState(Lease) == EGamePlatformDataRequestState::Succeeded && Lease.ResourcePaths.Num() == 1)
    {
        auto* Collection = Cast<UMaterialParameterCollection>(Lease.ResourcePaths[0].ResolveObject());
        auto* Instance = Collection ? World->GetParameterCollectionInstance(Collection) : nullptr;
        if (IsValid(Collection) && IsValid(Instance))
        {
            BoundCollection = Collection; BoundInstance = Instance;
            LastBindingResult = PushCurrentStateToMaterialParameters();
            return;
        }
    }
    // 失败只释放本次普通资源需求；保留稳定失败状态，显式Refresh才允许新一代重试。
    if (Data) Data->ReleaseResources(Lease);
    BindingLease = {};
    if (!bLoggedBindingFailure && GetDefault<UGamePlatformSurfaceSettings>()->bWarnOnMaterialBindingFailure)
    {
        UE_LOG(LogGamePlatformSurface, Warning, TEXT("Surface MPC未配置、加载失败或类型不符；状态已缓存，需完成真实材质链后显式Refresh。"));
        bLoggedBindingFailure = true;
    }
}

void UGamePlatformSurfaceWorldSubsystem::ReleaseMaterialBinding()
{
    ++BindingGeneration;
    const auto Lease = BindingLease; BindingLease = {};
    BoundInstance.Reset(); BoundCollection = nullptr;
    if (Lease.IsValid())
    {
        auto* World = GetWorld(); auto* GameInstance = World ? World->GetGameInstance() : nullptr;
        if (auto* Data = GameInstance ? IGamePlatformDataService::Get(*GameInstance) : nullptr) Data->ReleaseResources(Lease);
    }
    LastBindingResult.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
}

FGamePlatformSurfaceUpdateResult UGamePlatformSurfaceWorldSubsystem::PushCurrentStateToMaterialParameters()
{
    check(IsInGameThread());

    FGamePlatformSurfaceUpdateResult Result;
    Result.Revision = Revision;

    UMaterialParameterCollectionInstance* Instance = BoundInstance.Get();
    if (!IsValid(Instance))
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
        return Result;
    }

    const TPair<FName, float> Parameters[] =
    {
        { GamePlatformSurfaceParameters::GlobalWetness, CurrentState.GlobalWetness },
        { GamePlatformSurfaceParameters::GlobalSnowAmount, CurrentState.GlobalSnowAmount },
        { GamePlatformSurfaceParameters::GlobalSnowHeightCm, CurrentState.GlobalSnowHeightCm },
        { GamePlatformSurfaceParameters::GlobalMossInfluence, CurrentState.GlobalMossInfluence },
        { GamePlatformSurfaceParameters::GlobalPuddleAmount, CurrentState.GlobalPuddleAmount },
        { GamePlatformSurfaceParameters::RainIntensity, CurrentState.RainIntensity },
        { GamePlatformSurfaceParameters::SnowIntensity, CurrentState.SnowIntensity },
        { GamePlatformSurfaceParameters::TemperatureCelsius, CurrentState.TemperatureCelsius }
    };

    for (const TPair<FName, float>& Pair : Parameters)
    {
        if (Instance->SetScalarParameterValue(Pair.Key, Pair.Value))
        {
            ++Result.UpdatedParameterCount;
        }
        else
        {
            ++Result.FailedParameterCount;
        }
    }

    if (Result.FailedParameterCount > 0)
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::ParameterContractMismatch;
        UE_LOG(
            LogGamePlatformSurface,
            Warning,
            TEXT("Surface MPC参数契约不完整：成功写入%d项，失败%d项；请运行编辑器资产校验。"),
            Result.UpdatedParameterCount,
            Result.FailedParameterCount);
    }
    else
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::Applied;
    }

    LastBindingResult = Result;
    return Result;
}
