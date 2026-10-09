// 神兽联盟项目客户端输入适配：订阅本地玩家平台事件并转交受控Pawn/ASC；不重新实现平台绑定或持有服务器权威。
// GI/本地玩家退出先撤销订阅及自有Profile/Context句柄，弱回调避免销毁后访问；视角速率只在消费端乘一次世界秒差。
#include "Subsystems/DivineBeastsInputClientSubsystem.h"

#include "Async/Async.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Feedback/GamePlatformLocalHitstopSubsystem.h"
#include "HAL/PlatformTime.h"
#include "GameFramework/Character.h"
#include "EnhancedInputComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h" // 本文件读取GetDeltaSeconds，需要UWorld完整定义，不能依赖PCH传递头。
#include "EngineGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Input/DivineBeastsInputSemantics.h"
#include "Interfaces/IGamePlatformInputService.h"
#include "Math/RotationMatrix.h"
#include "Services/GamePlatformInputServices.h"
#include "Settings/DivineBeastsInputSettings.h"


void UDivineBeastsInputClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    PlatformInput = LocalPlayer ? IGamePlatformInputService::Get(*LocalPlayer) : nullptr;
    if (!PlatformInput)
    {
        return;
    }

    TWeakObjectPtr<UDivineBeastsInputClientSubsystem> WeakThis(this);
    Subscription = PlatformInput->SubscribeInputEvents(
        this,
        [WeakThis](const FGamePlatformInputEvent& Event)
        {
            if (UDivineBeastsInputClientSubsystem* Self = WeakThis.Get())
            {
                Self->HandlePlatformInput(Event);
            }
        });

    StateSubscription = PlatformInput->SubscribeInputState(
        this,
        [WeakThis](const FGamePlatformInputSnapshot& Snapshot)
        {
            if (UDivineBeastsInputClientSubsystem* Self = WeakThis.Get())
            {
                Self->HandlePlatformState(Snapshot);
            }
        });

    // 项目输入只观察平台视觉暂停结束，不持有其时钟，也不向底层注入项目语义。
    LocalHitstopSubsystem = LocalPlayer
        ? LocalPlayer->GetSubsystem<UGamePlatformLocalHitstopSubsystem>()
        : nullptr;
    if (UGamePlatformLocalHitstopSubsystem* Hitstop = LocalHitstopSubsystem.Get())
    {
        VisualHitstopFinishedHandle = Hitstop->OnVisualHitstopFinished().AddUObject(
            this, &UDivineBeastsInputClientSubsystem::HandleVisualHitstopFinished);
    }

    CachedController = LocalPlayer && GetWorld()
        ? LocalPlayer->GetPlayerController(GetWorld())
        : nullptr;
    RefreshControlledPawn();

    const UDivineBeastsInputSettings* Settings = GetDefault<UDivineBeastsInputSettings>();
    if (Settings && Settings->bAutoActivateGameplayInput &&
        Settings->DefaultGameplayProfileId.IsValid() && !Settings->GameplayContextNames.IsEmpty())
    {
        FGamePlatformResult ActivationResult;
        ActivateGameplayInput(
            Settings->DefaultGameplayProfileId,
            Settings->LocalSettingsKey,
            Settings->GameplayContextNames,
            Settings->GameplayContextPriority,
            ActivationResult);
        // 这里只发起异步准备；最终Ready由平台状态订阅推进，失败不会伪装成可用输入。
    }
}

void UDivineBeastsInputClientSubsystem::Deinitialize()
{
    ClearAbilityInput();
    if (UGamePlatformLocalHitstopSubsystem* Hitstop = LocalHitstopSubsystem.Get())
    {
        if (VisualHitstopFinishedHandle.IsValid())
        {
            Hitstop->OnVisualHitstopFinished().Remove(VisualHitstopFinishedHandle);
        }
    }
    VisualHitstopFinishedHandle.Reset();
    LocalHitstopSubsystem.Reset();

    if (PlatformInput)
    {
        if (StateSubscription.IsValid())
        {
            PlatformInput->UnsubscribeInputState(StateSubscription);
        }
        if (Subscription.IsValid())
        {
            PlatformInput->UnsubscribeInputEvents(Subscription);
        }
        if (ProfileHandle.IsValid())
        {
            PlatformInput->ReleaseInputProfile(ProfileHandle);
        }
    }

    StateSubscription = {};
    Subscription = {};
    ProfileHandle = {};
    BindingHandle = {};
    ContextHandles.Reset();
    RequestedContextNames.Reset();
    CachedController.Reset();
    CachedPawn.Reset();
    CachedAbilitySystem.Reset();
    PlatformInput = nullptr;
    GameplayInputEvent.Clear();
    TargetLockRequested.Clear();
    Super::Deinitialize();
}

