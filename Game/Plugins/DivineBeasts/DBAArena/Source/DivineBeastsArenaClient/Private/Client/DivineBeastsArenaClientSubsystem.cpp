#include "Client/DivineBeastsArenaClientSubsystem.h"

#include "Catalog/DivineBeastsArenaModeCatalog.h"
#include "DivineBeastsApplicationFlowSubsystem.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"
#include "Client/DivineBeastsArenaOnlineMatchmakingAdapter.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Party/GamePlatformPartyClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Flow/DivineBeastsFlowNodes.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Framework/GamePlatformArenaGameState.h"
#include "Framework/GamePlatformArenaPlayerState.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"

namespace
{
    const FName ArenaFlowExtensionId(TEXT("DivineBeasts.Arena.ApplicationFlow"));

    class FDivineBeastsArenaApplicationFlowExtension final
        : public IDivineBeastsApplicationFlowExtension
    {
    public:
        explicit FDivineBeastsArenaApplicationFlowExtension(
            TWeakObjectPtr<UDivineBeastsArenaClientSubsystem> InOwner)
            : Owner(InOwner)
        {
        }

        virtual void OnEnteredInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            if (Owner.IsValid())
            {
                Owner->SetApplicationFlowInWorld(true);
            }
        }

        virtual void OnLeavingInWorld(
            UDivineBeastsApplicationFlowSubsystem&) override
        {
            if (Owner.IsValid())
            {
                Owner->SetApplicationFlowInWorld(false);
            }
        }

    private:
        TWeakObjectPtr<UDivineBeastsArenaClientSubsystem> Owner;
    public:
        virtual bool TryObserveWorldReadiness(UWorld& World,bool& bBound,bool& bPrepared,bool& bActive)const override
        {return Owner.IsValid()&&Owner->TryObserveArenaReadiness(World,bBound,bPrepared,bActive);}
    };
}

void UDivineBeastsArenaClientSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bDeinitializing=false;
    ReadinessMapHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddWeakLambda(this,[this](UWorld* World){if(World&&World->GetGameInstance()==GetGameInstance())RefreshArenaReadiness();});
    ReadinessCleanupHandle=FWorldDelegates::OnWorldCleanup.AddWeakLambda(this,[this](UWorld* World,bool,bool){if(ReadinessWorld.Get()==World)ClearArenaReadiness();});
    Collection.InitializeDependency<UGamePlatformOnlineClientSubsystem>();
    Collection.InitializeDependency<UGamePlatformPartyClientSubsystem>();
    Collection.InitializeDependency<UDivineBeastsApplicationFlowSubsystem>();
    ApplicationFlow =
        GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UDivineBeastsApplicationFlowSubsystem>()
            : nullptr;
    if (!ApplicationFlow)
    {
        return;
    }

    FlowExtension =
        MakeShared<FDivineBeastsArenaApplicationFlowExtension>(
            TWeakObjectPtr<UDivineBeastsArenaClientSubsystem>(this));
    if (!ApplicationFlow->RegisterExtension(
            ArenaFlowExtensionId,
            FlowExtension.ToSharedRef()))
    {
        FlowExtension.Reset();
    }
    PartySource=GetGameInstance()->GetSubsystem<UGamePlatformPartyClientSubsystem>();
    if(PartySource.IsValid())PartyChangedHandle=PartySource->OnChanged().AddWeakLambda(this,[this](const FGamePlatformPartyResult&){HandleMatchmakingChanged();});
    if(auto* Online=GetGameInstance()->GetSubsystem<UGamePlatformOnlineClientSubsystem>())
    {
        OnlineMatchmaking=MakeShared<FDivineBeastsArenaOnlineMatchmakingAdapter>(*Online,*this);
        MatchmakingChangedHandle=OnlineMatchmaking->Changed.AddUObject(this,&ThisClass::HandleMatchmakingChanged);
        OnlineMatchmaking->Start();
    }

}

void UDivineBeastsArenaClientSubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(ReadinessMapHandle);FWorldDelegates::OnWorldCleanup.Remove(ReadinessCleanupHandle);ClearArenaReadiness();
    if(PartySource.IsValid())PartySource->OnChanged().Remove(PartyChangedHandle);PartySource.Reset();PartyChangedHandle.Reset();
    bDeinitializing=true;++MigrationGeneration;bMigrationPending=false;MigrationRequestId.Empty();MigrationCandidateId.Empty();MigrationDirection.Empty();
    if(OnlineMatchmaking){OnlineMatchmaking->Changed.Remove(MatchmakingChangedHandle);OnlineMatchmaking->Stop();OnlineMatchmaking.Reset();}
    MatchmakingChangedHandle.Reset();OnMatchmakingStateChanged.Clear();
    if (ApplicationFlow && FlowExtension)
    {
        ApplicationFlow->UnregisterExtension(ArenaFlowExtensionId);
    }
    FlowExtension.Reset();
    ApplicationFlow = nullptr;
    bApplicationFlowInWorld = false;
    Super::Deinitialize();
}

TArray<FName> UDivineBeastsArenaClientSubsystem::GetProjectArenaModeIds() const
{
    TArray<FName> Result;
    for (const FDivineBeastsArenaProjectModeSpec& Spec :
         FDivineBeastsArenaModeCatalog::GetAll())
    {
        Result.Add(Spec.ArenaModeId);
    }
    return Result;
}

bool UDivineBeastsArenaClientSubsystem::BuildMatchmakingRequest(
    FName ArenaModeId,
    const FString& PartyId,
    const FString& PreferredRegion,
    const FString& ClientRequestId,
    FGamePlatformArenaMatchmakingRequest& OutRequest,
    FString& OutError) const
{
    const FDivineBeastsArenaProjectModeSpec* Spec =
        FDivineBeastsArenaModeCatalog::Find(ArenaModeId);
    if (!Spec || !Spec->ValidateRuntime(FDivineBeastsArenaModeCatalog::IsDevelopmentEnabledForCurrentProcess(),OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("ArenaMode未配置为ProductionReady。");
        }
        return false;
    }

    if(!CanQueueMode(ArenaModeId,OutError))return false;
    OutRequest = FGamePlatformArenaMatchmakingRequest{};
    OutRequest.ArenaModeId = ArenaModeId;
    OutRequest.PartyId = PartySource.IsValid()&&PartySource->GetSnapshot().Snapshot.bHasParty?PartySource->GetSnapshot().Snapshot.PartyId:FString();
    OutRequest.PreferredRegion = PreferredRegion;
    OutRequest.ClientRequestId = ClientRequestId;
    return OutRequest.IsValid(OutError);
}

bool UDivineBeastsArenaClientSubsystem::CanQueueMode(FName ModeId,FString& Reason)const
{
    const auto* Mode=FDivineBeastsArenaModeCatalog::Find(ModeId);
    if(!Mode||!Mode->ValidateRuntime(FDivineBeastsArenaModeCatalog::IsDevelopmentEnabledForCurrentProcess(),Reason))return false;
    if(!bApplicationFlowInWorld||!OnlineMatchmaking||!OnlineMatchmaking->GetSnapshot().bKnown||!OnlineMatchmaking->GetSnapshot().bServiceReady||!PartySource.IsValid()||!PartySource->GetSnapshot().bSuccess||PartySource->GetAuthenticationGeneration()!=OnlineMatchmaking->GetAuthenticationGeneration())
    {Reason=TEXT("正在同步当前世界、队伍与匹配服务");return false;}
    const auto& Party=PartySource->GetSnapshot().Snapshot;
    if(Party.bHasParty)
    {
        auto* Online=GetGameInstance()->GetSubsystem<UGamePlatformOnlineClientSubsystem>();
        if(!Online||Party.LeaderPlayerId!=Online->GetAuthentication().PlayerId){Reason=TEXT("请由队长选择模式并开始准备");return false;}
        if(Party.Members.Num()>Mode->TeamSize){Reason=TEXT("此模式容纳不了完整小队，请选择人数更大的模式");return false;}
        if(Party.bRosterLocked){Reason=TEXT("当前小队已报名，请先取消当前匹配");return false;}
    }
    Reason.Empty();return true;
}
bool UDivineBeastsArenaClientSubsystem::RequestPostMatchReturnToWorld()
{return CanReturnObservedMatch()&&RequestObservedMigration(TEXT("return"));}

