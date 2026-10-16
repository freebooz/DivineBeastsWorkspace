// 本地演示只处理视觉和输入；世界退出取消定时器、解绑当前Controller的演示输入。
// 镜头所有权和安全恢复归GamePlatformCameraClient，Dedicated Server无此模块和资产。
#include "Review/DivineBeastsCinematicCameraReviewActor.h"
#include "Cinematic/GamePlatformCinematicCameraSubsystem.h"
#include "Cinematic/GamePlatformCinematicCameraProfile.h"

#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSSLM_CinematicReview, Log, All);

ADivineBeastsCinematicCameraReviewActor::ADivineBeastsCinematicCameraReviewActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetReplicateMovement(false);
}

ACineCameraActor* ADivineBeastsCinematicCameraReviewActor::ResolveCamera(int32 Index) const
{
    switch (Index)
    {
    case 0: return Camera24mm.Get();
    case 1: return Camera35mm.Get();
    case 2: return Camera50mm.Get();
    case 3: return Camera85mm.Get();
    default: return nullptr;
    }
}

UGamePlatformCinematicCameraProfile* ADivineBeastsCinematicCameraReviewActor::ResolveProfile(int32 Index) const
{
    switch (Index)
    {
    case 0: return Profile24mm.Get();
    case 1: return Profile35mm.Get();
    case 2: return Profile50mm.Get();
    case 3: return Profile85mm.Get();
    default: return nullptr;
    }
}

UGamePlatformCinematicCameraSubsystem* ADivineBeastsCinematicCameraReviewActor::ResolveService() const
{
    APlayerController* Controller = BoundController.Get();
    ULocalPlayer* Player = IsValid(Controller) ? Controller->GetLocalPlayer() : nullptr;
    return Player ? Player->GetSubsystem<UGamePlatformCinematicCameraSubsystem>() : nullptr;
}

void ADivineBeastsCinematicCameraReviewActor::BeginPlay()
{
    Super::BeginPlay();
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }
    GetWorld()->GetTimerManager().SetTimerForNextTick(this,
        &ADivineBeastsCinematicCameraReviewActor::SetupLocalInput);
}

void ADivineBeastsCinematicCameraReviewActor::SetupLocalInput()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown || bInputBound)
    {
        return;
    }
    UGameInstance* Instance = World->GetGameInstance();
    ULocalPlayer* Player = Instance ? Instance->GetFirstGamePlayer() : nullptr;
    APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
    if (!IsValid(Controller) || !Controller->IsLocalController())
    {
        if (++InitAttempts < 25)
        {
            World->GetTimerManager().SetTimer(InitTimer, this,
                &ADivineBeastsCinematicCameraReviewActor::SetupLocalInput, 0.2f, false);
        }
        return;
    }
    BoundController = Controller;
    EnableInput(Controller);
    if (!InputComponent)
    {
        if (++InitAttempts < 25)
        {
            World->GetTimerManager().SetTimer(InitTimer, this,
                &ADivineBeastsCinematicCameraReviewActor::SetupLocalInput, 0.2f, false);
        }
        return;
    }

    // 纯开发地图本地键盘：数字1/2/3/4切换焦距，Z/X开合光圈，C/V调焦，R恢复。
    InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::Select24);
    InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::Select35);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::Select50);
    InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::Select85);
    InputComponent->BindKey(EKeys::Z, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::OpenAperture);
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::CloseAperture);
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::FocusNear);
    InputComponent->BindKey(EKeys::V, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::FocusFar);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ADivineBeastsCinematicCameraReviewActor::ResetLens);
    bInputBound = true;

    if (!SelectLens(1))
    {
        UE_LOG(LogSSLM_CinematicReview, Warning, TEXT("SSLM_CINE_DEMO_SETUP_REJECTED: 默认镜头/数据资产未就绪"));
    }
    else
    {
        PrintCameraStatus(TEXT("1/2/3/4切换镜头  Z/X调整光圈  C/V调整焦点  R重置"));
    }

    // 开发自动回归专用：显式命令行才以定时器循环切换，不污染正式游戏。
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(), TEXT("SSLMReviewCameraAutoCycle")))
    {
        AutoCycleSteps = 0;
        World->GetTimerManager().SetTimer(AutoCycleTimer, this,
            &ADivineBeastsCinematicCameraReviewActor::RunAutoCycleStep, 0.7f, true);
    }
#endif
}

bool ADivineBeastsCinematicCameraReviewActor::SelectLens(int32 Index)
{
    APlayerController* Controller = BoundController.Get();
    ACineCameraActor* Camera = ResolveCamera(Index);
    UGamePlatformCinematicCameraProfile* Profile = ResolveProfile(Index);
    UGamePlatformCinematicCameraSubsystem* Service = ResolveService();
    if (!IsInGameThread() || !IsValid(Controller) || !IsValid(Camera) ||
        !IsValid(Profile) || !Profile->IsValidProfile() || !Service ||
        Camera->GetWorld() != GetWorld())
    {
        UE_LOG(LogSSLM_CinematicReview, Warning, TEXT("SSLM_CINE_DEMO_SELECT_REJECTED index=%d"), Index);
        return false;
    }

    const bool bSelected = Service->BeginCinematicView(Camera, Profile);
    if (!bSelected)
    {
        UE_LOG(LogSSLM_CinematicReview, Warning, TEXT("SSLM_CINE_DEMO_VIEWTARGET_REJECTED index=%d"), Index);
        return false;
    }
    ActiveIndex = Index;
    PrintCameraStatus(FString::Printf(TEXT("镜头 %dmm  光圈 f/%.1f  按R恢复"),
        FMath::RoundToInt(Profile->FocalLengthMm), Profile->Aperture));
    UE_LOG(LogSSLM_CinematicReview, Display,
        TEXT("SSLM_CINE_DEMO_SELECTED index=%d focal=%.1f aperture=%.1f"),
        Index, Profile->FocalLengthMm, Profile->Aperture);
    return true;
}

