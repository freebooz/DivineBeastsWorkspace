// 第三层双端世界控制器：桥接平台准备事实；本地PC输入策略拥有当前Pawn的鼠标拖动/观察镜头。
// 不依赖客户端UI，菜单由中立委托交给项目客户端；不提交坐标RPC或放宽服务器资格。
#include "Gameplay/DivineBeastsPlayerControlsComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "EngineUtils.h"
#include "Gameplay/DivineBeastsArenaEntrance.h"
#include "Framework/GamePlatformGameStateBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Components/GamePlatformExperienceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Gameplay/DivineBeastsWorldControlState.h"
#include "Components/InputComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Components/GamePlatformInteractorComponent.h"
#include "HAL/PlatformTime.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/Character.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/SpringArmComponent.h"
#if !UE_SERVER
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#endif

DECLARE_DELEGATE_OneParam(FDivineBeastsJournalInputDelegate, FName);

void UDivineBeastsPlayerControlsComponent::NotifyFinalCameraView(float DeltaSeconds)
{
    if (!Controller()->IsLocalController() || !Controller()->PlayerCameraManager || Controller()->GetNetMode() == NM_DedicatedServer) return;
    const FMinimalViewInfo& View = Controller()->PlayerCameraManager->GetCameraCacheView();
    if (!View.Location.Equals(LastNotifiedCameraLocation, 0.01) ||
        !View.Rotation.Equals(LastNotifiedCameraRotation, 0.001) ||
        !FMath::IsNearlyEqual(View.FOV, LastNotifiedCameraFOV, 0.001f))
    {
        // 先记缓存再广播，允许同步旅行/换视图，不在回调后操作旧世界状态。
        LastNotifiedCameraLocation = View.Location; LastNotifiedCameraRotation = View.Rotation; LastNotifiedCameraFOV = View.FOV;
        OnFinalCameraViewChanged.Broadcast();
    }
}

void UDivineBeastsPlayerControlsComponent::RequestWorldHealthBarsToggle()
{
    if (Controller()->IsLocalController() && bGameplayActive && !bWorldGameplayInputSuspended)
        OnWorldHealthBarsToggleRequested.Broadcast();
}