void UDivineBeastsInputClientSubsystem::PlayerControllerChanged(APlayerController* Controller)
{
    Super::PlayerControllerChanged(Controller);

    ClearAbilityInput();
    CachedController = Controller;
    CachedPawn.Reset();
    CachedAbilitySystem.Reset();

    // 平台Input子系统会精确撤销旧EnhancedInputComponent绑定；本层只丢弃旧句柄并在下一游戏线程重新绑定。
    BindingHandle = {};
    QueueActivationRefresh();
}

bool UDivineBeastsInputClientSubsystem::ActivateGameplayInput(
    const FPrimaryAssetId& ProfileId,
    const FString& LocalSettingsKey,
    const TArray<FName>& ContextNames,
    int32 ContextPriority,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!PlatformInput || !ProfileId.IsValid() || ContextNames.IsEmpty() || ContextNames.Num() > 8 ||
        ContextPriority < 0 || ContextPriority > 100)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("DivineBeastsInputActivationInvalid"),
            TEXT("神兽联盟输入激活缺少平台服务、Profile、Context或合法优先级。"));
        return false;
    }

    TSet<FName> UniqueContexts;
    for (const FName ContextName : ContextNames)
    {
        if (ContextName.IsNone() || UniqueContexts.Contains(ContextName))
        {
            OutResult = FGamePlatformResult::Failure(
                TEXT("DivineBeastsInputContextInvalid"),
                TEXT("神兽联盟输入Context名称不能为空或重复。"));
            return false;
        }
        UniqueContexts.Add(ContextName);
    }

    if (ProfileHandle.IsValid())
    {
        const FGamePlatformResult ReleaseResult = DeactivateGameplayInput();
        if (!ReleaseResult.IsSuccess())
        {
            OutResult = ReleaseResult;
            return false;
        }
    }

    RequestedContextNames = ContextNames;
    RequestedContextPriority = ContextPriority;
    ProfileHandle = PlatformInput->PrepareInputProfile(ProfileId, LocalSettingsKey, OutResult);
    if (!OutResult.IsSuccess() || !ProfileHandle.IsValid())
    {
        ProfileHandle = {};
        RequestedContextNames.Reset();
        return false;
    }

    QueueActivationRefresh();
    return true;
}

FGamePlatformResult UDivineBeastsInputClientSubsystem::DeactivateGameplayInput()
{
    check(IsInGameThread());
    ClearAbilityInput();

    if (!PlatformInput || !ProfileHandle.IsValid())
    {
        ProfileHandle = {};
        BindingHandle = {};
        ContextHandles.Reset();
        RequestedContextNames.Reset();
        return FGamePlatformResult::Success();
    }

    const FGamePlatformResult Result = PlatformInput->ReleaseInputProfile(ProfileHandle);
    if (!Result.IsSuccess())
    {
        return Result;
    }

    ProfileHandle = {};
    BindingHandle = {};
    ContextHandles.Reset();
    RequestedContextNames.Reset();
    return FGamePlatformResult::Success();
}

bool UDivineBeastsInputClientSubsystem::IsGameplayInputReady() const
{
    if (!PlatformInput || !ProfileHandle.IsValid() || !BindingHandle.IsValid())
    {
        return false;
    }
    const FGamePlatformInputSnapshot Snapshot = PlatformInput->GetInputSnapshot();
    return Snapshot.bProfilePrepared && Snapshot.bBindingsReady &&
        Snapshot.ProfileGeneration == ProfileHandle.Generation;
}

void UDivineBeastsInputClientSubsystem::HandlePlatformState(
    const FGamePlatformInputSnapshot& Snapshot)
{
    // 该回调运行在平台输入的只读广播栈中，禁止同步Acquire/Bind；这里只排队一次安全刷新。
    if (ProfileHandle.IsValid() &&
        (Snapshot.ProfileGeneration == ProfileHandle.Generation || Snapshot.ProfileGeneration == 0))
    {
        QueueActivationRefresh();
    }
}

void UDivineBeastsInputClientSubsystem::QueueActivationRefresh()
{
    if (bActivationRefreshQueued)
    {
        return;
    }
    bActivationRefreshQueued = true;

    TWeakObjectPtr<UDivineBeastsInputClientSubsystem> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis]()
    {
        if (UDivineBeastsInputClientSubsystem* Self = WeakThis.Get())
        {
            Self->bActivationRefreshQueued = false;
            Self->TryFinalizeActivation();
        }
    });
}