void ADivineBeastsCinematicCameraReviewActor::AdjustAperture(bool bOpen)
{
    ACineCameraActor* Camera = ResolveCamera(ActiveIndex);
    UCineCameraComponent* Cine = Camera ? Camera->GetCineCameraComponent() : nullptr;
    if (!Cine)
    {
        return;
    }
    const float NewValue = FMath::Clamp(Cine->CurrentAperture +
        (bOpen ? -0.4f : 0.4f), 1.4f, 16.f);
    Cine->SetCurrentAperture(NewValue);
    PrintCameraStatus(FString::Printf(TEXT("当前光圈 f/%.1f"), NewValue));
    UE_LOG(LogSSLM_CinematicReview, Display, TEXT("SSLM_CINE_DEMO_APERTURE f=%.2f"), NewValue);
}

void ADivineBeastsCinematicCameraReviewActor::AdjustFocus(bool bFarther)
{
    ACineCameraActor* Camera = ResolveCamera(ActiveIndex);
    UCineCameraComponent* Cine = Camera ? Camera->GetCineCameraComponent() : nullptr;
    if (!Cine)
    {
        return;
    }
    const float NewDistance = FMath::Clamp(
        Cine->FocusSettings.ManualFocusDistance + (bFarther ? 50.f : -50.f),
        10.f, 100000.f);
    Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
    Cine->FocusSettings.ManualFocusDistance = NewDistance;
    PrintCameraStatus(FString::Printf(TEXT("焦点距离 %.0f 厘米"), NewDistance));
    UE_LOG(LogSSLM_CinematicReview, Display, TEXT("SSLM_CINE_DEMO_FOCUS cm=%.1f"), NewDistance);
}

void ADivineBeastsCinematicCameraReviewActor::ResetLens()
{
    ACineCameraActor* Camera = ResolveCamera(ActiveIndex);
    UGamePlatformCinematicCameraProfile* Profile = ResolveProfile(ActiveIndex);
    UCineCameraComponent* Cine = Camera ? Camera->GetCineCameraComponent() : nullptr;
    if (!Cine || !IsValid(Profile) || !Profile->IsValidProfile())
    {
        return;
    }
    Cine->Filmback.SensorWidth = Profile->SensorWidthMm;
    Cine->SetCurrentFocalLength(Profile->FocalLengthMm);
    Cine->SetCurrentAperture(Profile->Aperture);
    Cine->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
    Cine->FocusSettings.ManualFocusDistance = Profile->FocusDistanceCm;
    PrintCameraStatus(TEXT("已从镜头配置资产恢复参数"));
    UE_LOG(LogSSLM_CinematicReview, Display, TEXT("SSLM_CINE_DEMO_RESET index=%d"), ActiveIndex);
}

void ADivineBeastsCinematicCameraReviewActor::RunAutoCycleStep()
{
    const int32 Index = AutoCycleSteps++;
    if (Index < 4)
    {
        SelectLens(Index);
        if (Index == 3)
        {
            // 自动回归同时验证可操作光圈、焦点及恢复；正式玩家模式不会自动调节。
            AdjustAperture(true);
            AdjustFocus(false);
            ResetLens();
        }
    }
    if (AutoCycleSteps >= 4 && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoCycleTimer);
        UE_LOG(LogSSLM_CinematicReview, Display, TEXT("SSLM_CINE_DEMO_AUTOCYCLE_COMPLETED steps=4"));
    }
}

void ADivineBeastsCinematicCameraReviewActor::PrintCameraStatus(const FString& Message) const
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan,
            FString::Printf(TEXT("SSLM 电影镜头演示: %s"), *Message));
    }
}

void ADivineBeastsCinematicCameraReviewActor::Select24() { SelectLens(0); }
void ADivineBeastsCinematicCameraReviewActor::Select35() { SelectLens(1); }
void ADivineBeastsCinematicCameraReviewActor::Select50() { SelectLens(2); }
void ADivineBeastsCinematicCameraReviewActor::Select85() { SelectLens(3); }
void ADivineBeastsCinematicCameraReviewActor::OpenAperture() { AdjustAperture(true); }
void ADivineBeastsCinematicCameraReviewActor::CloseAperture() { AdjustAperture(false); }
void ADivineBeastsCinematicCameraReviewActor::FocusNear() { AdjustFocus(false); }
void ADivineBeastsCinematicCameraReviewActor::FocusFar() { AdjustFocus(true); }

void ADivineBeastsCinematicCameraReviewActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(InitTimer);
        World->GetTimerManager().ClearTimer(AutoCycleTimer);
        if (!World->bIsTearingDown)
        {
            if (UGamePlatformCinematicCameraSubsystem* Service = ResolveService())
            {
                if (Service->IsCinematicViewActive())
                {
                    Service->EndCinematicView();
                }
            }
        }
    }
    if (APlayerController* Controller = BoundController.Get())
    {
        DisableInput(Controller);
    }
    BoundController.Reset();
    bInputBound = false;
    Super::EndPlay(EndPlayReason);
}