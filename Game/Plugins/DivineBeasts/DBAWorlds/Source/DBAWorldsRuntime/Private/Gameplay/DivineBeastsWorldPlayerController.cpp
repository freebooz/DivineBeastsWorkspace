// 第三层双端世界控制器：桥接平台准备事实；本地PC输入策略拥有当前Pawn的鼠标拖动/观察镜头。
// 不依赖客户端UI，菜单由中立委托交给项目客户端；不提交坐标RPC或放宽服务器资格。
#include "Gameplay/DivineBeastsWorldPlayerController.h"
#include "Framework/GamePlatformGameStateBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Gameplay/DivineBeastsWorldControlState.h"
#include "Components/InputComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#if !UE_SERVER
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#endif

DECLARE_DELEGATE_OneParam(FDivineBeastsJournalInputDelegate, FName);

ADivineBeastsWorldPlayerController::ADivineBeastsWorldPlayerController()
{
    auto* Interactor = CreateDefaultSubobject<UGamePlatformInteractorComponent>(TEXT("WorldInteractor"));
    Interactor->SetLocalFocusPolicy(false, true);
}

void ADivineBeastsWorldPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    // Controller先于Pawn消费同名原生轴，避免旧Pawn镜头/移动同时执行。UI已消费的按键不会进入GameAndUI后备。
    // 这些轴也被通用Pawn注册，显式消费可保证世界控制器拥有当前语义，同时保留竞技控制器的Pawn默认输入。
    InputComponent->BindAxis(TEXT("DBA.MoveForward"), this, &ThisClass::SetForwardAxis).bConsumeInput = true;
    InputComponent->BindAxis(TEXT("DBA.MoveRight"), this, &ThisClass::SetStrafeAxis).bConsumeInput = true;
    InputComponent->BindAxis(TEXT("DBA.KeyboardTurn"), this, &ThisClass::SetKeyboardTurnAxis).bConsumeInput = true;
    InputComponent->BindAxis(TEXT("DBA.Turn"), this, &ThisClass::TurnObservationCamera).bConsumeInput = true;
    InputComponent->BindAxis(TEXT("DBA.Look"), this, &ThisClass::LookObservationCamera).bConsumeInput = true;
    InputComponent->BindAxis(TEXT("DBA.CameraZoom"), this, &ThisClass::ZoomObservationCamera);
    InputComponent->BindAction(TEXT("DBA.CameraObserve"), IE_Pressed, this, &ThisClass::BeginLeftCameraDrag);
    InputComponent->BindAction(TEXT("DBA.CameraObserve"), IE_Released, this, &ThisClass::EndLeftCameraDrag);
    InputComponent->BindAction(TEXT("DBA.CameraSteer"), IE_Pressed, this, &ThisClass::BeginRightCameraDrag);
    InputComponent->BindAction(TEXT("DBA.CameraSteer"), IE_Released, this, &ThisClass::EndRightCameraDrag);
    InputComponent->BindAction(TEXT("DBA.Jump"), IE_Pressed, this, &ThisClass::BeginWorldJump);
    InputComponent->BindAction(TEXT("DBA.Jump"), IE_Released, this, &ThisClass::EndWorldJump);
    InputComponent->BindAction(TEXT("DBA.SystemMenu"), IE_Pressed, this, &ThisClass::RequestWorldMenu);
    // 原生I键只作为世界UI意图；不在双端World模块实例化或操纵任何Widget。
    InputComponent->BindAction(TEXT("DBA.Inventory"), IE_Pressed, this, &ThisClass::RequestWorldInventory);
    InputComponent->BindAction(TEXT("DBA.Interact"), IE_Pressed, this, &ThisClass::BeginWorldInteraction);
    InputComponent->BindAction(TEXT("DBA.Interact"), IE_Released, this, &ThisClass::EndWorldInteraction);
    InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.CharacterInfo"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("Character")));
    InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.MapQuest"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("MapQuest")));
    InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.Spellbook"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("Spellbook")));
    InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.Party"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("Party")));
}