void UDivineBeastsInputClientSubsystem::TryFinalizeActivation()
{
    check(IsInGameThread());
    if (!PlatformInput || !ProfileHandle.IsValid())
    {
        return;
    }

    const FGamePlatformInputSnapshot Snapshot = PlatformInput->GetInputSnapshot();
    if (!Snapshot.bProfilePrepared || Snapshot.ProfileGeneration != ProfileHandle.Generation)
    {
        return;
    }

    if (ContextHandles.IsEmpty())
    {
        TArray<FGamePlatformInputContextHandle> NewHandles;
        for (const FName ContextName : RequestedContextNames)
        {
            FGamePlatformResult Result;
            const FGamePlatformInputContextHandle Handle = PlatformInput->AcquireInputContext(
                ContextName,
                RequestedContextPriority,
                this,
                Result);
            if (!Result.IsSuccess() || !Handle.IsValid())
            {
                for (const FGamePlatformInputContextHandle& Acquired : NewHandles)
                {
                    PlatformInput->ReleaseInputContext(Acquired);
                }
                return;
            }
            NewHandles.Add(Handle);
        }
        ContextHandles = MoveTemp(NewHandles);
    }

    ULocalPlayer* LocalPlayer = GetLocalPlayer();
    APlayerController* Controller = LocalPlayer && GetWorld()
        ? LocalPlayer->GetPlayerController(GetWorld())
        : nullptr;
    CachedController = Controller;
    if (!Controller)
    {
        return;
    }

    if (!BindingHandle.IsValid())
    {
        UEnhancedInputComponent* EnhancedComponent = Cast<UEnhancedInputComponent>(Controller->InputComponent);
        if (!EnhancedComponent && Controller->GetPawn())
        {
            EnhancedComponent = Cast<UEnhancedInputComponent>(Controller->GetPawn()->InputComponent);
        }
        if (!EnhancedComponent)
        {
            return;
        }

        FGamePlatformResult Result;
        BindingHandle = PlatformInput->BindInputReceiver(*EnhancedComponent, Result);
        if (!Result.IsSuccess() || !BindingHandle.IsValid())
        {
            BindingHandle = {};
            return;
        }
    }

    RefreshControlledPawn();
}

void UDivineBeastsInputClientSubsystem::RefreshControlledPawn()
{
    APlayerController* Controller = CachedController.Get();
    if (!Controller)
    {
        ULocalPlayer* LocalPlayer = GetLocalPlayer();
        Controller = LocalPlayer && GetWorld() ? LocalPlayer->GetPlayerController(GetWorld()) : nullptr;
        CachedController = Controller;
    }

    APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
    if (CachedPawn.Get() == Pawn)
    {
        return;
    }

    ClearAbilityInput();
    CachedPawn = Pawn;
    CachedAbilitySystem.Reset();
    if (!Pawn)
    {
        return;
    }

    UGamePlatformAbilitySystemComponent* AbilitySystem =
        Pawn->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    if (!AbilitySystem && Controller && Controller->PlayerState)
    {
        AbilitySystem = Controller->PlayerState->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    }
    CachedAbilitySystem = AbilitySystem;
}

void UDivineBeastsInputClientSubsystem::ClearAbilityInput()
{
    // 控制角色更换、输入绑定撤销或应用卸载时，绝不可回放旧技能指令。
    BufferedAbilityInputs.Reset();
    if (UGamePlatformAbilitySystemComponent* AbilitySystem = CachedAbilitySystem.Get())
    {
        AbilitySystem->ClearAbilityInput();
    }
    LastAbilityProcessFrame = MAX_uint64;
}

void UDivineBeastsInputClientSubsystem::ProcessAbilityInputOncePerFrame()
{
    UGamePlatformAbilitySystemComponent* AbilitySystem = CachedAbilitySystem.Get();
    if (!AbilitySystem || LastAbilityProcessFrame == GFrameCounter)
    {
        return;
    }
    LastAbilityProcessFrame = GFrameCounter;
    AbilitySystem->ProcessAbilityInput();
}

