// Dedicated Server每World竞技适配：订阅GameInstance已验证连接，精确比赛/实例/体验隔离；退出解绑和撤销本世界资格。
#include "Server/GamePlatformArenaServerSubsystem.h"

#include "Backend/GamePlatformArenaServerBackendClient.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Server/GamePlatformArenaServerCoordinator.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"
#include "Features/IModularFeatures.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Server/GamePlatformServerAdmissionSubsystem.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Async/Async.h"
#include "Server/ArenaVerifiedAdmissionPolicy.h"
#include "TimerManager.h"
#include "HAL/PlatformMisc.h"

bool UGamePlatformArenaServerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World != nullptr && World->GetNetMode() == NM_DedicatedServer;
}

void UGamePlatformArenaServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        AdmissionSubsystem = Instance->GetSubsystem<UGamePlatformServerAdmissionSubsystem>();
        if (AdmissionSubsystem.IsValid())
        { AdmissionChangedHandle = AdmissionSubsystem->OnAdmissionChanged().AddUObject(this, &UGamePlatformArenaServerSubsystem::HandleAdmissionChanged); }
    }

    if(auto* Instance=GetWorld()?GetWorld()->GetGameInstance():nullptr)
    {
        LifecycleSubsystem=Instance->GetSubsystem<UGamePlatformServerLifecycleSubsystem>();
        if(auto* Lifecycle=LifecycleSubsystem.Get())LifecycleChangedHandle=Lifecycle->OnLifecycleChanged().AddUObject(this,&ThisClass::HandleLifecycleChanged);
    }

}

void UGamePlatformArenaServerSubsystem::Deinitialize()
{
    ++AssignmentGeneration;bWorldStarted=false;
    if(auto* Lifecycle=LifecycleSubsystem.Get())Lifecycle->OnLifecycleChanged().Remove(LifecycleChangedHandle);LifecycleChangedHandle.Reset();LifecycleSubsystem.Reset();
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AssignmentRetryTimer);
        World->GetTimerManager().ClearTimer(SafeDrainTimer);
    }
    if (auto* Mode = ObservedArenaMode.Get())
    {
        // 先解除结果通知，再取消HTTP：后端迟到的结果不得唤醒旧World的排空流程。
        Mode->OnResultPending().Remove(ResultPendingHandle);
    }
    ResultPendingHandle.Reset();
    ObservedArenaMode.Reset();
    bResultCommitted = false;
    bZeroHeartbeatRequested = false;
    bSafeDrainRequested = false;
    if(BackendClient)BackendClient->CancelAll();
    ResultHeartbeatBaselineGeneration = 0;
    if (AdmissionSubsystem.IsValid()) { AdmissionSubsystem->OnAdmissionChanged().Remove(AdmissionChangedHandle); }
    if (auto* World = GetWorld())
    {
        auto* Mode = World->GetAuthGameMode<AGamePlatformArenaGameMode>();
        for (const auto& Pair : AdmittedConnections)
        {
            FTimerHandle OwnedDeadlineTimer = Pair.Value.AuthorityDeadlineTimer;
            World->GetTimerManager().ClearTimer(OwnedDeadlineTimer);
            if (Mode) { Mode->MarkPlayerDisconnected(Pair.Value.PlayerId); }
        }
    }
    AdmittedConnections.Reset(); AdmissionSubsystem.Reset();
    Coordinator.Reset();
    BackendClient.Reset();
    Super::Deinitialize();
}

void UGamePlatformArenaServerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    bWorldStarted=true;
    AGamePlatformArenaGameMode* ArenaGameMode = InWorld.GetAuthGameMode<AGamePlatformArenaGameMode>();
    if (ArenaGameMode == nullptr) { return; }

    ObservedArenaMode = ArenaGameMode;
    const TWeakObjectPtr<AGamePlatformArenaGameMode> WeakArenaMode(ArenaGameMode);
    const TWeakObjectPtr<UGamePlatformArenaServerSubsystem> WeakSelf(this);
    ResultPendingHandle = ArenaGameMode->OnResultPending().AddWeakLambda(
        this,
        [WeakSelf, WeakArenaMode](const FGamePlatformArenaMatchResult&)
        {
            auto* Self = WeakSelf.Get();
            auto* Mode = WeakArenaMode.Get();
            if (!Self || !Mode || !Self->bWorldStarted || !Self->Coordinator.IsValid() ||
                Self->GetWorld() != Mode->GetWorld()) { return; }
            Self->Coordinator->SubmitPendingResult(
                Mode,
                [WeakSelf, WeakArenaMode](bool bCommitted, const FString& Error)
                {
                    auto* Current = WeakSelf.Get();
                    auto* CurrentMode = WeakArenaMode.Get();
                    if (!Current || !CurrentMode || !Current->bWorldStarted ||
                        !Current->GetWorld() || Current->GetWorld()->bIsTearingDown ||
                        Current->GetWorld()->GetAuthGameMode<AGamePlatformArenaGameMode>() != CurrentMode)
                    {
                        return;
                    }
                    if (!bCommitted)
                    {
                        UE_LOG(LogTemp, Error, TEXT("MainArena MatchResult remains pending: %s"), *Error);
                        return;
                    }
                    if (CurrentMode->GetMatchPhase() != EGamePlatformArenaMatchPhase::Completed)
                    {
                        // 后端提交成功还不等于当前世界状态可回收；已切换的阶段绝不关闭服务器。
                        UE_LOG(LogTemp, Warning, TEXT("MainArena result committed but match phase is not Completed."));
                        return;
                    }
                    // 先冻结历史成功心跳的代次；不能让开赛前的0人回执授权当前比赛退出。
                    if (auto* Lifecycle = Current->LifecycleSubsystem.Get())
                    {
                        Current->ResultHeartbeatBaselineGeneration = Lifecycle->GetIssuedOperationGeneration();
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("MainArena result committed without server lifecycle; defer shutdown."));
                        return;
                    }
                    Current->bResultCommitted = true;
                    // 只在后端结果终态后短期检查玩家是否真实离开；不使用逐帧Tick，不强制踢人。
                    Current->GetWorld()->GetTimerManager().SetTimer(
                        Current->SafeDrainTimer, Current,
                        &UGamePlatformArenaServerSubsystem::TryFinishArenaDrain,
                        5.0f, true);
                    Current->TryFinishArenaDrain();
                    UE_LOG(LogTemp, Log, TEXT("MainArena result committed; waiting for zero live players before Agones drain."));
                });
        });

    TryLoadAssignment();
}