bool ADivineBeastsWorldPlayerController::CanProcessWorldInput() const
{
    return IsLocalController() && bWorldInputActive && !bWorldGameplayInputSuspended && !IsMoveInputIgnored();
}

bool ADivineBeastsWorldPlayerController::CanProcessWorldLookInput() const
{
    return CanProcessWorldInput() && !IsLookInputIgnored();
}

bool ADivineBeastsWorldPlayerController::IsPointerOverWorldViewport() const
{
#if !UE_SERVER
    auto* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!ViewportClient || !FSlateApplication::IsInitialized()) return false;
    const TSharedPtr<SViewport> GameViewport = ViewportClient->GetGameViewportWidget();
    if (!GameViewport.IsValid()) return false;
    auto& Slate = FSlateApplication::Get();
    const auto* LocalPlayer = GetLocalPlayer();
    const int32 UserIndex = LocalPlayer ? Slate.GetUserIndexForPlatformUser(LocalPlayer->GetPlatformUserId()).Get(0) : 0;
    // IgnoreEnabledStatus=true是必要的：禁用按钮仍是UI区域，不能把它当作穿透到世界的空白。
    const FWidgetPath Path = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows(), true, UserIndex);
    return Path.IsValid() && FDivineBeastsWorldControlState::CanBeginViewportDrag(
        Path.ContainsWidget(GameViewport.Get()), &Path.GetLastWidget().Get() == GameViewport.Get());
#else
    return false; // 无桌面视口的服务器没有本地镜头捕获资格。
#endif
}

void ADivineBeastsWorldPlayerController::UpdateWorldInputLocks(bool bNeedsLock)
{
    const int MoveDelta = FDivineBeastsWorldControlState::UpdateOwnedInputLock(bNeedsLock, bOwnsWorldMoveInputLock);
    const int LookDelta = FDivineBeastsWorldControlState::UpdateOwnedInputLock(bNeedsLock, bOwnsWorldLookInputLock);
    if (MoveDelta != 0) SetIgnoreMoveInput(MoveDelta > 0);
    if (LookDelta != 0) SetIgnoreLookInput(LookDelta > 0);
}

