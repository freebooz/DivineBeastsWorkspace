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
{ return Policy::AllowsUpdates(bClosing,bDispatching,Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed,
    GetWorld()&&!GetWorld()->bIsTearingDown); }
void UGamePlatformWorldSubsystem::Fail(FName Code,const FString& Message)
{
    if(bClosing||Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed)return;
    Snapshot.Context.ReadinessState=EGamePlatformWorldReadiness::Failed;
    Snapshot.Context.Result=FGamePlatformResult::Failure(Code,Message);
    // 先拒绝后续回调，再在安全采样点统一清理；不在贡献者迭代或Data完成栈重入释放。
    bFailureCleanupPending=true;
}
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
FGamePlatformResult UGamePlatformWorldSubsystem::InitializeBoundWorld(const FPrimaryAssetId& Id, const FGamePlatformWorldContext& Verified)
{
    check(IsInGameThread());
    if (!CanMutate() || bStarted) return WorldError(TEXT("WorldBusyOrClosing"));
    // 只接受已绑定实例身份。旧世界GUID由本服务创建，不采用外部提交的就绪布尔或GUID。
    if (!Id.IsValid() || !Verified.WorldId.IsValid() || !Verified.ExperienceId.IsValid()
        || Verified.ServerInstanceId.IsEmpty() || Verified.ServerStartGeneration == 0
        || Verified.AuthorityKind != EGamePlatformWorldAuthority::SessionProjection)
        return WorldError(TEXT("BoundWorldIdentityInvalid"));
    auto* GI = GetWorld()->GetGameInstance();
    auto* Data = GI ? IGamePlatformDataService::Get(*GI) : nullptr;
    if (!Data) return WorldError(TEXT("DataUnavailable"));
    const FGuid Generation = Snapshot.Context.ContextGeneration;
    Snapshot.Context = Verified; Snapshot.Context.ContextGeneration = Generation;
    Snapshot.Context.ReadinessState = EGamePlatformWorldReadiness::Waiting;
    Snapshot.Context.Result = {}; Snapshot.bSessionContextMatched = true;
    bStarted = true; DeadlineSeconds = FPlatformTime::Seconds() + 60;
    FGamePlatformResult Accepted;
    DefinitionLease = Data->AcquireDefinition(Id, UGamePlatformWorldDefinition::StaticClass(), {}, EGamePlatformDataLifetime::World, this,
        [Weak=TWeakObjectPtr<UGamePlatformWorldSubsystem>(this),Generation](const auto& Lease,const auto& Result)
        {
            auto* Self=Weak.Get(); if (!Self || !Self->CanMutate() || Self->Snapshot.Context.ContextGeneration!=Generation) return;
            if (!Result.IsSuccess()) { Self->Fail(Result.Code,Result.Message); return; }
            auto* Service=IGamePlatformDataService::Get(*Self->GetWorld()->GetGameInstance());
            const auto* Definition=Service?Cast<UGamePlatformWorldDefinition>(Service->GetLoadedDefinition(Lease)):nullptr;
            if (!Definition || Definition->LogicalId!=Self->Snapshot.Context.WorldId || Definition->DefaultExperienceId!=Self->Snapshot.Context.ExperienceId)
            { Self->Fail(TEXT("BoundWorldDefinitionMismatch"),TEXT("权威实例身份与实际世界定义不一致")); return; }
            Self->DeadlineSeconds=FPlatformTime::Seconds()+Definition->ReadinessTimeoutSeconds;
            Self->LoadRegions(Definition->Regions); Self->Refresh();
        },Accepted);
    if (!Accepted.IsSuccess()) Fail(Accepted.Code,Accepted.Message);
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
                Self->Refresh(); // Data真实终态更新区域定义事实；既有采样轮发布变化。
            },Accepted);
        RegionLeases.Add(Id,Lease);
        if(!Accepted.IsSuccess()){Fail(Accepted.Code,Accepted.Message);return;}
    }
}
void UGamePlatformWorldSubsystem::Refresh()
{
    check(IsInGameThread());
    if(bClosing||bDispatching)return;
    if(Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed)
    {if(bFailureCleanupPending)ReleaseOwnedResources();return;}
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
    Snapshot.bRegionDefinitionsLoaded=Definition&&RegionIdentities.Num()==Definition->Regions.Num();
    Snapshot.RegisteredRegionProviders=0;for(const auto& R:Regions)if(Owns(R.Value.Provider))++Snapshot.RegisteredRegionProviders;
    Snapshot.bRequiredRegionsRegistered=Snapshot.bRegionDefinitionsLoaded;
    for(const auto& Pair:RegionIdentities)
    {
        const bool Found=Regions.ContainsByPredicate([&](const auto& R){return R.Value.RegionId==Pair.Value&&Owns(R.Value.Provider);});
        Snapshot.bRequiredRegionsRegistered&=Found;
        if(!Data||!Data->GetLoadedDefinition(RegionLeases.FindChecked(Pair.Key))){Snapshot.bDefinitionLoaded=false;Snapshot.bRegionDefinitionsLoaded=false;}
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
    if(!bStarted||Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed)
    {if(bFailureCleanupPending)ReleaseOwnedResources();return;}
    const bool Ready=Policy::IsReady({Snapshot.bWorldObjectValid,Snapshot.bDefinitionLoaded,Snapshot.bMapIdentityMatched,
        Snapshot.bSessionContextMatched,Snapshot.bRequiredRegionsRegistered,Snapshot.bRequiredStreamingReady,
        Snapshot.bWorldNotTearingDown,Snapshot.bContributorsReady});
    const double Now=FPlatformTime::Seconds();
    DeadlineSeconds=Policy::NextDeadline(Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Ready,Ready,Now,
        Definition?Definition->ReadinessTimeoutSeconds:60,DeadlineSeconds);
    if(!Ready&&Now>=DeadlineSeconds){Fail(TEXT("WorldReadinessTimeout"),TEXT("世界必需事实未在截止时间内就绪"));ReleaseOwnedResources();return;}
    Snapshot.Context.ReadinessState=Ready?EGamePlatformWorldReadiness::Ready:EGamePlatformWorldReadiness::Waiting;
    Snapshot.Context.Result=Ready?FGamePlatformResult::Success():FGamePlatformResult{};
}
FGamePlatformWorldReadinessSnapshot UGamePlatformWorldSubsystem::GetReadiness(){check(IsInGameThread());Refresh();return Snapshot;}
bool UGamePlatformWorldSubsystem::Tick(float)
{
    if(bClosing)return false;
    if(!GetWorld()||GetWorld()->bIsTearingDown){Stop();return false;}
    // Refresh负责唯一一次流送状态采样；旧实现此处重复Tick，造成每周期两次状态遍历。
    UpdateRegions();Refresh();PublishReadiness();
    // 更新过程中可能因世界失效而同步Stop；不可继续分发旧世界事件。
    if(bClosing)return false;
    // 大多数周期没有区域事件，不必每0.1秒复制所有订阅者。
    if(PendingEvents.IsEmpty())return true;
    auto Events=MoveTemp(PendingEvents);PendingEvents.Reset();auto List=Subscriptions;
    TGuardValue<bool> Guard(bDispatching,true);
    for(const auto& Event:Events)
    {
        if(bClosing)break;
        if(!Event.Observer.IsValid())continue;
        for(const auto& Pair:List)
        {
            if(bClosing)break;
            if(Owns(Pair.Value->Owner))Pair.Value->Callback(Event);
        }
    }
    // 回调允许关闭世界，但关闭后不得继续通知后续订阅者或保持Ticker运行。
    return !bClosing;
}
void UGamePlatformWorldSubsystem::Stop()
{
    if(bClosing)return;
    bClosing=true; // 先失效，再撤销可能同步完成的底层资源；不允许旧回调复活状态。
    Snapshot.Context.ReadinessState=EGamePlatformWorldReadiness::Invalidated;
    Snapshot.Context.Result=FGamePlatformResult::Cancelled(TEXT("世界生命周期结束"));
    Snapshot.bWorldNotTearingDown=false;Snapshot.bWorldObjectValid=false;
    if(TickerHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);TickerHandle.Reset();}
    ReleaseOwnedResources();
    PublishReadiness();
    // 普通通知栈内退出时，外层PublishReadiness负责有界补发终态后清表。
    if(!bPublishingReadiness)ReadinessSubscriptions.Reset();
}
void UGamePlatformWorldSubsystem::ReleaseOwnedResources()
{
    bFailureCleanupPending=false;
    if(Streaming){Streaming->Shutdown();Streaming.Reset();}
    Subscriptions.Reset();Contributors.Reset();PendingEvents.Reset();Regions.Reset();Observers.Reset();
    auto* World=GetWorld();auto* GI=World?World->GetGameInstance():nullptr;auto* Data=GI?IGamePlatformDataService::Get(*GI):nullptr;
    if(Data){for(const auto& Pair:RegionLeases)Data->ReleaseDefinition(Pair.Value);if(DefinitionLease.IsValid())Data->ReleaseDefinition(DefinitionLease);}
    RegionLeases.Reset();RegionIdentities.Reset();DefinitionLease={};
    Snapshot.bDefinitionLoaded=false;Snapshot.bRegionDefinitionsLoaded=false;Snapshot.RegisteredRegionProviders=0;Snapshot.bRequiredRegionsRegistered=false;Snapshot.bRequiredStreamingReady=false;
}
void UGamePlatformWorldSubsystem::OnWorldEndPlay(UWorld& World){Stop();Super::OnWorldEndPlay(World);}
void UGamePlatformWorldSubsystem::Deinitialize(){Stop();Super::Deinitialize();}