void UGamePlatformArenaServerSubsystem::RefreshVerifiedConnections()
{
    UWorld* World = GetWorld();
    if (!World || World->bIsTearingDown) { return; }
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    { if (auto* Controller = It->Get()) { TryAdmitVerifiedConnection(*Controller); } }
}
void UGamePlatformArenaServerSubsystem::HandleAdmissionChanged(const APlayerController* Controller,
    const FGamePlatformServerVerifiedAdmission& Admission, bool bAdmitted)
{
    check(IsInGameThread());
    // GI可同时服务多个World；这个适配器只能操作自己世界中的实际连接。
    if (!IsValid(Controller) || Controller->GetWorld() != GetWorld()) { return; }
    if (bAdmitted) { TryAdmitVerifiedConnection(*const_cast<APlayerController*>(Controller)); }
    else { ExpireVerifiedConnection(const_cast<APlayerController*>(Controller), Admission.AdmissionId); }
}
void UGamePlatformArenaServerSubsystem::TryAdmitVerifiedConnection(APlayerController& Controller)
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    auto* Mode = World ? World->GetAuthGameMode<AGamePlatformArenaGameMode>() : nullptr;
    auto* State = Controller.GetPlayerState<AGamePlatformArenaPlayerState>();
    if (!Mode || !State || !AdmissionSubsystem.IsValid() || Controller.GetWorld() != World || World->bIsTearingDown) { return; }
    FGamePlatformServerVerifiedAdmission Admission;
    // 必须再次查询当前实际连接；广播参数本身及客户端自报PlayerId均不能当认证凭据。
    if (!AdmissionSubsystem->GetVerifiedAdmission(Controller, Admission)) { return; }
    const auto& Assignment = Mode->GetAssignment();
    const auto* Slot = GamePlatformArenaVerifiedAdmissionPolicy::ResolveRosterSlot(Admission, Assignment);
    if (!Slot) { return; }
    if (const auto* Existing = AdmittedConnections.Find(&Controller))
    { if (Existing->AdmissionId == Admission.AdmissionId) { return; } }
    FGamePlatformArenaTransferTicketClaims Claims;
    Claims.TicketId = Admission.AdmissionId.ToString(); Claims.PlayerId = Admission.PlayerId;
    // CharacterId由已验证Assignment的唯一Roster决定；票据合同没有该字段，禁止伪造或从客户端取得。
    Claims.CharacterId = Slot->CharacterId; Claims.SessionId = Admission.SessionId; Claims.MatchId = Assignment.MatchId;
    Claims.DestinationServerId = Admission.ServerInstanceId; Claims.ExpiresAtUtc = Admission.AuthorityUntil; Claims.bConsumed = true;
    FString Error;
    const bool bAccepted = Mode->IsPlayerAwaitingReconnect(Admission.PlayerId)
        ? Mode->TryReconnectPlayer(State, Admission.PlayerId, Error) : Mode->AdmitPlayer(State, Claims, Error);
    if (!bAccepted) { UE_LOG(LogTemp, Warning, TEXT("Verified Arena connection refused: %s"), *Error); return; }
    FGamePlatformServerVerifiedAdmission Current;
    const double RemainingSeconds = (Admission.AuthorityUntil - FDateTime::UtcNow()).GetTotalSeconds();
    if (!AdmissionSubsystem.IsValid() || !AdmissionSubsystem->GetVerifiedAdmission(Controller, Current) ||
        Current.AdmissionId != Admission.AdmissionId || RemainingSeconds <= 0.0)
    { Mode->MarkPlayerDisconnected(Admission.PlayerId); return; }
    // 重连发布新绑定时先撤销同玩家旧连接Timer；旧连接后来的撤销/超时不能使新Pawn失活。
    for (auto It = AdmittedConnections.CreateIterator(); It; ++It)
    {
        if (It.Value().PlayerId == Admission.PlayerId)
        { World->GetTimerManager().ClearTimer(It.Value().AuthorityDeadlineTimer); It.RemoveCurrent(); }
    }
    FAdmittedConnection& Binding = AdmittedConnections.FindOrAdd(&Controller);
    World->GetTimerManager().ClearTimer(Binding.AuthorityDeadlineTimer);
    Binding.AdmissionId = Admission.AdmissionId; Binding.PlayerId = Admission.PlayerId;
    const TWeakObjectPtr<APlayerController> WeakController(&Controller);
    World->GetTimerManager().SetTimer(Binding.AuthorityDeadlineTimer,
        FTimerDelegate::CreateUObject(this, &UGamePlatformArenaServerSubsystem::ExpireVerifiedConnection, WeakController, Admission.AdmissionId),
        static_cast<float>(RemainingSeconds), false);
}
void UGamePlatformArenaServerSubsystem::ExpireVerifiedConnection(TWeakObjectPtr<APlayerController> Controller, FGuid AdmissionId)
{
    FAdmittedConnection* Existing = AdmittedConnections.Find(Controller);
    if (!Existing || Existing->AdmissionId != AdmissionId) { return; }
    const FAdmittedConnection Binding = *Existing;
    AdmittedConnections.Remove(Controller);
    if (auto* World = GetWorld())
    {
        FTimerHandle OwnedDeadlineTimer = Binding.AuthorityDeadlineTimer;
        World->GetTimerManager().ClearTimer(OwnedDeadlineTimer);
        if (auto* Mode = World->GetAuthGameMode<AGamePlatformArenaGameMode>()) { Mode->MarkPlayerDisconnected(Binding.PlayerId); }
    }
}

void UGamePlatformArenaServerSubsystem::HandleLifecycleChanged(const FGamePlatformServerLifecycleSnapshot&){TryLoadAssignment();}

