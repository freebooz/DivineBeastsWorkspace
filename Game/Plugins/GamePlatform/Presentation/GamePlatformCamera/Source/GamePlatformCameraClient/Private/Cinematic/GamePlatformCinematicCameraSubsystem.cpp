// 只在显式进入/退出时调整本地机位，无Tick、无额外SceneCapture和每技能新建Actor。
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Camera/CameraComponent.h"
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

void UGamePlatformCinematicCameraSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UGamePlatformCinematicCameraSubsystem::HandleWorldCleanup);
}

void UGamePlatformCinematicCameraSubsystem::Deinitialize()
{
    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

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

void UGamePlatformCinematicCameraSubsystem::HandleWorldCleanup(
    UWorld* World, bool, bool)
{
    // 无缝切图可能复用LocalPlayer和PlayerController；旧World清理时必须释放弱所有权栅栏。
    if (World && OwnedWorld.Get() == World)
    {
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

bool UGamePlatformCinematicCameraSubsystem::ApplyProfileToCameraComponent(
    UCameraComponent* CameraComponent,
    const UGamePlatformCinematicCameraProfile* Profile)
{
    if (!IsInGameThread() || !IsValid(CameraComponent) ||
        !IsValid(Profile) || !Profile->IsValidProfile())
    {
        return false;
    }

    // 竞技视角保持原有构图与清晰度，不应用展示焦段、光圈或手动对焦。
    if (Profile->Mode == EGamePlatformCameraPresentationMode::Arena)
    {
        return true;
    }

    if (UCineCameraComponent* CineComponent = Cast<UCineCameraComponent>(CameraComponent))
    {
        CineComponent->Filmback.SensorWidth = Profile->SensorWidthMm;
        CineComponent->SetCurrentFocalLength(Profile->FocalLengthMm);
        CineComponent->SetCurrentAperture(Profile->Aperture);
        if (Profile->bEnableCinematicFocus)
        {
            CineComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
            CineComponent->FocusSettings.ManualFocusDistance = Profile->FocusDistanceCm;
        }
        return true;
    }

    // 通用预览舞台使用UCameraComponent；用36毫米传感器等效视场角呈现真实焦段构图。
    CameraComponent->SetFieldOfView(
        UGamePlatformCinematicCameraProfile::CalculateHorizontalFieldOfViewDegrees(
            Profile->FocalLengthMm, Profile->SensorWidthMm));
    return true;
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

    // 同一目标更新Profile时重新应用焦段，但不重放视角过渡。
    if (UCameraComponent* CameraComponent =
            CameraActor->FindComponentByClass<UCameraComponent>())
    {
        if (!ApplyProfileToCameraComponent(CameraComponent, Profile))
        {
            return false;
        }
    }

    if (bAlreadyOwned && OwnedViewTarget.Get() == CameraActor)
    {
        RestoreBlendTime = Profile->BlendTimeSeconds;
        return true;
    }

    // 只操作已经存在的镜头组件，不创建Actor或复制任何玩家状态。
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
    return EndCinematicViewInternal(nullptr, false);
}

bool UGamePlatformCinematicCameraSubsystem::EndCinematicViewFor(AActor* ExpectedCameraActor)
{
    return EndCinematicViewInternal(ExpectedCameraActor, true);
}

bool UGamePlatformCinematicCameraSubsystem::EndCinematicViewInternal(
    AActor* ExpectedCameraActor, bool bRequireExpectedCameraActor)
{
    if (!IsInGameThread() ||
        (bRequireExpectedCameraActor &&
            (!IsValid(ExpectedCameraActor) || OwnedViewTarget.Get() != ExpectedCameraActor)))
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