void UDivineBeastsPlayerControlsComponent::BindInput()
{
    if (!Controller() || !Controller()->InputComponent || BoundInputComponent == Controller()->InputComponent) return;
    BoundInputComponent = Controller()->InputComponent;
    Controller()->InputComponent->BindAction(TEXT("DBA.ToggleWorldHealthBars"), IE_Pressed, this, &ThisClass::RequestWorldHealthBarsToggle).bConsumeInput = true;
    Controller()->InputComponent->BindAction(TEXT("DBA.CycleWorldTarget"), IE_Pressed, this, &ThisClass::RequestNextWorldTarget).bConsumeInput = true;
    // Controller先于Pawn消费同名原生轴，避免旧Pawn镜头/移动同时执行。UI已消费的按键不会进入GameAndUI后备。
    // 同一组件消费原生轴；通用Pawn默认输入不得再重复执行。
    Controller()->InputComponent->BindAxis(TEXT("DBA.MoveForward"), this, &ThisClass::SetForwardAxis).bConsumeInput = true;
    Controller()->InputComponent->BindAxis(TEXT("DBA.MoveRight"), this, &ThisClass::SetStrafeAxis).bConsumeInput = true;
    Controller()->InputComponent->BindAxis(TEXT("DBA.KeyboardTurn"), this, &ThisClass::SetKeyboardTurnAxis).bConsumeInput = true;
    Controller()->InputComponent->BindAxis(TEXT("DBA.Turn"), this, &ThisClass::TurnObservationCamera).bConsumeInput = true;
    Controller()->InputComponent->BindAxis(TEXT("DBA.Look"), this, &ThisClass::LookObservationCamera).bConsumeInput = true;
    Controller()->InputComponent->BindAxis(TEXT("DBA.CameraZoom"), this, &ThisClass::ZoomObservationCamera);
    Controller()->InputComponent->BindAction(TEXT("DBA.CameraObserve"), IE_Pressed, this, &ThisClass::BeginLeftCameraDrag);
    Controller()->InputComponent->BindAction(TEXT("DBA.CameraObserve"), IE_Released, this, &ThisClass::EndLeftCameraDrag);
    Controller()->InputComponent->BindAction(TEXT("DBA.CameraSteer"), IE_Pressed, this, &ThisClass::BeginRightCameraDrag);
    Controller()->InputComponent->BindAction(TEXT("DBA.CameraSteer"), IE_Released, this, &ThisClass::EndRightCameraDrag);
    Controller()->InputComponent->BindAction(TEXT("DBA.Jump"), IE_Pressed, this, &ThisClass::BeginWorldJump);
    Controller()->InputComponent->BindAction(TEXT("DBA.Jump"), IE_Released, this, &ThisClass::EndWorldJump);
    Controller()->InputComponent->BindAction(TEXT("DBA.SystemMenu"), IE_Pressed, this, &ThisClass::RequestWorldMenu);
    // 原生I键只作为世界UI意图；不在双端World模块实例化或操纵任何Widget。
    Controller()->InputComponent->BindAction(TEXT("DBA.Inventory"), IE_Pressed, this, &ThisClass::RequestWorldInventory);
    Controller()->InputComponent->BindAction(TEXT("DBA.Interact"), IE_Pressed, this, &ThisClass::BeginWorldInteraction);
    Controller()->InputComponent->BindAction(TEXT("DBA.Interact"), IE_Released, this, &ThisClass::EndWorldInteraction);
    Controller()->InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.CharacterInfo"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("Character")));
    Controller()->InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.MapQuest"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("MapQuest")));
    Controller()->InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.Spellbook"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("Spellbook")));
    Controller()->InputComponent->BindAction<FDivineBeastsJournalInputDelegate>(TEXT("DBA.Party"), IE_Pressed, this, &ThisClass::RequestWorldJournal, FName(TEXT("Party")));
    OnInputReceiverChanged.Broadcast();
}

bool UDivineBeastsPlayerControlsComponent::CanProcessWorldInput() const
{
    return Controller()->IsLocalController() && bWorldInputActive && !bWorldGameplayInputSuspended && !Controller()->IsMoveInputIgnored();
}

bool UDivineBeastsPlayerControlsComponent::CanProcessWorldLookInput() const
{
    return CanProcessWorldInput() && !Controller()->IsLookInputIgnored();
}

bool UDivineBeastsPlayerControlsComponent::IsPointerOverWorldViewport() const
{
#if !UE_SERVER
    auto* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr;
    if (!ViewportClient || !FSlateApplication::IsInitialized()) return false;
    const TSharedPtr<SViewport> GameViewport = ViewportClient->GetGameViewportWidget();
    if (!GameViewport.IsValid()) return false;
    auto& Slate = FSlateApplication::Get();
    const auto* LocalPlayer = Controller()->GetLocalPlayer();
    const int32 UserIndex = LocalPlayer ? Slate.GetUserIndexForPlatformUser(LocalPlayer->GetPlatformUserId()).Get(0) : 0;
    // IgnoreEnabledStatus=true是必要的：禁用按钮仍是UI区域，不能把它当作穿透到世界的空白。
    const FWidgetPath Path = Slate.LocateWindowUnderMouse(Slate.GetCursorPos(), Slate.GetInteractiveTopLevelWindows(), true, UserIndex);
    return Path.IsValid() && FDivineBeastsWorldControlState::CanBeginViewportDrag(
        Path.ContainsWidget(GameViewport.Get()), &Path.GetLastWidget().Get() == GameViewport.Get());
#else
    return false; // 无桌面视口的服务器没有本地镜头捕获资格。
#endif
}

void UDivineBeastsPlayerControlsComponent::UpdateWorldInputLocks(bool bNeedsLock)
{
    const int MoveDelta = FDivineBeastsWorldControlState::UpdateOwnedInputLock(bNeedsLock, bOwnsWorldMoveInputLock);
    const int LookDelta = FDivineBeastsWorldControlState::UpdateOwnedInputLock(bNeedsLock, bOwnsWorldLookInputLock);
    if (MoveDelta != 0) Controller()->SetIgnoreMoveInput(MoveDelta > 0);
    if (LookDelta != 0) Controller()->SetIgnoreLookInput(LookDelta > 0);
}