void ADivineBeastsWorldPlayerController::ApplyVisibleCursorMode()
{
    if (!IsLocalController()) return;
    bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    // 菜单暂停时UI客户端拥有焦点和UIOnly模式；失焦清理不能抢回焦点或覆盖该模式。
    if (!bWorldGameplayInputSuspended) SetInputMode(Mode);
    if (auto* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
    {
        ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
        ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
    }
    if (bHasSavedCursorPosition)
    {
        SetMouseLocation(SavedCursorPosition.X, SavedCursorPosition.Y);
        bHasSavedCursorPosition = false;
    }
}

void ADivineBeastsWorldPlayerController::ResetWorldInput()
{
    if (bResettingWorldInput) return;
    TGuardValue<bool> Guard(bResettingWorldInput, true);
    MouseButtonMask = 0;
    bPointerClickPending = false;
    PointerPressedTarget.Reset();
    PointerTravelPixels = 0.f;
    CurrentMouseCursor = EMouseCursor::Default;
    ForwardAxis = StrafeAxis = KeyboardTurnAxis = 0.f;
    RotationInput = FRotator::ZeroRotator;
    if (APawn* ControlledPawn = InputPawn.Get())
    {
        ControlledPawn->ConsumeMovementInputVector();
        if (auto* ControlledCharacter = Cast<ACharacter>(ControlledPawn)) ControlledCharacter->StopJumping();
        if (auto* ASC = ControlledPawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>()) ASC->ClearAbilityInput();
    }
    EndWorldInteraction();
    // 调基类避免Flush覆盖递归；释放也必须清空技能，丢失keyup不能留下WhileHeld请求。
    Super::FlushPressedKeys();
    ApplyVisibleCursorMode();
}

void ADivineBeastsWorldPlayerController::FlushPressedKeys()
{
    Super::FlushPressedKeys();
    if (!bResettingWorldInput) ResetWorldInput();
}

void ADivineBeastsWorldPlayerController::SetWorldGameplayInputSuspended(bool bSuspended)
{
    if (!IsLocalController() || bWorldGameplayInputSuspended == bSuspended) return;
    bWorldGameplayInputSuspended = bSuspended;
    ResetWorldInput();
    RefreshWorldInputPolicy();
}

void ADivineBeastsWorldPlayerController::RefreshWorldInputPolicy()
{
    const bool bActive = IsWorldGameplayActive() && !bWorldGameplayInputSuspended && !bWorldTravelPending;
    const bool bPawnChanged = InputPawn.Get() != GetPawn();
    if (bPawnChanged || bWorldInputActive != bActive)
    {
        if (bPawnChanged || !bActive) SetSelectedWorldTarget(nullptr);
        ResetWorldInput();
        InputPawn = GetPawn();
        bWorldInputActive = bActive;
        if (bPawnChanged && GetPawn())
        {
            ObservationRotation = GetControlRotation();
            ObservationRotation.Pitch = -15.f;
            if (auto* Boom = GetPawn()->FindComponentByClass<USpringArmComponent>())
            {
                // 仅本地观察镜头脱离ControlRotation；后者继续通过原生运动复制携带角色朝向。
                Boom->bUsePawnControlRotation = false;
                Boom->bDoCollisionTest = true;
                Boom->SetWorldRotation(ObservationRotation);
            }
        }
    }
    // 引擎Ignore输入由多个系统共同计数；本世界只增减自身一次锁，不能清零别人的禁用请求。
    UpdateWorldInputLocks(!bActive);
    if (auto* Interactor = FindComponentByClass<UGamePlatformInteractorComponent>())
        Interactor->SetLocalFocusPolicy(bActive, true);
}

void ADivineBeastsWorldPlayerController::SetCameraMouseButton(bool bRight, bool bPressed)
{
    // GameAndUI的未处理事件也可能来自按钮RMB或禁用按钮LMB。仅首次捕获查真实路径，
    // 已从世界开始的高精度拖动可接收第二个鼠标键；释放永远处理，不受命中/锁定变化阻挡。
    if (bPressed && (!CanProcessWorldLookInput() || (MouseButtonMask == 0 && !IsPointerOverWorldViewport()))) return;
    if (bPressed && !bRight && MouseButtonMask == 0)
    {
        PointerPressedTarget = FindPointerTarget();
        PointerPressedAtSeconds = FPlatformTime::Seconds();
        PointerTravelPixels = 0.f;
        bPointerClickPending = true; // 空白短按可清除目标；不是强制找到Actor才允许取消选择。
    }
    if (bRight) bPointerClickPending = false;
    const bool bSelectOnRelease = !bPressed && !bRight && CanProcessWorldInput() &&
        FDivineBeastsWorldControlState::IsSelectionClick(bPointerClickPending,
            FPlatformTime::Seconds() - PointerPressedAtSeconds, PointerTravelPixels);
    FDivineBeastsWorldControlState State(MouseButtonMask);
    const bool bWasDragging = State.IsDragging();
    State.SetMouseButton(bRight, bPressed);
    MouseButtonMask = State.MouseButtons;
    if (!bWasDragging && State.IsDragging())
    {
        float X = 0.f, Y = 0.f;
        bHasSavedCursorPosition = GetMousePosition(X, Y);
        SavedCursorPosition = FIntPoint(FMath::RoundToInt(X), FMath::RoundToInt(Y));
        bShowMouseCursor = false;
        SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false));
    }
    if (bRight && bPressed)
    {
        FRotator Facing = GetControlRotation();
        Facing.Yaw = ObservationRotation.Yaw;
        SetControlRotation(Facing);
    }
    if (bWasDragging && !State.IsDragging()) ApplyVisibleCursorMode();
    if (!bPressed && !bRight)
    {
        bPointerClickPending = false;
        // 恢复光标后再次验证UI遮挡和同一命中，避免按住期间目标移开/菜单出现导致误选。
        if (bSelectOnRelease && IsPointerOverWorldViewport() && FindPointerTarget() == PointerPressedTarget.Get())
            SetSelectedWorldTarget(PointerPressedTarget.Get());
        PointerPressedTarget.Reset();
    }
}
void ADivineBeastsWorldPlayerController::BeginLeftCameraDrag() { SetCameraMouseButton(false, true); }
void ADivineBeastsWorldPlayerController::EndLeftCameraDrag() { SetCameraMouseButton(false, false); }
void ADivineBeastsWorldPlayerController::BeginRightCameraDrag() { SetCameraMouseButton(true, true); }
void ADivineBeastsWorldPlayerController::EndRightCameraDrag() { SetCameraMouseButton(true, false); }
void ADivineBeastsWorldPlayerController::SetForwardAxis(float Value) { ForwardAxis = CanProcessWorldInput() ? Value : 0.f; }
void ADivineBeastsWorldPlayerController::SetStrafeAxis(float Value) { StrafeAxis = CanProcessWorldInput() ? Value : 0.f; }
void ADivineBeastsWorldPlayerController::SetKeyboardTurnAxis(float Value) { KeyboardTurnAxis = CanProcessWorldInput() ? Value : 0.f; }

