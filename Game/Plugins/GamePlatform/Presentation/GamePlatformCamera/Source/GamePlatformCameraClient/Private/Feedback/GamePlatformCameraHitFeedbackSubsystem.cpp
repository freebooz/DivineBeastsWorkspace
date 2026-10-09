#include "Feedback/GamePlatformCameraHitFeedbackSubsystem.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

void UGamePlatformCameraHitFeedbackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this, &UGamePlatformCameraHitFeedbackSubsystem::HandleWorldCleanup);
}

void UGamePlatformCameraHitFeedbackSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }
    ClearFeedbackHistory();
    Super::Deinitialize();
}

void UGamePlatformCameraHitFeedbackSubsystem::SetUserCameraShakeScale(float InScale)
{
    // 系统设置可由项目视图/玩家偏好调用；此处只存本地瞬时表现倍率。
    if (FMath::IsFinite(InScale))
    {
        UserCameraShakeScale = FMath::Clamp(InScale, 0.0f, 1.0f);
    }
}

bool UGamePlatformCameraHitFeedbackSubsystem::PlayHitCameraShake(
    const FGuid& EventId,
    TSubclassOf<UCameraShakeBase> ShakeClass,
    float Strength)
{
    ULocalPlayer* Player = GetLocalPlayer();
    UWorld* World = Player ? Player->GetWorld() : nullptr;
    if (!IsInGameThread() || !World || !EventId.IsValid() ||
        RecentEventIds.Contains(EventId) || !ShakeClass ||
        !FMath::IsFinite(Strength) || Strength <= 0.0f ||
        UserCameraShakeScale <= 0.0f ||
        World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    if (BoundWorld.IsValid() && BoundWorld.Get() != World)
    {
        ClearFeedbackHistory();
    }
    APlayerController* Controller = Player->GetPlayerController(World);
    APlayerCameraManager* Camera = Controller ? Controller->PlayerCameraManager : nullptr;
    if (!IsValid(Camera))
    {
        return false;
    }

    const double Now = FPlatformTime::Seconds();
    // 避免多人高频命中叠满镜头队列；真正的摇晃与回弹衰减由CameraShake资产定义。
    if (LastStartedAtSeconds > 0.0 && Now - LastStartedAtSeconds < 0.035)
    {
        return false;
    }

    const float FinalScale = FMath::Clamp(
        Strength * UserCameraShakeScale, 0.0f, 2.0f);
    if (!Camera->StartCameraShake(ShakeClass, FinalScale))
    {
        return false;
    }

    BoundWorld = World;
    LastStartedAtSeconds = Now;
    RecentEventIds.Add(EventId);
    RecentEventOrder.Add(EventId);
    if (RecentEventOrder.Num() > MaxRecentEvents)
    {
        RecentEventIds.Remove(RecentEventOrder[0]);
        RecentEventOrder.RemoveAt(0, 1, EAllowShrinking::No);
    }
    return true;
}

void UGamePlatformCameraHitFeedbackSubsystem::ClearFeedbackHistory()
{
    BoundWorld.Reset();
    RecentEventIds.Reset();
    RecentEventOrder.Reset();
    LastStartedAtSeconds = 0.0;
}

void UGamePlatformCameraHitFeedbackSubsystem::HandleWorldCleanup(
    UWorld* World, bool /*bSessionEnded*/, bool /*bCleanupResources*/)
{
    if (World && BoundWorld.Get() == World)
    {
        ClearFeedbackHistory();
    }
}