void UDivineBeastsPlayerControlsComponent::ApplyVisibleCursorMode()
{
    if (!Controller()->IsLocalController()) return;
    Controller()->bShowMouseCursor = true;
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    // 菜单暂停时UI客户端拥有焦点和UIOnly模式；失焦清理不能抢回焦点或覆盖该模式。
    if (!bWorldGameplayInputSuspended) Controller()->SetInputMode(Mode);
    // 菜单暂停时捕获/锁也归CommonUI；真实失焦Flush不能把UIOnly改回世界临时捕获。
    if (!bWorldGameplayInputSuspended)
    {
        if (auto* ViewportClient = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
        {
            // 临时捕获才能让FSceneViewport首次按下进入游戏输入；NoCapture会使左右键拖动永远无法启动。
            ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
            ViewportClient->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
    }
    if (bHasSavedCursorPosition)
    {
        Controller()->SetMouseLocation(SavedCursorPosition.X, SavedCursorPosition.Y);
        bHasSavedCursorPosition = false;
    }
}

void UDivineBeastsPlayerControlsComponent::ResetWorldInput()
{
    if (!Controller()->IsLocalController() || Controller()->GetNetMode() == NM_DedicatedServer) return;
    if (bResettingWorldInput) return;
    TGuardValue<bool> Guard(bResettingWorldInput, true);
    MouseButtonMask = 0;
    bPointerClickPending = false;
    PointerPressedTarget.Reset();
    PointerTravelPixels = 0.f;
    Controller()->CurrentMouseCursor = EMouseCursor::Default;
    ForwardAxis = StrafeAxis = KeyboardTurnAxis = 0.f;
    SemanticMoveAxis = FVector2D::ZeroVector; bSemanticMoveReceived = false;
    Controller()->RotationInput = FRotator::ZeroRotator;
    if (APawn* ControlledPawn = InputPawn.Get())
    {
        ControlledPawn->ConsumeMovementInputVector();
        if (auto* ControlledCharacter = Cast<ACharacter>(ControlledPawn)) ControlledCharacter->StopJumping();
        if (auto* ASC = ControlledPawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>()) ASC->ClearAbilityInput();
    }
    EndWorldInteraction();
    // 调基类避免Flush覆盖递归；释放也必须清空技能，丢失keyup不能留下WhileHeld请求。
    if (Controller()->PlayerInput) Controller()->PlayerInput->FlushPressedKeys();
    ApplyVisibleCursorMode();
}

void UDivineBeastsPlayerControlsComponent::SetWorldGameplayInputSuspended(bool bSuspended)
{
    if (!Controller()->IsLocalController() || bWorldGameplayInputSuspended == bSuspended) return;
    bWorldGameplayInputSuspended = bSuspended;
    ResetWorldInput();
    RefreshWorldInputPolicy();
}

void UDivineBeastsPlayerControlsComponent::RefreshWorldInputPolicy()
{
    const bool bActive = bGameplayActive && Controller()->GetPawn() && Controller()->GetPawn()->GetController() == Controller() &&
        !bWorldGameplayInputSuspended && !bWorldTravelPending;
    const bool bPawnChanged = InputPawn.Get() != Controller()->GetPawn();
    if (bPawnChanged || bWorldInputActive != bActive)
    {
        if (bPawnChanged || !bActive) SetSelectedWorldTarget(nullptr);
        ResetWorldInput();
        InputPawn = Controller()->GetPawn();
        bWorldInputActive = bActive;
        UE_LOG(LogTemp, Display, TEXT("SSLMControls: Active=%d ControllerClass=%s PawnBound=%d"),
            bActive, *Controller()->GetClass()->GetPathName(), Controller()->GetPawn() != nullptr);
        if (bPawnChanged && Controller()->GetPawn())
        {
            ObservationRotation = Controller()->GetControlRotation();
            ObservationRotation.Pitch = -15.f;
            if (auto* Boom = Controller()->GetPawn()->FindComponentByClass<USpringArmComponent>())
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
    if (auto* Interactor = Controller()->FindComponentByClass<UGamePlatformInteractorComponent>())
        Interactor->SetLocalFocusPolicy(bActive, true);
}

void UDivineBeastsPlayerControlsComponent::SetCameraMouseButton(bool bRight, bool bPressed)
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
        bHasSavedCursorPosition = Controller()->GetMousePosition(X, Y);
        SavedCursorPosition = FIntPoint(FMath::RoundToInt(X), FMath::RoundToInt(Y));
        Controller()->bShowMouseCursor = false;
        Controller()->SetInputMode(FInputModeGameOnly().SetConsumeCaptureMouseDown(false));
    }
    if (bRight && bPressed)
    {
        FRotator Facing = Controller()->GetControlRotation();
        Facing.Yaw = ObservationRotation.Yaw;
        Controller()->SetControlRotation(Facing);
    }
    if (bWasDragging && !State.IsDragging()) ApplyVisibleCursorMode();
    if (!bPressed && !bRight)
    {
        bPointerClickPending = false;
        // 恢复光标后再次验证UI遮挡和同一命中，避免按住期间目标移开/菜单出现导致误选。
        AActor* ClickedTarget = PointerPressedTarget.Get();
        const bool bCommitClick = bSelectOnRelease && IsPointerOverWorldViewport() && FindPointerTarget() == ClickedTarget;
        // 交互成功可能同步打开页面并重入输入清理，先撤销本次鼠标候选。
        PointerPressedTarget.Reset();
        if (bCommitClick) CommitWorldPointerClick(ClickedTarget);
    }
}

void UDivineBeastsPlayerControlsComponent::BeginLeftCameraDrag() { SetCameraMouseButton(false, true); }

void UDivineBeastsPlayerControlsComponent::EndLeftCameraDrag() { SetCameraMouseButton(false, false); }

void UDivineBeastsPlayerControlsComponent::BeginRightCameraDrag() { SetCameraMouseButton(true, true); }

void UDivineBeastsPlayerControlsComponent::EndRightCameraDrag() { SetCameraMouseButton(true, false); }

void UDivineBeastsPlayerControlsComponent::SetForwardAxis(float Value) { ForwardAxis = !bSemanticMoveOwned && CanProcessWorldInput() ? Value : 0.f; }

void UDivineBeastsPlayerControlsComponent::SetStrafeAxis(float Value) { StrafeAxis = !bSemanticMoveOwned && CanProcessWorldInput() ? Value : 0.f; }

void UDivineBeastsPlayerControlsComponent::SetKeyboardTurnAxis(float Value) { KeyboardTurnAxis = CanProcessWorldInput() ? Value : 0.f; }

void UDivineBeastsPlayerControlsComponent::TurnObservationCamera(float Value)
{
    if (!bSemanticLookOwned) ApplyObservationDelta(Value, 0.f, false);
}

void UDivineBeastsPlayerControlsComponent::LookObservationCamera(float Value)
{
    if (!bSemanticLookOwned) ApplyObservationDelta(0.f, Value, false);
}

void UDivineBeastsPlayerControlsComponent::ZoomObservationCamera(float Value)
{
    if (!CanProcessWorldLookInput() || !Controller()->GetPawn() || FMath::IsNearlyZero(Value) ||
        (MouseButtonMask == 0 && !IsPointerOverWorldViewport())) return;
    if (auto* Boom = Controller()->GetPawn()->FindComponentByClass<USpringArmComponent>())
        Boom->TargetArmLength = FMath::Clamp(Boom->TargetArmLength - Value * ZoomStepCentimeters,
            MinimumCameraDistanceCentimeters, MaximumCameraDistanceCentimeters);
}

void UDivineBeastsPlayerControlsComponent::BeginWorldJump()
{
    if (CanProcessWorldInput()) if (auto* ControlledCharacter = Cast<ACharacter>(Controller()->GetPawn())) ControlledCharacter->Jump();
}

void UDivineBeastsPlayerControlsComponent::EndWorldJump()
{
    if (auto* ControlledCharacter = Cast<ACharacter>(Controller()->GetPawn())) ControlledCharacter->StopJumping();
}

void UDivineBeastsPlayerControlsComponent::RequestWorldMenu()
{
    if (Controller()->IsLocalController() && bGameplayActive) OnWorldEscapeRequested.Broadcast();
}

void UDivineBeastsPlayerControlsComponent::RequestWorldInventory()
{
    // 不能在登录、角色入口、服务器角色或世界准备未完成时打开背包。
    if (Controller()->IsLocalController() && bGameplayActive)
    {
        OnWorldInventoryRequested.Broadcast();
    }
}

void UDivineBeastsPlayerControlsComponent::RequestWorldJournal(FName Section)
{
    if (Controller()->IsLocalController() && bGameplayActive) OnWorldJournalRequested.Broadcast(Section);
}

void UDivineBeastsPlayerControlsComponent::ProcessInput(float DeltaTime, bool bGamePaused)
{
    const FVector2D SemanticAxis = SemanticMoveAxis;
    const bool bUseSemanticMove = bSemanticMoveReceived || bSemanticMoveOwned;
    const FVector2D ResolvedSemanticAxis = bSemanticMoveReceived ? SemanticAxis : FVector2D::ZeroVector;
    bSemanticMoveReceived = false;
    RefreshPointerFeedback();
    if (!CanProcessWorldInput() || bGamePaused || !Controller()->GetPawn()) return;
    const FDivineBeastsWorldControlState State(MouseButtonMask);
    const float TurnDegrees = Controller()->IsLookInputIgnored() ? 0.f : State.ResolveTurn(KeyboardTurnAxis) * KeyboardTurnDegreesPerSecond * DeltaTime;
    FRotator Facing = Controller()->GetControlRotation(); Facing.Yaw += TurnDegrees; Controller()->SetControlRotation(Facing);
    // 左键观察时键盘仍转角色，但观察方位保持独立；松开时也不会骤然改变角色朝向。
    if (!State.IsDragging()) ObservationRotation.Yaw += TurnDegrees;
    const FRotationMatrix Heading(FRotator(0.f, Facing.Yaw, 0.f));
    Controller()->GetPawn()->AddMovementInput(Heading.GetUnitAxis(EAxis::X), State.ResolveForward(bUseSemanticMove ? ResolvedSemanticAxis.Y : ForwardAxis));
    Controller()->GetPawn()->AddMovementInput(Heading.GetUnitAxis(EAxis::Y), State.ResolveStrafe(bUseSemanticMove ? ResolvedSemanticAxis.X : StrafeAxis, KeyboardTurnAxis));
}

void UDivineBeastsPlayerControlsComponent::UpdateObservationRotation(float DeltaTime)
{
    if (Controller()->IsLocalController() && InputPawn.IsValid())
        if (auto* Boom = InputPawn->FindComponentByClass<USpringArmComponent>()) Boom->SetWorldRotation(ObservationRotation);
}

AActor* UDivineBeastsPlayerControlsComponent::FindPointerTarget() const
{
    if (!CanProcessWorldInput() || !GetWorld()) return nullptr;
    FVector Origin, Direction;
    if (!Controller()->DeprojectMousePositionToWorld(Origin, Direction)) return nullptr;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DivineBeastsPointerTarget), false, Controller()->GetPawn());
    const FVector End = Origin + Direction * 10000.f;
    FHitResult VisibilityHit;
    // 最大显示选择距离为100米；交互仍由平台按实际交互距离二次校验，不能使用该距离执行动作。
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(VisibilityHit, Origin, End, ECC_Visibility, Params);
    AActor* VisibleActor = bBlocked ? VisibilityHit.GetActor() : nullptr;
    if (IsValid(VisibleActor) && (Cast<APawn>(VisibleActor) ||
        VisibleActor->FindComponentByClass<UGamePlatformInteractableComponent>())) return VisibleActor;

    // 人物胶囊可能不阻挡Visibility。只补查Pawn对象通道，且不得越过Visibility已命中的遮挡物。
    FHitResult PawnHit;
    if (!GetWorld()->LineTraceSingleByObjectType(PawnHit, Origin, End,
        FCollisionObjectQueryParams(ECC_Pawn), Params)) return nullptr;
    AActor* PawnActor = PawnHit.GetActor();
    return IsValid(PawnActor) && Cast<APawn>(PawnActor) &&
        (!bBlocked || PawnHit.Distance <= VisibilityHit.Distance + 2.f) ? PawnActor : nullptr;
}

