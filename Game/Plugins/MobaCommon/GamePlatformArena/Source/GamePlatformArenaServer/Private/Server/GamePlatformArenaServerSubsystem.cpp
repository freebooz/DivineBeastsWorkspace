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
    if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(AssignmentRetryTimer);
    if(BackendClient)BackendClient->CancelAll();
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

    const TWeakObjectPtr<AGamePlatformArenaGameMode> WeakArenaMode(ArenaGameMode);
    ArenaGameMode->OnResultPending().AddWeakLambda(
        this,
        [this, WeakArenaMode](const FGamePlatformArenaMatchResult&)
        {
            if (!Coordinator.IsValid()) { return; }
            AGamePlatformArenaGameMode* Mode = WeakArenaMode.Get();
            if (Mode == nullptr) { return; }
            Coordinator->SubmitPendingResult(
                Mode,
                [](bool bCommitted, const FString& Error)
                {
                    if (bCommitted)
                    {
                        UE_LOG(LogTemp, Log, TEXT("MainArena MatchResult committed; backend owns per-player safe return and final instance release."));
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("MainArena MatchResult remains pending: %s"), *Error);
                    }
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
