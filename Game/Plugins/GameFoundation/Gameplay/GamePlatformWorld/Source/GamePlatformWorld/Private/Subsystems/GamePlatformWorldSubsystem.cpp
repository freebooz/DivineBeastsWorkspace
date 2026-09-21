#include "Subsystems/GamePlatformWorldSubsystem.h"
#include "Context/WorldPolicy.h"
#include "Streaming/GamePlatformWorldStreaming.h"
#include "Definitions/GamePlatformWorldDefinition.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace Policy = GamePlatformWorldPolicy;
static FGamePlatformResult WorldError(FName Code)
{ return FGamePlatformResult::Failure(Code,TEXT("世界操作未满足身份、生命周期或就绪前提；请按错误码核查")); }
UGamePlatformWorldSubsystem::UGamePlatformWorldSubsystem() = default;
UGamePlatformWorldSubsystem::UGamePlatformWorldSubsystem(FVTableHelper& Helper):Super(Helper){}
UGamePlatformWorldSubsystem::~UGamePlatformWorldSubsystem() = default;
IGamePlatformWorldService* IGamePlatformWorldService::Get(UWorld& World)
{ check(IsInGameThread());return World.GetSubsystem<UGamePlatformWorldSubsystem>(); }
bool UGamePlatformWorldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{ return Policy::SupportsWorld(Type==EWorldType::Game,Type==EWorldType::PIE,IsRunningCommandlet()); }
void UGamePlatformWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Snapshot.Context.ContextGeneration=FGuid::NewGuid();
    Streaming=MakeUnique<FGamePlatformWorldStreaming>(*GetWorld(),Snapshot.Context.ContextGeneration);
    TickerHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Tick),0.1f);
}
bool UGamePlatformWorldSubsystem::Owns(TWeakObjectPtr<UObject> Object) const
{ return Object.IsValid() && Object->GetWorld()==GetWorld(); }
bool UGamePlatformWorldSubsystem::CanMutate() const
{ return !bClosing&&!bDispatching&&GetWorld()&&!GetWorld()->bIsTearingDown; }
void UGamePlatformWorldSubsystem::Fail(FName Code,const FString& Message)
{Snapshot.Context.ReadinessState=EGamePlatformWorldReadiness::Failed;Snapshot.Context.Result=FGamePlatformResult::Failure(Code,Message);}
FGamePlatformResult UGamePlatformWorldSubsystem::InitializeSessionWorld()
{
    check(IsInGameThread());
    // 没有真实公开Session服务时不暴露可由客户端注入的分配结构，不制造可信标志。
    return FGamePlatformResult::Unsupported(TEXT("SessionPrerequisiteMissing"),TEXT("Session尚无真实公开目标快照，不能绑定网络世界上下文"));
}
FGamePlatformResult UGamePlatformWorldSubsystem::InitializeDevelopment(const FPrimaryAssetId& Id,const FGamePlatformVersion& Version,FName Role)
{
    check(IsInGameThread());
    if(!CanMutate()||bStarted)return WorldError(TEXT("WorldBusyOrClosing"));
    UWorld* World=GetWorld();
    if(!Policy::AllowLocalDevelopment(FParse::Param(FCommandLine::Get(),TEXT("FoundationWorld")),UE_BUILD_SHIPPING,
        World->GetNetMode()==NM_Client,World->GetNetMode()==NM_ListenServer))return WorldError(TEXT("DevelopmentNotAuthorized"));
    if(!Id.IsValid()||!Version.IsValid())return WorldError(TEXT("InvalidDefinitionOrBuildVersion"));
    if(World->GetNetMode()==NM_DedicatedServer && Role!=TEXT("OpenWorld")&&Role!=TEXT("Village")&&Role!=TEXT("MainArena"))
        return WorldError(TEXT("DevelopmentServerRoleMissing"));
    auto* Instance=World->GetGameInstance();
    auto* Data=Instance?IGamePlatformDataService::Get(*Instance):nullptr;
    if(!Data)return WorldError(TEXT("DataUnavailable"));
    bStarted=true;
    Snapshot.Context.AuthorityKind=World->GetNetMode()==NM_DedicatedServer?EGamePlatformWorldAuthority::DevelopmentServer:EGamePlatformWorldAuthority::DevelopmentLocal;
    Snapshot.Context.BuildVersion=Version;Snapshot.Context.ServerRole=World->GetNetMode()==NM_DedicatedServer?Role:NAME_None;
    Snapshot.Context.ReadinessState=EGamePlatformWorldReadiness::Waiting;
    Snapshot.bSessionContextMatched=true;DeadlineSeconds=FPlatformTime::Seconds()+60;
    const FGuid Expected=Snapshot.Context.ContextGeneration;
    FGamePlatformResult Accepted;
    DefinitionLease=Data->AcquireDefinition(Id,UGamePlatformWorldDefinition::StaticClass(),{},EGamePlatformDataLifetime::World,this,
        [Weak=TWeakObjectPtr<UGamePlatformWorldSubsystem>(this),Expected](const auto& Lease,const auto& Result)
        {
            auto* Self=Weak.Get();
            if(!Self||!Self->CanMutate()||Expected!=Self->Snapshot.Context.ContextGeneration)return;
            if(!Result.IsSuccess()){Self->Fail(Result.Code,Result.Message);return;}
            auto* GI=Self->GetWorld()->GetGameInstance();auto* Service=GI?IGamePlatformDataService::Get(*GI):nullptr;
            const auto* Definition=Service?Cast<UGamePlatformWorldDefinition>(Service->GetLoadedDefinition(Lease)):nullptr;
            if(!Definition){Self->Fail(TEXT("WorldDefinitionMissing"),TEXT("完成回调没有可读世界定义"));return;}
            Self->Snapshot.Context.WorldId=Definition->LogicalId;
            Self->Snapshot.Context.ExperienceId=Definition->DefaultExperienceId;
            Self->DeadlineSeconds=FPlatformTime::Seconds()+Definition->ReadinessTimeoutSeconds;
            Self->LoadRegions(Definition->Regions);
            Self->Refresh();
        },Accepted);
    if(!Accepted.IsSuccess())Fail(Accepted.Code,Accepted.Message);
    return Accepted;
}
void UGamePlatformWorldSubsystem::LoadRegions(const TArray<FPrimaryAssetId>& Ids)
{
    auto* GI=GetWorld()->GetGameInstance();auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr;
    if(!Data){Fail(TEXT("DataUnavailable"),TEXT("区域定义加载前数据服务失效"));return;}
    const FGuid Expected=Snapshot.Context.ContextGeneration;
    for(const auto& Id:Ids)
    {
        FGamePlatformResult Accepted;
        auto Lease=Data->AcquireDefinition(Id,UGamePlatformRegionDefinition::StaticClass(),{},EGamePlatformDataLifetime::World,this,
            [Weak=TWeakObjectPtr<UGamePlatformWorldSubsystem>(this),Expected,Id](const auto& RegionLease,const auto& Result)
            {
                auto* Self=Weak.Get();if(!Self||!Self->CanMutate()||Self->Snapshot.Context.ContextGeneration!=Expected)return;
                if(!Result.IsSuccess()){Self->Fail(Result.Code,Result.Message);return;}
                auto* GI=Self->GetWorld()->GetGameInstance();auto* Service=GI?IGamePlatformDataService::Get(*GI):nullptr;
                const auto* Definition=Service?Cast<UGamePlatformRegionDefinition>(Service->GetLoadedDefinition(RegionLease)):nullptr;
                if(!Definition){Self->Fail(TEXT("RegionDefinitionMissing"),TEXT("区域租约没有可读定义"));return;}
                Self->RegionIdentities.Add(Id,Definition->LogicalId);
            },Accepted);
        RegionLeases.Add(Id,Lease);
        if(!Accepted.IsSuccess()){Fail(Accepted.Code,Accepted.Message);return;}
    }
}
void UGamePlatformWorldSubsystem::Refresh()
{
    check(IsInGameThread());
    if(bClosing||bDispatching)return;
    UWorld* World=GetWorld();
    Snapshot.bWorldObjectValid=IsValid(World)&&World->HasBegunPlay()&&DoesSupportWorldType(World->WorldType);
    Snapshot.bWorldNotTearingDown=IsValid(World)&&!World->bIsTearingDown;
    if(!Snapshot.bWorldNotTearingDown){Stop();return;}
    Snapshot.Context.MapPackageName=UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
    auto* GI=World->GetGameInstance();auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr;
    const auto* Definition=Data?Cast<UGamePlatformWorldDefinition>(Data->GetLoadedDefinition(DefinitionLease)):nullptr;
    Snapshot.bDefinitionLoaded=Definition!=nullptr;
    Snapshot.bMapIdentityMatched=Definition&&Definition->MapIdentity.ToSoftObjectPath().GetLongPackageName()==Snapshot.Context.MapPackageName;
    if(Definition&&!Snapshot.bMapIdentityMatched)Fail(TEXT("MapIdentityMismatch"),TEXT("实际地图与世界定义MapIdentity不同"));
    Snapshot.bRequiredRegionsRegistered=Definition&&RegionIdentities.Num()==Definition->Regions.Num();
    for(const auto& Pair:RegionIdentities)
    {
        const bool Found=Regions.ContainsByPredicate([&](const auto& R){return R.Value.RegionId==Pair.Value&&Owns(R.Value.Provider);});
        Snapshot.bRequiredRegionsRegistered&=Found;
        if(!Data||!Data->GetLoadedDefinition(RegionLeases.FindChecked(Pair.Key)))Snapshot.bDefinitionLoaded=false;
    }
    // GetReadiness也是公开持续屏障，不能仅依赖上一轮0.1秒采样留下的流送成功。
    if(Streaming)Streaming->Tick();
    Snapshot.bRequiredStreamingReady=Streaming&&Streaming->IsRequiredReady();
    if(Streaming&&Streaming->HasRequiredFailure())Fail(TEXT("RequiredStreamingFailed"),TEXT("必需流送失败或取消"));
    Snapshot.bContributorsReady=true;
    {
        TGuardValue<bool> Guard(bDispatching,true);
        for(auto It=Contributors.CreateIterator();It;++It)
        {
            if(!Owns(It.Value().Owner)){Snapshot.bContributorsReady=false;Fail(TEXT("ContributorOwnerExpired"),TEXT("必需贡献者已销毁"));It.RemoveCurrent();continue;}
            const auto Result=It.Value().Contributor->Evaluate(Snapshot.Context);
            if(!Result.IsSuccess())Snapshot.bContributorsReady=false;
            if(Result.Status!=EGamePlatformResultStatus::NotExecuted&&!Result.IsSuccess())Fail(Result.Code,Result.Message);
        }
    }
    if(!bStarted||Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed)return;
    const bool Ready=Policy::IsReady({Snapshot.bWorldObjectValid,Snapshot.bDefinitionLoaded,Snapshot.bMapIdentityMatched,
        Snapshot.bSessionContextMatched,Snapshot.bRequiredRegionsRegistered,Snapshot.bRequiredStreamingReady,
        Snapshot.bWorldNotTearingDown,Snapshot.bContributorsReady});
    if(!Ready&&FPlatformTime::Seconds()>=DeadlineSeconds){Fail(TEXT("WorldReadinessTimeout"),TEXT("世界必需事实未在截止时间内就绪"));return;}
    Snapshot.Context.ReadinessState=Ready?EGamePlatformWorldReadiness::Ready:EGamePlatformWorldReadiness::Waiting;
    Snapshot.Context.Result=Ready?FGamePlatformResult::Success():FGamePlatformResult{};
}
FGamePlatformWorldReadinessSnapshot UGamePlatformWorldSubsystem::GetReadiness(){check(IsInGameThread());Refresh();return Snapshot;}
bool UGamePlatformWorldSubsystem::Tick(float)
{
    if(bClosing)return false;
    if(!GetWorld()||GetWorld()->bIsTearingDown){Stop();return false;}
    if(Streaming)Streaming->Tick();
    UpdateRegions();Refresh();
    // 复制待发布事件及订阅，发布期间拒绝修改，避免用户回调导致容器迭代失效。
    auto Events=MoveTemp(PendingEvents);PendingEvents.Reset();auto List=Subscriptions;
    TGuardValue<bool> Guard(bDispatching,true);
    for(const auto& Event:Events)for(const auto& Pair:List)
        if(Owns(Pair.Value->Owner)&&Event.Observer.IsValid())Pair.Value->Callback(Event);
    return true;
}
void UGamePlatformWorldSubsystem::Stop()
{
    if(bClosing)return;
    bClosing=true; // 先失效，再撤销可能同步完成的底层资源；不允许旧回调复活状态。
    Snapshot.Context.ReadinessState=EGamePlatformWorldReadiness::Invalidated;
    Snapshot.Context.Result=FGamePlatformResult::Cancelled(TEXT("世界生命周期结束"));
    Snapshot.bWorldNotTearingDown=false;Snapshot.bWorldObjectValid=false;
    if(TickerHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);TickerHandle.Reset();}
    if(Streaming){Streaming->Shutdown();Streaming.Reset();}
    Subscriptions.Reset();Contributors.Reset();PendingEvents.Reset();Regions.Reset();Observers.Reset();
    auto* World=GetWorld();auto* GI=World?World->GetGameInstance():nullptr;auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr;
    if(Data){for(const auto& Pair:RegionLeases)Data->ReleaseDefinition(Pair.Value);if(DefinitionLease.IsValid())Data->ReleaseDefinition(DefinitionLease);}
    RegionLeases.Reset();RegionIdentities.Reset();DefinitionLease={};
}
void UGamePlatformWorldSubsystem::OnWorldEndPlay(UWorld& World){Stop();Super::OnWorldEndPlay(World);}
void UGamePlatformWorldSubsystem::Deinitialize(){Stop();Super::Deinitialize();}