void UDivineBeastsPlayerControlsComponent::CommitWorldPointerClick(AActor* Target)
{
    if (!CanProcessWorldInput()) return;
    auto* Entrance = Cast<ADivineBeastsArenaEntrance>(Target);
    if (!Entrance)
    {
        SetSelectedWorldTarget(Target);
        return;
    }
    if (!IsValid(Entrance) || Entrance->GetWorld() != GetWorld()) return;
    auto* Interactor = Controller()->FindComponentByClass<UGamePlatformInteractorComponent>();
    if (!Interactor) return;
    Interactor->RefreshLocalFocus();
    const auto Focus = Interactor->GetCurrentFocus();
    // 只交互本次点击且已通过本地候选过滤的门，不借旧焦点打开另一扇门。
    // 服务器仍重新验证拥有关系、Active、距离、视线和门的代次。
    if (Focus.TargetActor != Entrance || !Focus.IsValid() || !Focus.bLocallyAvailable ||
        Focus.Option.OptionId != FName(TEXT("Arena.Entry.Open")))
    {
        UE_LOG(LogTemp, Display, TEXT("ArenaEntranceClick: Mode=%s FocusAvailable=0"), *Entrance->GetArenaModeId().ToString());
        return;
    }
    const FGuid Request = Interactor->BeginFocusedInteraction();
    UE_LOG(LogTemp, Display, TEXT("ArenaEntranceClick: Mode=%s Requested=%d"), *Entrance->GetArenaModeId().ToString(), Request.IsValid());
}