void ADivineBeastsWorldPlayerController::TurnObservationCamera(float Value)
{
    if (!CanProcessWorldLookInput() || MouseButtonMask == 0) return;
    const FDivineBeastsWorldMouseLookDelta Look =
        FDivineBeastsWorldControlState(MouseButtonMask).ResolveMouseLook(Value, 0.f);
    ObservationRotation.Yaw = FRotator::NormalizeAxis(ObservationRotation.Yaw + Look.CameraYawInput);
    if (!FMath::IsNearlyZero(Look.CharacterYawInput))
    {
        FRotator Facing = GetControlRotation(); Facing.Yaw = ObservationRotation.Yaw; SetControlRotation(Facing);
    }
}
void ADivineBeastsWorldPlayerController::LookObservationCamera(float Value)
{
    if (!CanProcessWorldLookInput() || MouseButtonMask == 0) return;
    const FDivineBeastsWorldMouseLookDelta Look =
        FDivineBeastsWorldControlState(MouseButtonMask).ResolveMouseLook(0.f, Value);
    ObservationRotation.Pitch = FMath::Clamp(ObservationRotation.Pitch + Look.CameraPitchInput, -80.f, 60.f);
}
void ADivineBeastsWorldPlayerController::ZoomObservationCamera(float Value)
{
    if (!CanProcessWorldLookInput() || !GetPawn() || FMath::IsNearlyZero(Value) ||
        (MouseButtonMask == 0 && !IsPointerOverWorldViewport())) return;
    if (auto* Boom = GetPawn()->FindComponentByClass<USpringArmComponent>())
        Boom->TargetArmLength = FMath::Clamp(Boom->TargetArmLength - Value * ZoomStepCentimeters,
            MinimumCameraDistanceCentimeters, MaximumCameraDistanceCentimeters);
}
void ADivineBeastsWorldPlayerController::BeginWorldJump()
{
    if (CanProcessWorldInput()) if (auto* ControlledCharacter = Cast<ACharacter>(GetPawn())) ControlledCharacter->Jump();
}
void ADivineBeastsWorldPlayerController::EndWorldJump()
{
    if (auto* ControlledCharacter = Cast<ACharacter>(GetPawn())) ControlledCharacter->StopJumping();
}
void ADivineBeastsWorldPlayerController::RequestWorldMenu()
{
    if (IsLocalController() && IsWorldGameplayActive()) OnWorldEscapeRequested.Broadcast();
}

