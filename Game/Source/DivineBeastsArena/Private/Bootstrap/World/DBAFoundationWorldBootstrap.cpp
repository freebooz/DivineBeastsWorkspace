#include "Bootstrap/World/DBAFoundationWorldBootstrap.h"
#include "Definitions/GamePlatformWorldDefinition.h"
#include "Definitions/GamePlatformRegionDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Services/GamePlatformWorldServices.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAWorld, Log, All);
namespace
{
    constexpr const TCHAR* SandboxPackage = TEXT("/Game/Development/Foundation/Maps/L_FoundationSandbox");
    constexpr const TCHAR* BootstrapPackage = TEXT("/Game/Development/Foundation/Maps/L_FoundationBootstrap");
    // 项目夹具单位厘米，与地图地面范围一致；分离的盒避免边界点等优先级歧义。
    // 按World资产Regions顺序绑定，身份始终从真实Region定义LogicalId取得。
    FBox FixtureBounds(int32 Index)
    {
        return Index == 0 ? FBox(FVector(-1900,-1900,-200),FVector(-10,1900,1000))
                          : FBox(FVector(10,-1900,-200),FVector(1900,1900,1000));
    }
}

bool UDBAFoundationWorldBootstrap::ShouldCreateSubsystem(UObject* Outer) const
{
    return !UE_BUILD_SHIPPING && !IsRunningCommandlet() &&
        FParse::Param(FCommandLine::Get(),TEXT("FoundationWorld")) && Super::ShouldCreateSubsystem(Outer);
}
void UDBAFoundationWorldBootstrap::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    FString RunText, DefinitionText = TEXT("GamePlatformDefinition:foundation.world@1"), Scenario = TEXT("Foundation"), Role;
    if(FParse::Value(FCommandLine::Get(),TEXT("FoundationRunId="),RunText))
    {
        if(!FGuid::ParseExact(RunText,EGuidFormats::DigitsWithHyphens,RunId)||!RunId.IsValid())
        { Fail(TEXT("InvalidFoundationRunId")); return; }
    }
    else RunId=FGuid::NewGuid();
    FParse::Value(FCommandLine::Get(),TEXT("FoundationWorldScenario="),Scenario);
    if(Scenario!=TEXT("Foundation")){Fail(TEXT("UnsupportedWorldScenario"));return;}
    FParse::Value(FCommandLine::Get(),TEXT("FoundationWorldDefinition="),DefinitionText);
    DefinitionId=FPrimaryAssetId(DefinitionText);
    FGamePlatformId Logical;
    if(DefinitionId.PrimaryAssetType!=TEXT("GamePlatformDefinition")||
       !FGamePlatformId::TryParse(DefinitionId.PrimaryAssetName.ToString(),Logical))
    {Fail(TEXT("InvalidWorldDefinitionId"));return;}
    FParse::Value(FCommandLine::Get(),TEXT("FoundationWorldServerRole="),Role);
    DevelopmentServerRole=FName(*Role);
    Exercise=DBAFoundationWorldExercise::FPolicy(FParse::Param(FCommandLine::Get(),TEXT("FoundationWorldExercise")));
    WorldOperation=FGuid::NewGuid();
    DeadlineSeconds=FPlatformTime::Seconds()+60;
    Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::Tick),0.1f);
}
void UDBAFoundationWorldBootstrap::Marker(const TCHAR* Event,const FGuid& Generation) const
{
    UE_LOG(LogDBAWorld,Display,TEXT("WorldValidation RunId=%s ProcessId=%u Scenario=Foundation Event=%s Generation=%s"),
        *RunId.ToString(EGuidFormats::DigitsWithHyphens).ToLower(),FPlatformProcess::GetCurrentProcessId(),Event,
        *Generation.ToString(EGuidFormats::DigitsWithHyphens).ToLower());
}
void UDBAFoundationWorldBootstrap::Fail(FName Code)
{
    if(bFailed)return;
    bFailed=true;
    // 不输出令牌/后端地址，不把失败打印为可被验收脚本接受的成功marker。
    UE_LOG(LogDBAWorld,Error,TEXT("FoundationWorld failed Code=%s RunId=%s"),*Code.ToString(),*RunId.ToString(EGuidFormats::DigitsWithHyphens));
}
void UDBAFoundationWorldBootstrap::AcquireDefinition(const FPrimaryAssetId& Id,TSubclassOf<UGamePlatformDefinitionBase> Class)
{
    auto* Data=IGamePlatformDataService::Get(*GetGameInstance());
    if(!Data){Fail(TEXT("DataUnavailable"));return;}
    const FGuid Expected=AttemptId;
    FGamePlatformResult Accepted;
    // 租约绑定当时World；完成回调只记录同一尝试的真实失败，不在回调里创建或释放对象。
    auto Lease=Data->AcquireDefinition(Id,Class,{},EGamePlatformDataLifetime::World,this,
        [Weak=TWeakObjectPtr<UDBAFoundationWorldBootstrap>(this),Expected](const auto&,const FGamePlatformResult& Result)
        {
            auto* Self=Weak.Get();
            if(!Self||Self->bStopping)return;
            if(Self->AttemptId!=Expected||!Self->ActiveWorld.IsValid()||Self->ActiveWorld->bIsTearingDown||
               Self->GetGameInstance()->GetWorld()!=Self->ActiveWorld.Get())
            {UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld old Data callback rejected Attempt=%s"),*Expected.ToString());return;}
            if(!Result.IsSuccess())Self->Fail(Result.Code);
        },Accepted);
    if(Lease.IsValid())Leases.Add(Lease);
    if(!Accepted.IsSuccess())Fail(Accepted.Code);
}
void UDBAFoundationWorldBootstrap::BeginWorld(UWorld& World)
{
    ActiveWorld=&World;AttemptId=FGuid::NewGuid();WorldGeneration.Invalidate();
    bFailed=false;bInitialized=false;bRegionsReady=false;bReadyReported=false;bBusyReported=false;
    bOldGenerationRejected=false;
    bRegionEventsComplete=false;ReceivedRegionEvents=0;ObserverStep=0;
    DeadlineSeconds=FPlatformTime::Seconds()+60;
    // Session尚未提供公开可信快照：禁止Client/Listen借开发参数绕过网络准入。
    if(World.GetNetMode()==NM_Client||World.GetNetMode()==NM_ListenServer)
    {Fail(TEXT("SessionPrerequisiteMissing"));return;}
    if(World.GetNetMode()==NM_DedicatedServer && DevelopmentServerRole!=TEXT("OpenWorld")&&
       DevelopmentServerRole!=TEXT("Village")&&DevelopmentServerRole!=TEXT("MainArena"))
    {Fail(TEXT("DevelopmentServerRoleMissing"));return;}
    AcquireDefinition(DefinitionId,UGamePlatformWorldDefinition::StaticClass());
}
bool UDBAFoundationWorldBootstrap::Tick(float)
{
    if(bStopping)return false;
    UWorld* Current=GetGameInstance()->GetWorld();
    if(!IsValid(Current)||Current->bIsTearingDown||(Current->WorldType!=EWorldType::Game&&Current->WorldType!=EWorldType::PIE))Current=nullptr;
    if(Current&&Current->GetGameInstance()!=GetGameInstance()){Fail(TEXT("ForeignWorldInstance"));return true;}
    if(bFailed){ReleaseOwnedResources();return true;}
    if(AdvanceExercise(Current))return true;
    if(AttemptId.IsValid()&&(!ActiveWorld.IsValid()||Current!=ActiveWorld.Get()))
    {
        // 先失效所有旧回调，Busy期间不初始化新World，也不丢弃工厂撤销句柄。
        AttemptId.Invalidate();PreviousGeneration=WorldGeneration;
        bPreviousResourcesReleased=ReleaseOwnedResources();
        if(!bPreviousResourcesReleased)return true;
        ActiveWorld.Reset();
    }
    if(!AttemptId.IsValid()&&(Operation.IsValid()||Factory.IsValid()||Leases.Num()||ProviderActors.Num()||Providers.Num()||RegionSubscription.IsValid()))
    {
        if(!ReleaseOwnedResources())return true;
        bPreviousResourcesReleased=true;
        ActiveWorld.Reset();
    }
    if(!Current||!Current->HasBegunPlay())return true;
    const FString Package=UWorld::RemovePIEPrefix(Current->GetOutermost()->GetName());
    if(Package==BootstrapPackage)
    {
        // 不主动Travel；只有确实返回Bootstrap且上轮资源已释放才记录跨图事实。
        if(bFirstReadyObserved&&!bReturnedToBootstrap&&bPreviousResourcesReleased)
        {
            bReturnedToBootstrap=true;
            Marker(TEXT("ReturnedToBootstrap"),PreviousGeneration);
            Marker(TEXT("CleanupComplete"),PreviousGeneration);
        }
        return true;
    }
    if(!AttemptId.IsValid())BeginWorld(*Current);
    if(bFailed){ReleaseOwnedResources();return true;}
    if(!bReadyReported&&FPlatformTime::Seconds()>=DeadlineSeconds){Fail(TEXT("FoundationWorldTimeout"));return true;}
    AdvanceWorld();
    if(!bFailed)AdvanceExercise(Current);
    return true;
}
bool UDBAFoundationWorldBootstrap::AdvanceExercise(UWorld* Current)
{
    using namespace DBAFoundationWorldExercise;
    const auto Stage=Exercise.Stage();
    if(Stage==EStage::Disabled||Stage==EStage::Completed)return false;
    FFacts Facts;
    Facts.bTimedOut=FPlatformTime::Seconds()>=DeadlineSeconds;
    if(Current)
    {
        const FString Map=UWorld::RemovePIEPrefix(Current->GetOutermost()->GetName());
        Facts.bDedicated=Current->GetNetMode()==NM_DedicatedServer;
        Facts.bNetworkPeer=Current->GetNetMode()==NM_Client||Current->GetNetMode()==NM_ListenServer;
        Facts.bSameInstance=Current->GetGameInstance()==GetGameInstance();
        Facts.bBegunPlay=Current->HasBegunPlay();Facts.bSandbox=Map==SandboxPackage;Facts.bBootstrap=Map==BootstrapPackage;
        FGuid UrlOperation;
        Facts.bOperationMatched=FGuid::ParseExact(Current->URL.GetOption(TEXT("FoundationWorldOperation="),TEXT("")),
            EGuidFormats::DigitsWithHyphens,UrlOperation)&&UrlOperation==WorldOperation;
    }
    Facts.bReady=bReadyReported&&Current&&Current==ActiveWorld.Get();
    Facts.bFreshGeneration=WorldGeneration.IsValid()&&PreviousGeneration.IsValid()&&WorldGeneration!=PreviousGeneration;
    Facts.bOldGenerationRejected=bOldGenerationRejected;
    if(Stage==EStage::ReleaseFirst)
    {
        AttemptId.Invalidate();PreviousGeneration=WorldGeneration;
        Facts.bOwnedReleased=ReleaseOwnedResources();bPreviousResourcesReleased=Facts.bOwnedReleased;
    }
    const auto Action=Exercise.Step(Facts);
    if(Action==EAction::Reject){Fail(TEXT("WorldExerciseUnauthorizedOrExpired"));return true;}
    if(Action==EAction::ReleaseOwned)
    {
        DeadlineSeconds=FPlatformTime::Seconds()+ReadinessTimeoutSeconds;
        UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld owned cleanup requested before Travel Generation=%s"),*WorldGeneration.ToString());
        return true;
    }
    if(Action==EAction::OpenBootstrap||Action==EAction::OpenSandbox)
    {
        if(Action==EAction::OpenSandbox)
        {
            bReturnedToBootstrap=true;
            Marker(TEXT("ReturnedToBootstrap"),PreviousGeneration);Marker(TEXT("CleanupComplete"),PreviousGeneration);
        }
        else UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld owned cleanup verified before Travel Generation=%s"),*PreviousGeneration.ToString());
        ActiveWorld.Reset();
        const TCHAR* Destination=Action==EAction::OpenBootstrap?BootstrapPackage:SandboxPackage;
        DeadlineSeconds=FPlatformTime::Seconds()+ReadinessTimeoutSeconds;
        const FString Options=TEXT("FoundationWorldOperation=")+WorldOperation.ToString(EGuidFormats::DigitsWithHyphens);
        UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld OpenLevel Target=%s Operation=%s"),Destination,*WorldOperation.ToString());
        UGameplayStatics::OpenLevel(GetGameInstance(),FName(Destination),true,Options);
        return true;
    }
    if(Action==EAction::Complete)
    {
        Marker(TEXT("Completed"),WorldGeneration);
        UE_LOG(LogDBAWorld,Display,TEXT("WorldValidationComplete RunId=%s"),*RunId.ToString(EGuidFormats::DigitsWithHyphens).ToLower());
        return false;
    }
    const auto Next=Exercise.Stage();
    return Next==EStage::ReleaseFirst||Next==EStage::WaitBootstrap||Next==EStage::WaitSandbox||Next==EStage::Failed;
}
void UDBAFoundationWorldBootstrap::AdvanceWorld()
{
    auto* World=ActiveWorld.Get();
    auto* Data=IGamePlatformDataService::Get(*GetGameInstance());
    auto* Loading=IGamePlatformLoadingService::Get(*GetGameInstance());
    auto* Service=World?IGamePlatformWorldService::Get(*World):nullptr;
    if(!Data||!Loading||!Service){Fail(TEXT("WorldDependencyUnavailable"));return;}
    if(Leases.IsEmpty()){Fail(TEXT("WorldLeaseMissing"));return;}
    const auto* Definition=Cast<UGamePlatformWorldDefinition>(Data->GetLoadedDefinition(Leases[0]));
    if(!Definition)return; // Data回调报告失败；加载状态受项目截止时间约束。
    if(!bInitialized)
    {
        const auto Valid=Definition->ValidateDefinition();
        if(!Valid.IsSuccess()){Fail(Valid.Code);return;}
        const FString Actual=UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
        if(Definition->MapIdentity.ToSoftObjectPath().GetLongPackageName()!=Actual||Actual!=SandboxPackage)
        {Fail(TEXT("FoundationMapMismatch"));return;}
        if(Definition->Regions.Num()!=2){Fail(TEXT("FoundationFixtureRequiresTwoRegions"));return;}
        // 项目开发版本只表示本夹具，不冒充UE版本或Session协商版本。
        FGamePlatformVersion BuildVersion;BuildVersion.Major=1;
        const auto Started=Service->InitializeDevelopment(DefinitionId,BuildVersion,DevelopmentServerRole);
        if(!Started.IsSuccess()){Fail(Started.Code);return;}
        bInitialized=true;WorldGeneration=Service->GetReadiness().Context.ContextGeneration;
        ReadinessTimeoutSeconds=Definition->ReadinessTimeoutSeconds;
        DeadlineSeconds=FPlatformTime::Seconds()+ReadinessTimeoutSeconds;
        RegionDefinitions=Definition->Regions;
        Marker(TEXT("Initialized"),WorldGeneration);Marker(TEXT("DefinitionLoaded"),WorldGeneration);Marker(TEXT("MapMatched"),WorldGeneration);
        for(const auto& Id:RegionDefinitions){AcquireDefinition(Id,UGamePlatformRegionDefinition::StaticClass());if(bFailed)return;}
    }
    auto Snapshot=Service->GetReadiness();
    if(Snapshot.Context.ContextGeneration!=WorldGeneration){Fail(TEXT("WorldGenerationMismatch"));return;}
    if(Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Failed||Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated)
    {Fail(Snapshot.Context.Result.Code);return;}
    if(!bRegionsReady)
    {
        for(int32 Index=Providers.Num();Index<RegionDefinitions.Num();++Index)
        {
            if(!Leases.IsValidIndex(Index+1)){Fail(TEXT("RegionLeaseMissing"));return;}
            const auto* Region=Cast<UGamePlatformRegionDefinition>(Data->GetLoadedDefinition(Leases[Index+1]));
            if(!Region)return;
            const auto Valid=Region->ValidateDefinition();if(!Valid.IsSuccess()){Fail(Valid.Code);return;}
            if(Region->GetPrimaryAssetId()!=RegionDefinitions[Index]){Fail(TEXT("RegionIdentityMismatch"));return;}
            if(!ProviderActors.IsValidIndex(Index))
            {
                FActorSpawnParameters Parameters;Parameters.ObjectFlags|=RF_Transient;
                Parameters.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                auto* Actor=World->SpawnActor<AActor>(AActor::StaticClass(),FixtureBounds(Index).GetCenter(),FRotator::ZeroRotator,Parameters);
                if(!Actor){Fail(TEXT("RegionProviderSpawnFailed"));return;}
                ProviderActors.Add(Actor);
            }
            FGamePlatformRegionProvider Provider;
            Provider.RegionId=Region->LogicalId;Provider.DefinitionId=RegionDefinitions[Index];
            Provider.ContextGeneration=WorldGeneration;Provider.Provider=ProviderActors[Index].Get();Provider.Bounds=FixtureBounds(Index);
            FGamePlatformResult Result;const auto Handle=Service->RegisterRegionProvider(Provider,Result);
            if(!Result.IsSuccess())
            {
                // 服务有自己的依赖租约，完成时序可能晚于本装配；不把未就绪当注册成功。
                if(Result.Code==TEXT("RegionDefinitionNotReady"))return;
                Fail(Result.Code);return;
            }
            if(!Handle.IsValid()){Fail(TEXT("InvalidProviderHandle"));return;}
            Providers.Add(Handle);
            RegionIdentities.Add(Region->LogicalId);
            FGamePlatformResult DuplicateResult;
            const auto Duplicate=Service->RegisterRegionProvider(Provider,DuplicateResult);
            if(Duplicate.IsValid())Providers.Add(Duplicate);
            if(Duplicate.IsValid()||DuplicateResult.IsSuccess()||DuplicateResult.Code!=TEXT("DuplicateRegionId"))
            {Fail(TEXT("DuplicateRegionProviderWasNotRejected"));return;}
            UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld duplicate provider rejected Region=%s"),*Region->LogicalId.ToString());
            FGamePlatformId Found;const auto Queried=Service->QueryRegion(Provider.Bounds.GetCenter(),Found);
            if(!Queried.IsSuccess()||Found!=Region->LogicalId){Fail(TEXT("FixtureRegionQueryFailed"));return;}
        }
        bRegionsReady=true;Marker(TEXT("RegionsReady"),WorldGeneration);
        if(bReturnedToBootstrap)
        {
            // 故意提交旧World代次，真实服务必须拒绝；不能用本地布尔模拟拒绝事实。
            const auto Old=Service->UpdateObserver(ProviderActors[0].Get(),FixtureBounds(0).GetCenter(),PreviousGeneration);
            if(Old.IsSuccess()||Old.Code!=TEXT("ObserverScopeMismatch")){Fail(TEXT("OldWorldGenerationWasNotRejected"));return;}
            bOldGenerationRejected=true;
            UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld old generation rejected Old=%s Current=%s Code=%s"),
                *PreviousGeneration.ToString(),*WorldGeneration.ToString(),*Old.Code.ToString());
        }
    }
    if(!AdvanceRegionEvents())return;
    if(!Factory.IsValid())
    {
        FGamePlatformResult Result;
        Factory=Loading->RegisterTaskFactory(TEXT("WorldReadiness"),[]{return GamePlatformWorldServices::CreateReadinessTask();},Result);
        if(!Result.IsSuccess())
        {
            if(Result.Code==TEXT("Busy"))
            {if(!bBusyReported){UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld waiting for Loading Busy; other operation retained"));bBusyReported=true;}return;}
            Fail(Result.Code);return;
        }
        if(!Factory.IsValid()){Fail(TEXT("InvalidReadinessFactory"));return;}
    }
    if(!Operation.IsValid())
    {
        FGamePlatformLoadingOperationSpec Spec;Spec.Purpose=TEXT("FoundationWorld");
        Spec.TargetWorldPackage=SandboxPackage;Spec.TimeoutSeconds=ReadinessTimeoutSeconds;
        FGamePlatformLoadingTaskSpec Task;Task.TaskId=TEXT("FoundationWorldReadiness");Task.TaskType=TEXT("WorldReadiness");
        Task.TimeoutSeconds=ReadinessTimeoutSeconds;Spec.Tasks.Add(Task);
        FGamePlatformResult Result;Operation=Loading->StartLoadingOperation(Spec,this,Result);
        if(!Result.IsSuccess()){if(Result.Code==TEXT("Busy"))return;Fail(Result.Code);return;}
        if(!Operation.IsValid()){Fail(TEXT("InvalidWorldLoadingHandle"));return;}
    }
    const auto Loaded=Loading->GetLoadingSnapshot();
    if(!(Loaded.Handle==Operation)){Fail(TEXT("WorldLoadingOwnershipLost"));return;}
    if(Loaded.State==EGamePlatformLoadingState::Failed||Loaded.State==EGamePlatformLoadingState::Cancelled||Loaded.State==EGamePlatformLoadingState::TimedOut)
    {Fail(Loaded.Result.Code);return;}
    Snapshot=Service->GetReadiness();
    // Loading终态不是永久承诺：额外检查当前World事实，Provider失效不能继续显示Ready。
    const bool bReady=Loading->IsReadyToPlay(Operation)&&Snapshot.Context.ReadinessState==EGamePlatformWorldReadiness::Ready;
    if(bReadyReported&&!bReady){Fail(TEXT("WorldReadinessLost"));return;}
    if(bReady&&!bReadyReported)
    {
        bReadyReported=true;
        if(bFirstReadyObserved&&bReturnedToBootstrap&&PreviousGeneration!=WorldGeneration)
        {
            Marker(TEXT("Reentered"),WorldGeneration);
        }
        else Marker(TEXT("WorldReady"),WorldGeneration);
        bFirstReadyObserved=true;
    }
}
bool UDBAFoundationWorldBootstrap::AdvanceRegionEvents()
{
    if(bRegionEventsComplete)return true;
    auto* World=ActiveWorld.Get();auto* Service=World?IGamePlatformWorldService::Get(*World):nullptr;
    if(!Service||RegionIdentities.Num()!=2||ProviderActors.Num()!=2){Fail(TEXT("RegionEventFixtureMissing"));return false;}
    if(!RegionSubscription.IsValid())
    {
        ExpectedRegionEvents={{RegionIdentities[0],true},{RegionIdentities[0],false},{RegionIdentities[1],true},{RegionIdentities[1],false}};
        const FGuid ExpectedAttempt=AttemptId,ExpectedGeneration=WorldGeneration;
        const TWeakObjectPtr<UObject> Observer=ProviderActors[0].Get();
        RegionSubscription=Service->SubscribeRegions(Observer,
            [Weak=TWeakObjectPtr<UDBAFoundationWorldBootstrap>(this),ExpectedAttempt,ExpectedGeneration,Observer](const FGamePlatformRegionEvent& Event)
            {
                auto* Self=Weak.Get();if(!Self||Self->bStopping)return;
                if(Self->AttemptId!=ExpectedAttempt||Event.ContextGeneration!=ExpectedGeneration||!Self->ActiveWorld.IsValid()||
                   Self->GetGameInstance()->GetWorld()!=Self->ActiveWorld.Get())
                {UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld old region callback rejected"));return;}
                if(Event.Observer!=Observer)return; // 其他玩法观察者不是本次夹具的证据。
                if(!Self->ExpectedRegionEvents.IsValidIndex(Self->ReceivedRegionEvents))
                {Self->Fail(TEXT("UnexpectedExtraRegionEvent"));return;}
                const auto& Expected=Self->ExpectedRegionEvents[Self->ReceivedRegionEvents];
                if(Event.RegionId!=Expected.Key||Event.bEntered!=Expected.Value)
                {Self->Fail(TEXT("RegionEventOrderMismatch"));return;}
                ++Self->ReceivedRegionEvents;
                UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld region event Index=%d Region=%s Entered=%d Generation=%s"),
                    Self->ReceivedRegionEvents,*Event.RegionId.ToString(),Event.bEntered?1:0,*ExpectedGeneration.ToString());
                // 服务发布回调期间禁止重入UpdateObserver；下一次装配Tick再提交下一步。
            });
        if(!RegionSubscription.IsValid()){Fail(TEXT("RegionSubscriptionRejected"));return false;}
    }
    FVector Position;bool bSubmit=false;
    if(ObserverStep==0){Position=FixtureBounds(0).GetCenter();bSubmit=true;}
    else if(ObserverStep==1&&ReceivedRegionEvents==1){Position=FixtureBounds(1).GetCenter();bSubmit=true;}
    else if(ObserverStep==2&&ReceivedRegionEvents==3){Position=FVector(0,0,2000);bSubmit=true;}
    if(bSubmit)
    {
        const auto Result=Service->UpdateObserver(ProviderActors[0].Get(),Position,WorldGeneration);
        if(!Result.IsSuccess()){Fail(Result.Code);return false;}
        ++ObserverStep;
    }
    if(ObserverStep==3&&ReceivedRegionEvents==4)
    {
        bRegionEventsComplete=true;
        UE_LOG(LogDBAWorld,Display,TEXT("FoundationWorld region event sequence verified Generation=%s"),*WorldGeneration.ToString());
    }
    return bRegionEventsComplete;
}
bool UDBAFoundationWorldBootstrap::ReleaseOwnedResources()
{
    auto* Instance=GetGameInstance();
    auto* Loading=Instance?IGamePlatformLoadingService::Get(*Instance):nullptr;
    bool bReleased=true;
    if(Operation.IsValid())
    {
        if(!Loading)bReleased=false;
        else
        {
            const auto Result=Loading->ReleaseLoadingOperation(Operation);
            if(Result.IsSuccess())Operation={};else bReleased=false;
        }
    }
    if(Factory.IsValid()&&!Operation.IsValid())
    {
        if(!Loading)bReleased=false;
        else
        {
            const auto Result=Loading->UnregisterTaskFactory(Factory);
            if(Result.IsSuccess())Factory={};
            else
            {
                bReleased=false;
                if(!bBusyReported){UE_LOG(LogDBAWorld,Warning,TEXT("FoundationWorld factory revoke pending Code=%s"),*Result.Code.ToString());bBusyReported=true;}
            }
        }
    }
    auto* World=ActiveWorld.Get();
    auto* Service=World?IGamePlatformWorldService::Get(*World):nullptr;
    if(RegionSubscription.IsValid())
    {
        if(!World||World->bIsTearingDown||(Service&&Service->Unregister(RegionSubscription)))RegionSubscription={};
        else bReleased=false;
    }
    for(int32 Index=Providers.Num()-1;Index>=0;--Index)
    {
        // TearDown由World服务失效整代记录；正常世界必须实际撤销才能记录清理成功。
        if(!World||World->bIsTearingDown||(Service&&Service->UnregisterRegionProvider(Providers[Index])))Providers.RemoveAt(Index);
        else bReleased=false;
    }
    if(Providers.IsEmpty()&&!RegionSubscription.IsValid())
    {
        // 撤销失败不能先销毁弱所有者再伪报成功；销毁失败也保留强引用供下一安全采样重试。
        for(int32 Index=ProviderActors.Num()-1;Index>=0;--Index)
        {
            auto* Actor=ProviderActors[Index].Get();
            if(!IsValid(Actor)||Actor->IsActorBeingDestroyed()||Actor->Destroy())ProviderActors.RemoveAt(Index);
            else bReleased=false;
        }
    }
    auto* Data=Instance?IGamePlatformDataService::Get(*Instance):nullptr;
    for(int32 Index=Leases.Num()-1;Index>=0;--Index)
    {
        if(Data&&Data->ReleaseDefinition(Leases[Index]).IsSuccess())Leases.RemoveAt(Index);
        else bReleased=false;
    }
    if(Leases.IsEmpty()){RegionDefinitions.Reset();RegionIdentities.Reset();}
    if(!RegionSubscription.IsValid())ExpectedRegionEvents.Reset();
    return bReleased;
}
void UDBAFoundationWorldBootstrap::Deinitialize()
{
    bStopping=true;AttemptId.Invalidate();
    if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}
    if(!ReleaseOwnedResources())
    {
        // 关闭顺序或他人Busy不能越权解除；无捕获工厂最终由所属Loading服务销毁，不伪报完整清理。
        UE_LOG(LogDBAWorld,Warning,TEXT("FoundationWorld cleanup incomplete during instance shutdown; service teardown owns remaining records"));
    }
    Super::Deinitialize();
}