void UDivineBeastsPlayerControlsComponent::SetSelectedWorldTarget(AActor* Target)
{
    if (Target && (!IsValid(Target) || Target->GetWorld() != GetWorld())) Target = nullptr;
    // 显式点击即重交当前化身意图；同Actor重生或首次复制未就绪后，允许再次点击恢复有效选择。
    // 界面选择不等于攻击授权；受控Pawn可靠提交目标意图，平台按世界/化身/距离二次检查。
    if (APawn* ControlledPawn=Controller()->GetPawn())
        if (auto* Combat=ControlledPawn->FindComponentByClass<UGamePlatformCombatComponent>()) Combat->RequestProjectileTarget(Target);
    // 同Actor重生后显式再点仍须发布选择事件；UI已经在死亡时清理，不能等待引用地址改变。
    if (SelectedWorldTarget.Get() == Target)
    {
        if (Target) OnWorldTargetChanged.Broadcast();
        return;
    }
    if (AActor* Previous = SelectedWorldTarget.Get()) Previous->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleSelectedTargetEndPlay);
    SelectedWorldTarget = Target;
    if (Target) Target->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleSelectedTargetEndPlay);
    OnWorldTargetChanged.Broadcast();
}

void UDivineBeastsPlayerControlsComponent::HandleSelectedTargetEndPlay(AActor* Actor, EEndPlayReason::Type /*Reason*/)
{
    if (SelectedWorldTarget.Get() == Actor) SetSelectedWorldTarget(nullptr);
}