void UDivineBeastsInputClientSubsystem::HandleMoveLookInput(
    const FGamePlatformInputEvent& Event)
{
    if (Event.Phase != ETriggerEvent::Triggered || Event.Value.GetValueType() != EInputActionValueType::Axis2D)
    {
        return;
    }

    RefreshControlledPawn();
    APlayerController* Controller = CachedController.Get();
    APawn* Pawn = CachedPawn.Get();
    if (!Controller)
    {
        return;
    }

    const FGameplayTag SemanticTag = Event.SemanticId.Tag;
    const FVector2D Axis = Event.Value.Get<FVector2D>();
    if (SemanticTag == GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move))
    {
        if (!Pawn || Event.Unit != EGamePlatformInputUnit::NormalizedAxis)
        {
            return;
        }

        const FRotator YawRotation(0.0, Controller->GetControlRotation().Yaw, 0.0);
        const FRotationMatrix Rotation(YawRotation);
        Pawn->AddMovementInput(Rotation.GetUnitAxis(EAxis::X), Axis.Y);
        Pawn->AddMovementInput(Rotation.GetUnitAxis(EAxis::Y), Axis.X);
        return;
    }

    const bool bLookDelta =
        SemanticTag == GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookDelta);
    const bool bLookRate =
        SemanticTag == GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookRate);
    if (!bLookDelta && !bLookRate)
    {
        return;
    }

    double Scale = 1.0;
    if (bLookRate)
    {
        if (Event.Unit != EGamePlatformInputUnit::DegreesPerSecond || !GetWorld())
        {
            return;
        }
        // LookRate是度/秒，最终视角消费端在这里且只在这里乘一次DeltaSeconds。
        Scale = GetWorld()->GetDeltaSeconds();
    }
    else if (Event.Unit != EGamePlatformInputUnit::DegreesDelta)
    {
        return;
    }

    Controller->AddYawInput(static_cast<float>(Axis.X * Scale));
    Controller->AddPitchInput(static_cast<float>(Axis.Y * Scale));
}

bool UDivineBeastsInputClientSubsystem::IsLocalPawnVisualHitstopActive() const
{
    const APawn* Pawn = CachedPawn.Get();
    const UGamePlatformLocalHitstopSubsystem* Hitstop = LocalHitstopSubsystem.Get();
    if (!Pawn || !Hitstop)
    {
        return false;
    }

    // 与MobaPresentation的Character主网格提取保持一致，只查询本地受控Pawn。
    const ACharacter* Character = Cast<ACharacter>(Pawn);
    const USkeletalMeshComponent* Mesh = Character
        ? Character->GetMesh() : Pawn->FindComponentByClass<USkeletalMeshComponent>();
    return Mesh && Hitstop->IsVisualHitstopActive(Mesh);
}

void UDivineBeastsInputClientSubsystem::FlushBufferedAbilityInput()
{
    if (bReplayingBufferedAbilityInput || BufferedAbilityInputs.Num() == 0)
    {
        return;
    }
    RefreshControlledPawn();
    if (IsLocalPawnVisualHitstopActive())
    {
        return; // 仅视觉动画恢复后重放，绝不提前消费。
    }
    if (!PlatformInput || !CachedAbilitySystem.IsValid() || !GetWorld())
    {
        BufferedAbilityInputs.Reset();
        return;
    }

    const FGamePlatformInputSnapshot Snapshot = PlatformInput->GetInputSnapshot();
    TArray<FGamePlatformInputEvent> ReadyEvents;
    BufferedAbilityInputs.ConsumePending(
        FPlatformTime::Seconds(), Snapshot.BindingGeneration, ReadyEvents);
    if (ReadyEvents.IsEmpty())
    {
        return;
    }

    // 不重复广播物理事件，也不绕过GAS合法性检查。所有回放仍由现存AbilityInputToken校验。
    bReplayingBufferedAbilityInput = true;
    for (const FGamePlatformInputEvent& BufferedEvent : ReadyEvents)
    {
        HandleProjectGameplayInput(BufferedEvent);
    }
    bReplayingBufferedAbilityInput = false;
}

void UDivineBeastsInputClientSubsystem::HandleVisualHitstopFinished(
    USkeletalMeshComponent* RestoredMesh)
{
    if (!IsInGameThread() || !IsValid(RestoredMesh))
    {
        return;
    }
    RefreshControlledPawn();
    const APawn* Pawn = CachedPawn.Get();
    const ACharacter* Character = Cast<ACharacter>(Pawn);
    const USkeletalMeshComponent* LocalMesh = Character
        ? Character->GetMesh()
        : (Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr);
    if (LocalMesh == RestoredMesh)
    {
        // 仅当前本地角色的视觉暂停结束，才触发技能输入消费。
        FlushBufferedAbilityInput();
    }
}