void ADivineBeastsWorldPlayerController::RequestWorldInventory()
{
    // 不能在登录、角色入口、服务器角色或世界准备未完成时打开背包。
    if (IsLocalController() && IsWorldGameplayActive())
    {
        OnWorldInventoryRequested.Broadcast();
    }
}

void ADivineBeastsWorldPlayerController::RequestWorldJournal(FName Section)
{
    if (IsLocalController() && IsWorldGameplayActive()) OnWorldJournalRequested.Broadcast(Section);
}

void ADivineBeastsWorldPlayerController::PostProcessInput(float DeltaTime, bool bGamePaused)
{
    Super::PostProcessInput(DeltaTime, bGamePaused);
    RefreshPointerFeedback();
    if (!CanProcessWorldInput() || bGamePaused || !GetPawn()) return;
    const FDivineBeastsWorldControlState State(MouseButtonMask);
    const float TurnDegrees = IsLookInputIgnored() ? 0.f : State.ResolveTurn(KeyboardTurnAxis) * KeyboardTurnDegreesPerSecond * DeltaTime;
    FRotator Facing = GetControlRotation(); Facing.Yaw += TurnDegrees; SetControlRotation(Facing);
    // 左键观察时键盘仍转角色，但观察方位保持独立；松开时也不会骤然改变角色朝向。
    if (!State.IsDragging()) ObservationRotation.Yaw += TurnDegrees;
    const FRotationMatrix Heading(FRotator(0.f, Facing.Yaw, 0.f));
    GetPawn()->AddMovementInput(Heading.GetUnitAxis(EAxis::X), State.ResolveForward(ForwardAxis));
    GetPawn()->AddMovementInput(Heading.GetUnitAxis(EAxis::Y), State.ResolveStrafe(StrafeAxis, KeyboardTurnAxis));
}