void UDivineBeastsPlayerControlsComponent::RefreshPointerFeedback()
{
    if (!Controller()->IsLocalController()) return;
    if (MouseButtonMask != 0)
    {
        float X = 0.f, Y = 0.f;
        Controller()->GetInputMouseDelta(X, Y);
        PointerTravelPixels += FMath::Abs(X) + FMath::Abs(Y);
        if (!FMath::IsFinite(PointerTravelPixels) || PointerTravelPixels > 6.f) bPointerClickPending = false;
        return;
    }
    Controller()->CurrentMouseCursor = EMouseCursor::Default;
    if (!CanProcessWorldInput() || !Controller()->bShowMouseCursor || !IsPointerOverWorldViewport()) return;
    AActor* Target = FindPointerTarget();
    if (!Target) return;
    if (const auto* Interactable = Target->FindComponentByClass<UGamePlatformInteractableComponent>())
        Controller()->CurrentMouseCursor = Interactable->IsInteractionEnabled() ? EMouseCursor::Hand : EMouseCursor::SlashedCircle;
    else Controller()->CurrentMouseCursor = EMouseCursor::Crosshairs; // 只代表可选目标，不推断敌我或授权攻击。
}

void UDivineBeastsPlayerControlsComponent::BeginWorldInteraction()
{
    if (!CanProcessWorldInput() || MouseButtonMask != 0 || !IsPointerOverWorldViewport()) return;
    if (auto* Interactor = Controller()->FindComponentByClass<UGamePlatformInteractorComponent>()) Interactor->BeginFocusedInteraction();
}