TSharedPtr<IGamePlatformArenaMatchmakingAdapter> UDivineBeastsArenaClientSubsystem::GetMatchmakingAdapter()const{return OnlineMatchmaking;}
bool UDivineBeastsArenaClientSubsystem::CanCancelObservedQueue()const{return OnlineMatchmaking&&OnlineMatchmaking->GetSnapshot().bKnown&&OnlineMatchmaking->GetSnapshot().bCanCancel;}
bool UDivineBeastsArenaClientSubsystem::CanReturnObservedMatch()const{return !bDeinitializing&&!bMigrationPending&&bApplicationFlowInWorld&&OnlineMatchmaking&&OnlineMatchmaking->GetSnapshot().bKnown&&OnlineMatchmaking->GetSnapshot().Phase==TEXT("returning");}
bool UDivineBeastsArenaClientSubsystem::CanRetryObservedMigration()const
{return !bDeinitializing&&!bMigrationPending&&bApplicationFlowInWorld&&OnlineMatchmaking&&(!OnlineMatchmaking->GetSnapshot().bKnown||!MigrationError.IsEmpty());}
bool UDivineBeastsArenaClientSubsystem::RetryObservedMigration()
{
    if(!CanRetryObservedMigration())return false;
    if(!OnlineMatchmaking->GetSnapshot().bKnown){OnlineMatchmaking->Resynchronize();return true;}
    const auto Phase=OnlineMatchmaking->GetSnapshot().Phase;
    if(Phase==TEXT("returning"))return RequestObservedMigration(TEXT("return"));
    if(Phase==TEXT("transferReady")||Phase==TEXT("inMatch"))return RequestObservedMigration(TEXT("arena"));
    MigrationError=FText::GetEmpty();OnlineMatchmaking->Resynchronize();return true;
}
void UDivineBeastsArenaClientSubsystem::SetApplicationFlowInWorld(bool bInWorld)
{bApplicationFlowInWorld=bInWorld;if(bInWorld)HandleMatchmakingChanged();}
FText UDivineBeastsArenaClientSubsystem::GetObservedMatchmakingStatus()const
{
    if(!MigrationError.IsEmpty())return MigrationError;
    if(!OnlineMatchmaking||!OnlineMatchmaking->GetSnapshot().bKnown)return FText::FromString(TEXT("正在同步匹配状态"));
    const auto& View=OnlineMatchmaking->GetSnapshot();
    if(View.Phase==TEXT("challengeInvitation"))return FText::FromString(TEXT("约战邀请待回复；接受后全员分别确认"));
    if(View.Phase==TEXT("readyCheck"))return FText::FromString(TEXT("等待全部队员准备；名单保持完整"));
    if(View.Phase==TEXT("searching"))return FText::FromString(TEXT("正在匹配；小队不会被拆分"));
    if(View.Phase==TEXT("confirmation"))return FText::FromString(FString::Printf(TEXT("已找到对局 · 已确认 %d / %d"),View.Accepted,View.Participants));
    if(View.Phase==TEXT("allocating"))return FText::FromString(TEXT("全员已确认，正在准备竞技场"));
    if(View.Phase==TEXT("transferReady"))return FText::FromString(TEXT("竞技场已就绪，正在连接"));
    if(View.Phase==TEXT("inMatch"))return FText::FromString(TEXT("对局进行中"));
    if(View.Phase==TEXT("returning"))return FText::FromString(TEXT("对局已结束，可以安全返回"));
    if(View.Phase==TEXT("cancelled")||View.Phase==TEXT("failed"))return FText::FromString(TEXT("本次匹配已结束，可以重新报名"));
    return FText::FromString(View.bServiceReady?TEXT("请选择模式；单人可直接匹配"):TEXT("当前竞技场不可用，请稍后重试"));
}
FName UDivineBeastsArenaClientSubsystem::GetObservedMatchmakingMode() const
{
    if(!OnlineMatchmaking)return NAME_None;
    const auto& View=OnlineMatchmaking->GetSnapshot();
    if(View.Phase==TEXT("idle")||View.Phase==TEXT("completed")||View.Phase==TEXT("cancelled")||View.Phase==TEXT("failed"))return NAME_None;
    return FName(*View.ModeId);
}
FDivineBeastsArenaChallengeView UDivineBeastsArenaClientSubsystem::GetObservedChallenge() const
{
    return OnlineMatchmaking&&OnlineMatchmaking->GetSnapshot().bKnown?OnlineMatchmaking->GetSnapshot().Challenge:FDivineBeastsArenaChallengeView{};
}
bool UDivineBeastsArenaClientSubsystem::CanChallengeMode(FName ModeId,FString& Reason) const
{
    if(!CanQueueMode(ModeId,Reason))return false;
    const auto* Mode=FDivineBeastsArenaModeCatalog::Find(ModeId);
    const auto& Party=PartySource->GetSnapshot().Snapshot;
    const int32 Count=Party.bHasParty?Party.Members.Num():1;
    if(!Mode||Count!=Mode->TeamSize){Reason=TEXT("约战需要双方各有完整人数；请先组满本方小队");return false;}
    const auto State=OnlineMatchmaking->GetObservedState();
    if(State!=EGamePlatformArenaClientFlowState::Idle){Reason=TEXT("当前已有邀请或报名，请先等待终态或取消");return false;}
    Reason.Empty();return true;
}
void UDivineBeastsArenaClientSubsystem::RequestChallenge(FName ModeId,const FString& TargetPlayerId,const FString& Id,TFunction<void(bool,const FString&)> Done)
{
    FString Error;FGamePlatformArenaMatchmakingRequest Request;
    if(bDeinitializing||!CanChallengeMode(ModeId,Error)||!BuildMatchmakingRequest(ModeId,FString(),FString(),Id,Request,Error))
    {Done(false,TEXT("Matchmaking.ChallengeUnavailable"));return;}
    OnlineMatchmaking->RequestChallenge(Request,TargetPlayerId,MoveTemp(Done));
}
void UDivineBeastsArenaClientSubsystem::ReplyChallenge(bool bAccept,const FString& Id,TFunction<void(bool,const FString&)> Done)
{
    if(bDeinitializing||!bApplicationFlowInWorld||!OnlineMatchmaking){Done(false,TEXT("Matchmaking.ChallengeUnavailable"));return;}
    OnlineMatchmaking->ReplyChallenge(bAccept,Id,MoveTemp(Done));
}
void UDivineBeastsArenaClientSubsystem::CancelChallenge(const FString& Id,TFunction<void(bool,const FString&)> Done)
{
    if(bDeinitializing||!bApplicationFlowInWorld||!OnlineMatchmaking){Done(false,TEXT("Matchmaking.ChallengeUnavailable"));return;}
    OnlineMatchmaking->CancelChallenge(Id,MoveTemp(Done));
}
void UDivineBeastsArenaClientSubsystem::HandleMatchmakingChanged()
{
    if(bDeinitializing||!OnlineMatchmaking)return;
    const auto Auth=OnlineMatchmaking->GetAuthenticationGeneration();
    if(MigrationAuthGeneration!=Auth){++MigrationGeneration;bMigrationPending=false;MigrationAuthGeneration=Auth;MigrationRequestId.Empty();MigrationCandidateId.Empty();MigrationDirection.Empty();MigrationError=FText::GetEmpty();}
    OnMatchmakingStateChanged.Broadcast();
    if(bDeinitializing||!OnlineMatchmaking||!bApplicationFlowInWorld||!ApplicationFlow)return;
    const auto& View=OnlineMatchmaking->GetSnapshot();const auto World=ApplicationFlow->GetViewState();
    if(View.bKnown&&(View.Phase==TEXT("transferReady")||View.Phase==TEXT("inMatch"))&&World.Assignment.ServerRoleId!=FName(TEXT("GameServer.Role.MainArena"))&&!bMigrationPending&&MigrationError.IsEmpty())RequestObservedMigration(TEXT("arena"));
}
bool UDivineBeastsArenaClientSubsystem::RequestObservedMigration(const FString& Direction)
{
    if(bDeinitializing||!OnlineMatchmaking||!ApplicationFlow||!bApplicationFlowInWorld||bMigrationPending||!OnlineMatchmaking->GetSnapshot().bKnown)return false;
    const auto& View=OnlineMatchmaking->GetSnapshot();if(View.CandidateId.IsEmpty())return false;
    // 每次明确重试新建返回请求；原调用在途时禁重复，传输重放仍由同一个IdempotencyKey保护。
    if(MigrationCandidateId!=View.CandidateId||MigrationDirection!=Direction||MigrationRequestId.IsEmpty()||!MigrationError.IsEmpty())
    {MigrationCandidateId=View.CandidateId;MigrationDirection=Direction;MigrationRequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);}
    MigrationError=FText::GetEmpty();bMigrationPending=true;const uint64 Expected=++MigrationGeneration;const FGuid Auth=OnlineMatchmaking->GetAuthenticationGeneration();
    const TWeakObjectPtr<UDivineBeastsArenaClientSubsystem> Weak(this);OnMatchmakingStateChanged.Broadcast();
    if(bDeinitializing||!OnlineMatchmaking||Expected!=MigrationGeneration||OnlineMatchmaking->GetAuthenticationGeneration()!=Auth)return false;
    OnlineMatchmaking->RequestMigration(Direction,MigrationRequestId,[Weak,Expected,Auth](bool bSuccess,FDivineBeastsWorldAssignmentPayload Payload)mutable
    {
        auto* Self=Weak.Get();if(!Self||Self->bDeinitializing||Self->MigrationGeneration!=Expected||!Self->OnlineMatchmaking||Self->OnlineMatchmaking->GetAuthenticationGeneration()!=Auth)return;
        Self->bMigrationPending=false;
        if(!bSuccess||!Self->ApplicationFlow||!Self->ApplicationFlow->RequestTrustedWorldMigration(MoveTemp(Payload),Auth))Self->MigrationError=FText::FromString(TEXT("连接尚未确认，请重试；不会复用旧地址进入"));
        Self->OnMatchmakingStateChanged.Broadcast();
    });return true;
}

