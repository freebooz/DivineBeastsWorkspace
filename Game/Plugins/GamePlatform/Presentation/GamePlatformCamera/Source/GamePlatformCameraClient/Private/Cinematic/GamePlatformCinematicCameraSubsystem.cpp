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
    if (!IsInGameThread() || !IsValid(Controller) || !IsValid(CameraActor) ||
        !IsValid(Profile) || !Profile->IsValidProfile() ||
        !World || World->bIsTearingDown || World->GetNetMode() == NM_DedicatedServer ||
        CameraActor->GetWorld() != World || !Controller->IsLocalController() ||
        (OwnedViewTarget.IsValid() && !IsCinematicViewActive()))
    {
        return false;
    }

    // 防止嵌套替换原始目标；调用方必须显式先退出再切其他展示机位。
    if (IsCinematicViewActive())
    {
        return OwnedViewTarget.Get() == CameraActor;
    }

    // CineCamera只允许使用现有Actor，非电影Actor也可借用UE原生ViewTarget混合。
    if (ACineCameraActor* CineActor = Cast<ACineCameraActor>(CameraActor))
    {
        UCineCameraComponent* CineComponent = CineActor->GetCineCameraComponent();
        if (CineComponent)
        {
            CineComponent->Filmback.SensorWidth = Profile->SensorWidthMm;
            CineComponent->SetCurrentFocalLength(Profile->FocalLengthMm);
            // 竞技默认不更改光圈或追焦，避免影响战斗判读。
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

    PreviousViewTarget = Controller->GetViewTarget();
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