void UDivineBeastsPlayerControlsComponent::EndWorldInteraction()
{
    // 释放/暂停即使世界不再Active也应取消已受理的长按；组件负责节流、所有权及服务器终态校验。
    if (auto* Interactor = Controller()->FindComponentByClass<UGamePlatformInteractorComponent>()) Interactor->CancelCurrentInteraction();
}

void UDivineBeastsPlayerControlsComponent::RequestNextWorldTarget()
{
    UWorld* World = GetWorld();
    APawn* Source = Controller()->GetPawn();
    if (!CanProcessWorldInput() || !World || World->bIsTearingDown || World->IsPaused() || !IsValid(Source)) return;
    // 键盘选择取代未完成的鼠标点击意图；保留RMB观察/转向，松开LMB不能恢复按下时的旧目标。
    bPointerClickPending = false;
    PointerPressedTarget.Reset();

    struct FCandidate { AActor* Actor; double DistanceSquared; FString StablePath; };
    TArray<FCandidate> Candidates;
    FVector ViewLocation; FRotator ViewRotation;
    Controller()->GetPlayerViewPoint(ViewLocation, ViewRotation);
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DivineBeastsTabTarget), false, Source);
    constexpr double MaximumDistanceSquared = 10000.0 * 10000.0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!IsValid(Actor) || Actor == Source || Actor->IsActorBeingDestroyed() || Actor->IsHidden() ||
            (!Cast<APawn>(Actor) && !Actor->FindComponentByClass<UGamePlatformInteractableComponent>())) continue;
        const auto* Combat = Actor->FindComponentByClass<UGamePlatformCombatComponent>();
        if (Combat && (Combat->IsCombatDead() || (Combat->GetCombatMaxHealth() > 0.f && Combat->GetCombatHealth() <= 0.f))) continue;
        const FBox CollisionBounds = Actor->GetComponentsBoundingBox(true);
        const FVector Center = Combat ? Combat->GetCombatTargetPoint() :
            (CollisionBounds.IsValid ? CollisionBounds.GetCenter() : Actor->GetActorLocation());
        const double DistanceSquared = FVector::DistSquared(Source->GetActorLocation(), Center);
        if (!FMath::IsFinite(DistanceSquared) || DistanceSquared > MaximumDistanceSquared) continue;
        FHitResult Hit;
        if (World->LineTraceSingleByChannel(Hit, ViewLocation, Center, ECC_Visibility, Query) && Hit.GetActor() != Actor) continue;
        Candidates.Add({Actor, DistanceSquared, Actor->GetPathName()});
    }
    Candidates.Sort([](const FCandidate& A, const FCandidate& B)
    {
        // 精确距离比较保持严格弱序；同距使用稳定路径，不能让Actor迭代/哈希顺序决定目标。
        return A.DistanceSquared != B.DistanceSquared ? A.DistanceSquared < B.DistanceSquared :
            A.StablePath.Compare(B.StablePath, ESearchCase::CaseSensitive) < 0;
    });
    if (Candidates.IsEmpty()) { SetSelectedWorldTarget(nullptr); return; }
    const int32 Current = Candidates.IndexOfByPredicate([this](const FCandidate& Entry) { return Entry.Actor == SelectedWorldTarget.Get(); });
    SetSelectedWorldTarget(Candidates[(Current + 1) % Candidates.Num()].Actor);
}