// 服务器空人回收纯安全门禁：结果、阶段和实例身份必须均为真实权威，
// UI关闭、玩家状态副本或单次心跳零人不能独立授权Agones终止。
bool UGamePlatformArenaServerSubsystem::CanRequestSafeDrain(
    bool bResultConfirmed, bool bMatchCompleted,
    bool bSameGameServer, bool bHasLivePlayerControllers)
{
    return bResultConfirmed && bMatchCompleted && bSameGameServer && !bHasLivePlayerControllers;
}

void UGamePlatformArenaServerSubsystem::TryFinishArenaDrain()
{
    check(IsInGameThread());
    UWorld* World = GetWorld();
    if (!bWorldStarted || !bResultCommitted || !World || World->bIsTearingDown ||
        World->GetNetMode() != NM_DedicatedServer)
    {
        return;
    }

    AGamePlatformArenaGameMode* Mode = World->GetAuthGameMode<AGamePlatformArenaGameMode>();
    UGamePlatformServerLifecycleSubsystem* Lifecycle = LifecycleSubsystem.Get();
    if (!Mode || Mode != ObservedArenaMode.Get() || !Lifecycle)
    {
        return;
    }

    const FGamePlatformServerLifecycleSnapshot Snapshot = Lifecycle->GetSnapshot();
    const FString& MatchGameServerId = Mode->GetAssignment().GameServerId;
    bool bHasLiveControllers = false;
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        if (IsValid(It->Get()))
        {
            // 以真实控制器是否仍属于本世界作为最保守的迁出条件；不使用UI计数或过期Roster。
            bHasLiveControllers = true;
            break;
        }
    }
    if (!CanRequestSafeDrain(bResultCommitted,
        Mode->GetMatchPhase() == EGamePlatformArenaMatchPhase::Completed,
        !MatchGameServerId.IsEmpty() && MatchGameServerId == Snapshot.GameServerId,
        bHasLiveControllers))
    {
        return;
    }

    // 结果已提交且真实世界零人后，先关闭本地新准入，避免零心跳与SDK Shutdown之间重新出现玩家。
    if (auto* Admission = AdmissionSubsystem.Get())
    {
        Admission->StopAcceptingAdmissions();
    }

    if (Snapshot.State == EGamePlatformServerLifecycleState::Stopped)
    {
        World->GetTimerManager().ClearTimer(SafeDrainTimer);
        return;
    }
    if (bSafeDrainRequested)
    {
        if (Snapshot.State == EGamePlatformServerLifecycleState::Draining &&
            Lifecycle->CompleteDrain())
        {
            // CompleteDrain是平台确认后的本地终态；Agones适配器只在该终态发送SDK Shutdown。
            World->GetTimerManager().ClearTimer(SafeDrainTimer);
            UE_LOG(LogTemp, Log, TEXT("MainArena empty and committed: controlled drain completed; Agones may shut down."));
        }
        return;
    }
    if (Snapshot.State != EGamePlatformServerLifecycleState::Registered &&
        Snapshot.State != EGamePlatformServerLifecycleState::Ready)
    {
        return;
    }

    if (!bZeroHeartbeatRequested)
    {
        // 后端FinalizeArena只接受晚于结果持久提交的真实零人数心跳；网络请求在途时下一次安全重试。
        // 平台BeginDrain会串行等待该心跳完成，不能用客户端人数或虚拟快照提前宣布成功。
        bZeroHeartbeatRequested = Lifecycle->SendHeartbeat(0);
        return;
    }
    // 必须先获得结果提交后的零人心跳真正成功回执；发送成功仅表示HTTP已开始。
    const bool bConfirmedZeroAfterResult =
        Lifecycle->GetLastAcknowledgedHeartbeatPlayers() == 0 &&
        Lifecycle->GetLastAcknowledgedHeartbeatGeneration() > ResultHeartbeatBaselineGeneration;
    if (!bConfirmedZeroAfterResult)
    {
        // 5秒下一轮重试暂时失败的零心跳；不把请求发送、旧ACK或网络未知结果作为授权。
        if (Snapshot.ConsecutiveHeartbeatFailures > 0)
        {
            bZeroHeartbeatRequested = false;
        }
        return;
    }
    if (Lifecycle->BeginDrain())
    {
        bSafeDrainRequested = true;
    }
}

