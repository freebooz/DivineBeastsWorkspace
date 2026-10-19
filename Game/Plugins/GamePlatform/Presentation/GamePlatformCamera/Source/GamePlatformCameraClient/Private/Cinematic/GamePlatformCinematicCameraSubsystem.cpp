// 只在显式进入/退出时调整本地机位，无Tick、无额外SceneCapture和每技能新建Actor。
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void UGamePlatformCinematicCameraSubsystem::ClearOwnedView()
{
    PreviousViewTarget.Reset();
    OwnedViewTarget.Reset();
    OwnedController.Reset();
    OwnedWorld.Reset();
    RestoreBlendTime = 0.0f;
}

void UGamePlatformCinematicCameraSubsystem::Deinitialize()
{
    // 本地玩家销毁时不向可能已销毁的旧世界发出视角切换。
    ClearOwnedView();
    Super::Deinitialize();
}

void UGamePlatformCinematicCameraSubsystem::PlayerControllerChanged(APlayerController* NewPlayerController)
{
    Super::PlayerControllerChanged(NewPlayerController);
    if (OwnedController.Get() != NewPlayerController)
    {
        // 重新附身/重连后的视角由新的Controller负责；绝不恢复旧世界ViewTarget。
        ClearOwnedView();
    }
}

bool UGamePlatformCinematicCameraSubsystem::IsCinematicViewActive() const
{
    APlayerController* Controller = OwnedController.Get();
    return IsValid(Controller) && OwnedWorld.IsValid() &&
        OwnedViewTarget.IsValid() && Controller->GetWorld() == OwnedWorld.Get() &&
        Controller->GetViewTarget() == OwnedViewTarget.Get();
}

bool UGamePlatformCinematicCameraSubsystem::BeginCinematicView(
    AActor* CameraActor, const UGamePlatformCinematicCameraProfile* Profile)
{
    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    UWorld* World = LocalPlayer ? LocalPlayer->GetWorld() : nullptr;
    APlayerController* Controller = LocalPlayer && World
        ? LocalPlayer->GetPlayerController(World) : nullptr;
    // PlayerCameraManager的GetViewTarget在混合时返回PendingViewTarget。
    // 本地玩家拥有镜头时可以直接更换现有电影机位，保留首次借用的恢复目标。
    const bool bAlreadyOwned = IsCinematicViewActive();
    if (!IsInGameThread() || !IsValid(Controller) || !IsValid(CameraActor) ||
        !IsValid(Profile) || !Profile->IsValidProfile() ||
        !World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer ||
        CameraActor->GetWorld() != World || !Controller->IsLocalController() ||
        (OwnedViewTarget.IsValid() && !bAlreadyOwned))
    {
        return false;
    }

    // 重复同一镜头不重放过渡；切换镜头时只更新目标，不覆盖最初的退出回退。
    if (bAlreadyOwned && OwnedViewTarget.Get() == CameraActor)
    {
        return true;
    }

    // 只操作已存在的原生电影相机，不逐技能生成Actor或复制任何玩家状态。
    if (ACineCameraActor* CineActor = Cast<ACineCameraActor>(CameraActor))
    {
        UCineCameraComponent* CineComponent = CineActor->GetCineCameraComponent();
        if (CineComponent)
        {
            CineComponent->Filmback.SensorWidth = Profile->SensorWidthMm;
            CineComponent->SetCurrentFocalLength(Profile->FocalLengthMm);
            // 竞技状态优先保持清晰和可操作性，不动态重设景深与追焦。
            if (Profile->Mode != EGamePlatformCameraPresentationMode::Arena)
            {
                CineComponent->SetCurrentAperture(Profile->Aperture);
                if (Profile->bEnableCinematicFocus)
                {
                    CineComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
                    CineComponent->FocusSettings.ManualFocusDistance = Profile->FocusDistanceCm;
                }
            }
        }
    }

    if (!bAlreadyOwned)
    {
        PreviousViewTarget = Controller->GetViewTarget();
    }
    OwnedViewTarget = CameraActor;
    OwnedController = Controller;
    OwnedWorld = World;
    RestoreBlendTime = Profile->BlendTimeSeconds;
    Controller->SetViewTargetWithBlend(CameraActor, RestoreBlendTime);
    return true;
}
bool UGamePlatformCinematicCameraSubsystem::EndCinematicView()
{
    if (!IsInGameThread())
    {
        return false;
    }
    APlayerController* Controller = OwnedController.Get();
    AActor* Previous = PreviousViewTarget.Get();
    const bool bCanRestore = IsCinematicViewActive() && IsValid(Previous) &&
        Previous->GetWorld() == OwnedWorld.Get() && Previous != OwnedViewTarget.Get();
    if (bCanRestore)
    {
        Controller->SetViewTargetWithBlend(Previous, RestoreBlendTime);
    }
    ClearOwnedView();
    return bCanRestore;
}