void ADivineBeastsWorldPlayerController::UpdateRotation(float DeltaTime)
{
    Super::UpdateRotation(DeltaTime);
    if (IsLocalController() && InputPawn.IsValid())
        if (auto* Boom = InputPawn->FindComponentByClass<USpringArmComponent>()) Boom->SetWorldRotation(ObservationRotation);
}
void ADivineBeastsWorldPlayerController::BeginPlay()
{
    Super::BeginPlay();
    OnPreparationChanged.AddUObject(this,&ThisClass::RefreshLocalWorldFacts);
    GetWorld()->GameStateSetEvent.AddUObject(this,&ThisClass::BindGameState);
    BindGameState(GetWorld()->GetGameState()); RefreshLocalWorldFacts();
}
void ADivineBeastsWorldPlayerController::BindGameState(AGameStateBase* State)
{
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.RemoveAll(this);
    auto* Shared=Cast<AGamePlatformGameStateBase>(State);
    ObservedExperience=Shared?Shared->GetExperienceComponent():nullptr;
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.AddUObject(this,&ThisClass::RefreshLocalWorldFacts);
    RefreshLocalWorldFacts();
}
bool ADivineBeastsWorldPlayerController::AreLocalResourcesPrepared() const
{
    return ObservedExperience.IsValid() && ObservedExperience->GetClientExperienceSnapshot().Stage==EGamePlatformClientExperienceStage::Prepared;
}
bool ADivineBeastsWorldPlayerController::IsLocalPawnBound() const
{
    return ObservedPlayer.IsValid() && GetPawn() && GetPawn()->GetController()==this
        && ObservedPlayer->GetLifecycleSnapshot().ControlledPawn==GetPawn() && GetPawn()->InputComponent;
}
bool ADivineBeastsWorldPlayerController::IsWorldGameplayActive() const
{
    return ObservedPlayer.IsValid() && ObservedPlayer->GetLifecycleSnapshot().IsServerActive() && IsLocalPawnBound()
        && ObservedCharacter.IsValid() && ObservedCharacter->IsCharacterReady();
}
void ADivineBeastsWorldPlayerController::RefreshLocalWorldFacts()
{
    if(!IsLocalController() || bRefreshingFacts) return;
    TGuardValue<bool> Guard(bRefreshingFacts,true);
    auto* State=GetPlayerState<AGamePlatformPlayerStateBase>();
    if(ObservedPlayer.Get()!=State)
    {
        if(ObservedPlayer.IsValid()) ObservedPlayer->OnLifecycleChanged.RemoveAll(this);
        ObservedPlayer=State;
        if(State) State->OnLifecycleChanged.AddUObject(this,&ThisClass::RefreshLocalWorldFacts);
    }
    auto* CurrentCharacterIdentity = GetPawn() ? GetPawn()->FindComponentByClass<UDivineBeastsCharacterComponent>() : nullptr;
    if (ObservedCharacter.Get() != CurrentCharacterIdentity)
    {
        if (ObservedCharacter.IsValid()) ObservedCharacter->OnReadinessChanged().RemoveAll(this);
        ObservedCharacter = CurrentCharacterIdentity;
        if (CurrentCharacterIdentity) CurrentCharacterIdentity->OnReadinessChanged().AddWeakLambda(this, [this](bool) { RefreshLocalWorldFacts(); });
    }
    // Prepare令牌只提交一次，必须等项目必要定义Ready后再报告，避免被服务器拒绝后没有新事件重试。
    if(AreLocalResourcesPrepared() && IsLocalPawnBound() && CurrentCharacterIdentity && CurrentCharacterIdentity->IsCharacterReady())
    {
        FGamePlatformLocalPreparationFacts Facts;
        Facts.bClientExperiencePrepared=AreLocalResourcesPrepared(); Facts.bClientPawnBound=IsLocalPawnBound();
        Facts.bInputProfilePrepared=GetPawn()->InputComponent!=nullptr; Facts.bBindingsReady=Facts.bInputProfilePrepared;
        ReportLocalPreparation(Facts);
    }
    RefreshWorldInputPolicy();
    OnLocalWorldFactsChanged.Broadcast();
}
void ADivineBeastsWorldPlayerController::OnRep_PlayerState(){Super::OnRep_PlayerState();RefreshLocalWorldFacts();}
void ADivineBeastsWorldPlayerController::OnRep_Pawn(){Super::OnRep_Pawn();RefreshLocalWorldFacts();}
void ADivineBeastsWorldPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
    Super::ClientRestart_Implementation(NewPawn);
    // 只有引擎接收服务器重新拥有当前Pawn的回执才撤销旅行门闩；旧鼠标状态不跨世界保留。
    bWorldTravelPending = false;
    RefreshLocalWorldFacts();
}
void ADivineBeastsWorldPlayerController::PreClientTravel(const FString& PendingURL, ETravelType TravelType, bool bIsSeamlessTravel)
{
    bWorldTravelPending = true;
    ResetWorldInput();
    RefreshWorldInputPolicy();
    Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}
void ADivineBeastsWorldPlayerController::OnPossess(APawn* NewPawn)
{
    Super::OnPossess(NewPawn);
    RefreshLocalWorldFacts();
}
void ADivineBeastsWorldPlayerController::OnUnPossess()
{
    ResetWorldInput();
    Super::OnUnPossess();
    RefreshLocalWorldFacts();
}
void ADivineBeastsWorldPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
    SetSelectedWorldTarget(nullptr);
    OnWorldTargetChanged.Clear();
    ResetWorldInput(); InputPawn.Reset(); bWorldInputActive = false;
    OnWorldEscapeRequested.Clear();
    OnWorldInventoryRequested.Clear();
    OnWorldJournalRequested.Clear();
    UpdateWorldInputLocks(false);
    OnPreparationChanged.RemoveAll(this); GetWorld()->GameStateSetEvent.RemoveAll(this);
    if(ObservedPlayer.IsValid()) ObservedPlayer->OnLifecycleChanged.RemoveAll(this);
    if(ObservedExperience.IsValid()) ObservedExperience->OnLocalResourcesPrepared.RemoveAll(this);
    if(ObservedCharacter.IsValid()) ObservedCharacter->OnReadinessChanged().RemoveAll(this);
    OnLocalWorldFactsChanged.Clear(); Super::EndPlay(Reason);
}