void UDivineBeastsInputClientSubsystem::HandleProjectGameplayInput(
    const FGamePlatformInputEvent& Event)
{
    RefreshControlledPawn();
    const FGameplayTag SemanticTag = Event.SemanticId.Tag;
    if (SemanticTag == DivineBeastsInputSemantics::TargetLock())
    {
        if (Event.Phase == ETriggerEvent::Started)
        {
            TargetLockRequested.Broadcast(CachedPawn.Get());
        }
        return;
    }

    const FGameplayTag AbilityInputTag = DivineBeastsInputSemantics::ToAbilityInputTag(SemanticTag);
    UGamePlatformAbilitySystemComponent* AbilitySystem = CachedAbilitySystem.Get();
    if (!AbilityInputTag.IsValid() || !AbilitySystem)
    {
        return;
    }

    if (!bReplayingBufferedAbilityInput && IsLocalPawnVisualHitstopActive())
    {
        // 只记录离散按下/释放的物理事件；连续移动和视角仍照常响应。
        // 缓冲入口完全不驱动技能，其有效性由恢复后的GAS Token决定。
        if (Event.Phase == ETriggerEvent::Started ||
            Event.Phase == ETriggerEvent::Completed ||
            Event.Phase == ETriggerEvent::Canceled)
        {
            BufferedAbilityInputs.Enqueue(Event, FPlatformTime::Seconds());
        }
        return;
    }
    if (!bReplayingBufferedAbilityInput && BufferedAbilityInputs.Num() > 0)
    {
        FlushBufferedAbilityInput(); // 即使通知稍晚，也优先重放旧命令。
    }

    const FGamePlatformAbilityInputToken Token = AbilitySystem->GetInputToken();
    if (!Token.IsValid())
    {
        return;
    }

    if (Event.Phase == ETriggerEvent::Started)
    {
        if (AbilitySystem->AbilityInputPressed(AbilityInputTag, Token).IsSuccess())
        {
            ProcessAbilityInputOncePerFrame();
        }
    }
    else if (Event.Phase == ETriggerEvent::Triggered || Event.Phase == ETriggerEvent::Ongoing)
    {
        ProcessAbilityInputOncePerFrame();
    }
    else if (Event.Phase == ETriggerEvent::Completed || Event.Phase == ETriggerEvent::Canceled ||
        Event.EndReason != EGamePlatformInputEndReason::None)
    {
        AbilitySystem->AbilityInputReleased(AbilityInputTag, Token);
    }
}

void UDivineBeastsInputClientSubsystem::HandlePlatformInput(
    const FGamePlatformInputEvent& Event)
{
    if (!Event.SemanticId.IsValid())
    {
        return;
    }

    const FGameplayTag SemanticTag = Event.SemanticId.Tag;
    if (SemanticTag == GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move) ||
        SemanticTag == GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookDelta) ||
        SemanticTag == GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::LookRate))
    {
        HandleMoveLookInput(Event);
        return;
    }

    if (!DivineBeastsInputSemantics::IsCoreGameplaySemantic(SemanticTag))
    {
        return;
    }

    // 对外广播仍是项目只读请求事件；内部路由同时把攻击/技能交给平台ASC，把TargetLock交给项目目标系统。
    GameplayInputEvent.Broadcast(Event);
    HandleProjectGameplayInput(Event);
}

FGamePlatformInputTouchHandle UDivineBeastsInputClientSubsystem::BeginProjectTouchInput(
    int32 PointerId,
    FGameplayTag SemanticTag,
    TWeakObjectPtr<UObject> Owner,
    FGamePlatformResult& OutResult)
{
    if (!PlatformInput || !DivineBeastsInputSemantics::IsCoreGameplaySemantic(SemanticTag))
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("DivineBeastsInputUnavailable"),
            TEXT("神兽联盟输入服务不可用或Touch语义不属于项目核心输入集合。"));
        return {};
    }

    FGamePlatformInputSemanticId SemanticId;
    SemanticId.Tag = SemanticTag;
    return PlatformInput->BeginTouchInputBySemantic(
        PointerId,
        SemanticId,
        Owner,
        OutResult);
}

FGamePlatformResult UDivineBeastsInputClientSubsystem::UpdateProjectTouchInput(
    const FGamePlatformInputTouchHandle& Handle,
    const FInputActionValue& Value)
{
    return PlatformInput
        ? PlatformInput->UpdateTouchInput(Handle, Value)
        : FGamePlatformResult::Failure(TEXT("DivineBeastsInputUnavailable"), TEXT("神兽联盟输入服务不可用。"));
}

FGamePlatformResult UDivineBeastsInputClientSubsystem::EndProjectTouchInput(
    const FGamePlatformInputTouchHandle& Handle)
{
    return PlatformInput
        ? PlatformInput->EndTouchInput(Handle)
        : FGamePlatformResult::Failure(TEXT("DivineBeastsInputUnavailable"), TEXT("神兽联盟输入服务不可用。"));
}