void UDivineBeastsArenaClientSubsystem::ClearArenaReadiness()
{
    if(ReadinessWorld.IsValid())ReadinessWorld->GameStateSetEvent.Remove(ReadinessGameStateHandle);
    if(ReadinessController.IsValid())ReadinessController->OnPossessedPawnChanged.RemoveDynamic(this,&ThisClass::HandleReadinessPawnChanged);
    if(ReadinessGameState.IsValid())ReadinessGameState->OnArenaPhaseChanged.RemoveAll(this);
    if(ReadinessCharacter.IsValid())ReadinessCharacter->OnReadinessChanged().RemoveAll(this);
    if(ReadinessEligibility.IsValid())ReadinessEligibility->OnEligibilityChanged().RemoveAll(this);
    ReadinessWorld.Reset();ReadinessController.Reset();ReadinessGameState.Reset();ReadinessCharacter.Reset();ReadinessEligibility.Reset();ReadinessGameStateHandle.Reset();
}
void UDivineBeastsArenaClientSubsystem::HandleReadinessPawnChanged(APawn*,APawn*){RefreshArenaReadiness();}
void UDivineBeastsArenaClientSubsystem::RefreshArenaReadiness()
{
    if(bDeinitializing)return;
    auto* World=GetGameInstance()?GetGameInstance()->GetWorld():nullptr;
    if(World&&World->bIsTearingDown)World=nullptr;
    if(ReadinessWorld.Get()!=World)
    {
        ClearArenaReadiness();ReadinessWorld=World;
        if(World)ReadinessGameStateHandle=World->GameStateSetEvent.AddWeakLambda(this,[this](AGameStateBase*){RefreshArenaReadiness();});
    }
    auto* Controller=World?World->GetFirstPlayerController():nullptr;
    if(ReadinessController.Get()!=Controller)
    {
        if(ReadinessController.IsValid())ReadinessController->OnPossessedPawnChanged.RemoveDynamic(this,&ThisClass::HandleReadinessPawnChanged);
        ReadinessController=Controller;
        if(Controller)Controller->OnPossessedPawnChanged.AddUniqueDynamic(this,&ThisClass::HandleReadinessPawnChanged);
    }
    auto* State=World?World->GetGameState<AGamePlatformArenaGameState>():nullptr;
    if(ReadinessGameState.Get()!=State)
    {
        if(ReadinessGameState.IsValid())ReadinessGameState->OnArenaPhaseChanged.RemoveAll(this);
        ReadinessGameState=State;
        if(State)State->OnArenaPhaseChanged.AddWeakLambda(this,[this](EGamePlatformArenaMatchPhase,int32){RefreshArenaReadiness();});
    }
    APawn* Pawn=Controller?Controller->GetPawn():nullptr;
    auto* Character=Pawn?Pawn->FindComponentByClass<UDivineBeastsCharacterComponent>():nullptr;
    if(ReadinessCharacter.Get()!=Character)
    {
        if(ReadinessCharacter.IsValid())ReadinessCharacter->OnReadinessChanged().RemoveAll(this);
        ReadinessCharacter=Character;
        if(Character)Character->OnReadinessChanged().AddWeakLambda(this,[this](bool){RefreshArenaReadiness();});
    }
    auto* Eligibility=Pawn?Pawn->FindComponentByClass<UGamePlatformGameplayEligibilityComponent>():nullptr;
    if(ReadinessEligibility.Get()!=Eligibility)
    {
        if(ReadinessEligibility.IsValid())ReadinessEligibility->OnEligibilityChanged().RemoveAll(this);
        ReadinessEligibility=Eligibility;
        if(Eligibility)Eligibility->OnEligibilityChanged().AddWeakLambda(this,[this](const FGamePlatformGameplayEligibilitySnapshot&){RefreshArenaReadiness();});
    }
    if(ApplicationFlow)ApplicationFlow->NotifyGameplayWorldFactsChanged();
}
bool UDivineBeastsArenaClientSubsystem::TryObserveArenaReadiness(UWorld& World,bool& bBound,bool& bPrepared,bool& bActive)const
{
    if(bDeinitializing||ReadinessWorld.Get()!=&World||!ReadinessGameState.IsValid()||!ReadinessController.IsValid()||!ReadinessController->IsLocalController())return false;
    const auto* Controller=ReadinessController.Get();const APawn* Pawn=Controller->GetPawn();
    const auto* Player=Controller->GetPlayerState<AGamePlatformArenaPlayerState>();
    if(!Player||Player->ConnectionState!=EGamePlatformArenaConnectionState::Connected)return true;
    // 淘汰者恢复的是观战连接，不能为了满足加载屏障重新生成角色或补满资源。
    const bool bSpectating=Player->bEliminated&&!Player->CharacterId.IsEmpty();
    bBound=bSpectating||(Pawn&&Pawn->GetWorld()==&World&&Pawn->GetController()==Controller&&Pawn->InputComponent);
    bPrepared=bSpectating||(ReadinessCharacter.IsValid()&&ReadinessCharacter->IsCharacterReady());
    bActive=bBound&&bPrepared&&(bSpectating||(ReadinessEligibility.IsValid()&&ReadinessEligibility->IsServerPlayerActiveForGameplay()));
    return true;
}