AActor* ADivineBeastsWorldPlayerController::FindPointerTarget() const
{
    if (!CanProcessWorldInput() || !GetWorld()) return nullptr;
    FVector Origin, Direction;
    if (!DeprojectMousePositionToWorld(Origin, Direction)) return nullptr;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DivineBeastsPointerTarget), false, GetPawn());
    FHitResult Hit;
    // 最大显示选择距离为100米；交互仍由平台按实际交互距离二次校验，不能使用该距离执行动作。
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * 10000.f, ECC_Visibility, Params)) return nullptr;
    AActor* Actor = Hit.GetActor();
    return IsValid(Actor) && (Cast<APawn>(Actor) || Actor->FindComponentByClass<UGamePlatformInteractableComponent>()) ? Actor : nullptr;
}

void ADivineBeastsWorldPlayerController::SetSelectedWorldTarget(AActor* Target)
{
    if (Target && (!IsValid(Target) || Target->GetWorld() != GetWorld())) Target = nullptr;
    if (SelectedWorldTarget.Get() == Target) return;
    if (AActor* Previous = SelectedWorldTarget.Get()) Previous->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleSelectedTargetEndPlay);
    SelectedWorldTarget = Target;
    if (Target) Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleSelectedTargetEndPlay);
    OnWorldTargetChanged.Broadcast();
}

void ADivineBeastsWorldPlayerController::HandleSelectedTargetEndPlay(AActor* Actor, EEndPlayReason::Type /*Reason*/)
{
    if (SelectedWorldTarget.Get() == Actor) SetSelectedWorldTarget(nullptr);
}

void ADivineBeastsWorldPlayerController::RefreshPointerFeedback()
{
    if (!IsLocalController()) return;
    if (MouseButtonMask != 0)
    {
        float X = 0.f, Y = 0.f;
        GetInputMouseDelta(X, Y);
        PointerTravelPixels += FMath::Abs(X) + FMath::Abs(Y);
        if (!FMath::IsFinite(PointerTravelPixels) || PointerTravelPixels > 6.f) bPointerClickPending = false;
        return;
    }
    CurrentMouseCursor = EMouseCursor::Default;
    if (!CanProcessWorldInput() || !bShowMouseCursor || !IsPointerOverWorldViewport()) return;
    AActor* Target = FindPointerTarget();
    if (!Target) return;
    if (const auto* Interactable = Target->FindComponentByClass<UGamePlatformInteractableComponent>())
        CurrentMouseCursor = Interactable->IsInteractionEnabled() ? EMouseCursor::Hand : EMouseCursor::SlashedCircle;
    else CurrentMouseCursor = EMouseCursor::Crosshairs; // 只代表可选目标，不推断敌我或授权攻击。
}

void ADivineBeastsWorldPlayerController::BeginWorldInteraction()
{
    if (!CanProcessWorldInput() || MouseButtonMask != 0 || !IsPointerOverWorldViewport()) return;
    if (auto* Interactor = FindComponentByClass<UGamePlatformInteractorComponent>()) Interactor->BeginFocusedInteraction();
}

void ADivineBeastsWorldPlayerController::EndWorldInteraction()
{
    // 释放/暂停即使世界不再Active也应取消已受理的长按；组件负责节流、所有权及服务器终态校验。
    if (auto* Interactor = FindComponentByClass<UGamePlatformInteractorComponent>()) Interactor->CancelCurrentInteraction();
}