void UGamePlatformArenaServerSubsystem::TryLoadAssignment()
{
    auto* World=GetWorld();auto* Mode=World?World->GetAuthGameMode<AGamePlatformArenaGameMode>():nullptr;auto* Lifecycle=LifecycleSubsystem.Get();
    if(!bWorldStarted||!World||World->bIsTearingDown||!Mode||!Lifecycle||bAssignmentFailed||bAssignmentLoaded||bAssignmentRequestPending)return;
    const auto Phase=Lifecycle->GetSnapshot().State;
    if(Phase!=EGamePlatformServerLifecycleState::Registered&&Phase!=EGamePlatformServerLifecycleState::PublishingReady&&Phase!=EGamePlatformServerLifecycleState::Ready)return;
    if(!Coordinator)
    {
    FGamePlatformArenaServerBackendConfig Config;
    Config.BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_BASE_URL"));
    Config.InternalToken = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_INTERNAL_TOKEN"));
    Config.GameServerId=Lifecycle->GetSnapshot().GameServerId;
    Config.ServerBootId=Lifecycle->GetRegisteredServerBootId();
    if (!Config.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("Arena server backend config incomplete; assignment auto-load disabled."));
        return;
    }

    BackendClient = MakeShared<FGamePlatformArenaServerBackendClient>(MoveTemp(Config));
    Coordinator = MakeShared<FGamePlatformArenaServerCoordinator>(BackendClient.ToSharedRef());

    const TArray<IGamePlatformArenaServerProjectExtension*> Extensions =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<IGamePlatformArenaServerProjectExtension>(
                IGamePlatformArenaServerProjectExtension::GetModularFeatureName());
    if (Extensions.Num() > 1)
    {
        UE_LOG(LogTemp, Error, TEXT("Arena server project extension conflict: %d providers registered."), Extensions.Num());
        Coordinator.Reset();
        BackendClient.Reset();
        bAssignmentFailed=true;return;
    }
    Coordinator->SetProjectExtension(Extensions.Num() == 1 ? Extensions[0] : nullptr);
    }
    if(!Coordinator)return;
    bAssignmentRequestPending=true;const uint64 Generation=++AssignmentGeneration;
    Coordinator->LoadAndApplyAssignment(Mode,[Weak=TWeakObjectPtr<UGamePlatformArenaServerSubsystem>(this),Generation](bool bSuccess,const FString& Error)
    {
        auto* Self=Weak.Get();if(!Self||!Self->bWorldStarted||Self->AssignmentGeneration!=Generation||!Self->bAssignmentRequestPending)return;
        Self->bAssignmentRequestPending=false;
        if(bSuccess){Self->bAssignmentLoaded=true;Self->RefreshVerifiedConnections();UE_LOG(LogTemp,Log,TEXT("MainArena assignment loaded and validated."));return;}
        if(Error==TEXT("AssignmentPending"))
        {
            Self->AssignmentFailures=0;
            // 空等待由控制面20秒挂起或真实分配事件唤醒；立即续接订阅，不按本地刷新周期扫描业务。
            AsyncTask(ENamedThreads::GameThread,[Weak,Generation](){if(auto* Current=Weak.Get())if(Current->AssignmentGeneration==Generation)Current->TryLoadAssignment();});return;
        }
        if(++Self->AssignmentFailures>4){Self->bAssignmentFailed=true;UE_LOG(LogTemp,Error,TEXT("MainArena assignment subscription failed after bounded recovery."));return;}
        if(auto* CurrentWorld=Self->GetWorld())CurrentWorld->GetTimerManager().SetTimer(Self->AssignmentRetryTimer,Self,&ThisClass::TryLoadAssignment,static_cast<float>(1<<(Self->AssignmentFailures-1)),false);
    });
}
