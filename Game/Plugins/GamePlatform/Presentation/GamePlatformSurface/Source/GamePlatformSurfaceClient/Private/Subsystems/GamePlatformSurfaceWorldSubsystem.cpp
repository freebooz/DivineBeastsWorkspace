// Surface世界服务：事件驱动缓存全局表现状态，并批量桥接到MPC。
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"

#include "Engine/World.h"
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
    BoundInstance.Reset();
    BoundCollection = nullptr;
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

    if (!bStateChanged && BoundInstance.IsValid())
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::Unchanged;
        Result.Revision = Revision;
        return Result;
    }

    // 普通状态事件复用绑定；只有显式Refresh命令重读配置，避免每次状态变化重复装载。
    if (ResolveMaterialBinding()) Result = PushCurrentStateToMaterialParameters();
    else
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
        Result.Revision = Revision;
    }
    if (bStateChanged)
    {
        StateChanged.Broadcast(CurrentState, Revision);
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
    BoundInstance.Reset();
    BoundCollection = nullptr;
    bLoggedBindingFailure = false;
    if (bClosing || !IsValid(GetWorld()) || !ResolveMaterialBinding())
    {
        Result.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
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
    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return false;
    }

    if (BoundCollection && BoundInstance.IsValid())
    {
        return true;
    }

    BoundInstance.Reset();
    BoundCollection = nullptr;

    const UGamePlatformSurfaceSettings* Settings = GetDefault<UGamePlatformSurfaceSettings>();
    UMaterialParameterCollection* Collection = Settings
        ? Settings->GlobalParameterCollection.LoadSynchronous()
        : nullptr;
    if (!IsValid(Collection))
    {
        if (!bLoggedBindingFailure && Settings && Settings->bWarnOnMaterialBindingFailure)
        {
            UE_LOG(
                LogGamePlatformSurface,
                Warning,
                TEXT("Surface全局MPC不可用；状态仍会缓存，但材质不会更新。请在编辑器中生成/配置MPC_GP_SurfaceGlobal。"));
            bLoggedBindingFailure = true;
        }
        return false;
    }

    UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection);
    if (!IsValid(Instance))
    {
        if (!bLoggedBindingFailure && Settings && Settings->bWarnOnMaterialBindingFailure)
        {
            UE_LOG(LogGamePlatformSurface, Warning, TEXT("当前世界无法取得Surface材质参数集合实例。"));
            bLoggedBindingFailure = true;
        }
        return false;
    }

    BoundCollection = Collection;
    BoundInstance = Instance;
    bLoggedBindingFailure = false;
    return true;
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

    return Result;
}