UDivineBeastsPlayerControlsComponent::UDivineBeastsPlayerControlsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}
APlayerController* UDivineBeastsPlayerControlsComponent::Controller() const
{ return CastChecked<APlayerController>(GetOwner()); }
void UDivineBeastsPlayerControlsComponent::SetGameplayActive(bool bActive)
{
    if (!Controller()->IsLocalController() || Controller()->GetNetMode() == NM_DedicatedServer) return;
    bGameplayActive = bActive;
    RefreshWorldInputPolicy();
    OnLocalWorldFactsChanged.Broadcast();
}
void UDivineBeastsPlayerControlsComponent::SetTravelPending(bool bPending)
{
    if (!Controller()->IsLocalController() || Controller()->GetNetMode() == NM_DedicatedServer) return;
    bWorldTravelPending = bPending;
    ResetWorldInput();
    RefreshWorldInputPolicy();
    if (!bPending) OnInputReceiverChanged.Broadcast();
}
void UDivineBeastsPlayerControlsComponent::ShutdownControls()
{
    SetSelectedWorldTarget(nullptr);
    bGameplayActive = false;
    ResetWorldInput(); InputPawn.Reset(); bWorldInputActive = false;
    UpdateWorldInputLocks(false);
    OnWorldTargetChanged.Clear(); OnWorldEscapeRequested.Clear(); OnWorldInventoryRequested.Clear();
    OnWorldJournalRequested.Clear(); OnFinalCameraViewChanged.Clear(); OnWorldHealthBarsToggleRequested.Clear();
    OnLocalWorldFactsChanged.Clear();
    OnInputReceiverChanged.Clear();
}
void UDivineBeastsPlayerControlsComponent::EndPlay(const EEndPlayReason::Type Reason)
{ ShutdownControls(); Super::EndPlay(Reason); }
void UDivineBeastsPlayerControlsComponent::ApplySemanticMoveLook(FVector2D Axis, bool bMove, bool bDirectLook)
{
    if (!CanProcessWorldInput() || !FMath::IsFinite(Axis.X) || !FMath::IsFinite(Axis.Y)) return;
    if (bMove) { SemanticMoveAxis = Axis.GetClampedToMaxSize(1.); bSemanticMoveReceived = true; }
    else ApplyObservationDelta(Axis.X, Axis.Y, bDirectLook);
}
void UDivineBeastsPlayerControlsComponent::SetSemanticInputOwned(bool bMoveOwned, bool bLookOwned)
{
    if (bSemanticMoveOwned == bMoveOwned && bSemanticLookOwned == bLookOwned) return;
    bSemanticMoveOwned = bMoveOwned; bSemanticLookOwned = bLookOwned;
    ResetWorldInput();
}
void UDivineBeastsPlayerControlsComponent::ApplyObservationDelta(float Horizontal, float Vertical, bool bDirectLook)
{
    if (!CanProcessWorldLookInput() || (!bDirectLook && MouseButtonMask == 0) ||
        !FMath::IsFinite(Horizontal) || !FMath::IsFinite(Vertical)) return;
    const FDivineBeastsWorldMouseLookDelta Look = bDirectLook ?
        FDivineBeastsWorldMouseLookDelta{Horizontal,Vertical,Horizontal} :
        FDivineBeastsWorldControlState(MouseButtonMask).ResolveMouseLook(Horizontal,Vertical);
    ObservationRotation.Yaw = FRotator::NormalizeAxis(ObservationRotation.Yaw + Look.CameraYawInput);
    ObservationRotation.Pitch = FMath::Clamp(ObservationRotation.Pitch + Look.CameraPitchInput,-80.f,60.f);
    if (!FMath::IsNearlyZero(Look.CharacterYawInput))
    {
        FRotator Facing = Controller()->GetControlRotation(); Facing.Yaw = ObservationRotation.Yaw;
        Controller()->SetControlRotation(Facing);
    }
}
