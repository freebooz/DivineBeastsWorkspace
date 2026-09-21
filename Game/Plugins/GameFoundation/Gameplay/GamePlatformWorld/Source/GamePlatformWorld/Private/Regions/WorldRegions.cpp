#include "Subsystems/GamePlatformWorldSubsystem.h"
#include "Context/WorldPolicy.h"
#include "Streaming/GamePlatformWorldStreaming.h"
#include "Engine/World.h"
namespace Policy = GamePlatformWorldPolicy;
static Policy::FBox PolicyBox(const FBox& B){return {{B.Min.X,B.Min.Y,B.Min.Z},{B.Max.X,B.Max.Y,B.Max.Z}};}
static FGamePlatformResult RegionError(FName Code){return FGamePlatformResult::Failure(Code,TEXT("区域/观察者请求不满足当前世界身份、定义或边界约束"));}
FGamePlatformWorldRegistration UGamePlatformWorldSubsystem::NewRegistration() const
{return {Snapshot.Context.ContextGeneration,FGuid::NewGuid()};}
FGamePlatformWorldRegistration UGamePlatformWorldSubsystem::RegisterRegionProvider(const FGamePlatformRegionProvider& Value,FGamePlatformResult& Out)
{
    check(IsInGameThread());Out=RegionError(TEXT("InvalidRegionProvider"));
    if(!CanMutate()||!bStarted||!Owns(Value.Provider)||Value.ContextGeneration!=Snapshot.Context.ContextGeneration||
        !Value.RegionId.IsValid()||!Value.Bounds.IsValid||!Policy::ValidBox(PolicyBox(Value.Bounds)))return {};
    const auto* Id=RegionIdentities.Find(Value.DefinitionId);
    if(!Id||!(*Id==Value.RegionId)){Out=RegionError(TEXT("RegionDefinitionNotReady"));return {};}
    // 先清理弱Provider再做重复检测，销毁对象不永久占住逻辑身份。
    Regions.RemoveAll([this](const auto& R){return !Owns(R.Value.Provider);});
    if(Regions.ContainsByPredicate([&](const auto& R){return R.Value.RegionId==Value.RegionId;}))
    {Out=RegionError(TEXT("DuplicateRegionId"));return {};}
    const auto Handle=NewRegistration();Regions.Add({Handle,Value});Out=FGamePlatformResult::Success();return Handle;
}
bool UGamePlatformWorldSubsystem::UnregisterRegionProvider(const FGamePlatformWorldRegistration& Handle)
{
    check(IsInGameThread());
    if(!CanMutate()||!Handle.IsValid()||Handle.ContextGeneration!=Snapshot.Context.ContextGeneration)return false;
    return Regions.RemoveAll([&](const auto& R){return R.Handle.RegistrationId==Handle.RegistrationId;})>0;
}
FGamePlatformResult UGamePlatformWorldSubsystem::QueryRegion(const FVector& P,FGamePlatformId& Out)
{
    check(IsInGameThread());Out={};
    if(bClosing||!Policy::ValidPoint({P.X,P.Y,P.Z}))return RegionError(TEXT("InvalidRegionQuery"));
    const FGamePlatformRegionProvider* Best=nullptr;bool Ambiguous=false;
    for(const auto& Record:Regions)
    {
        const auto& V=Record.Value;
        if(!Owns(V.Provider)||!Policy::Contains(PolicyBox(V.Bounds),{P.X,P.Y,P.Z}))continue;
        if(!Best){Best=&V;continue;}
        const int Compare=Policy::CompareCandidate(V.Priority,PolicyBox(V.Bounds),Best->Priority,PolicyBox(Best->Bounds));
        if(Compare>0){Best=&V;Ambiguous=false;}else if(Compare==0){Ambiguous=true;}
    }
    if(Ambiguous)return RegionError(TEXT("AmbiguousRegion"));
    if(Best)Out=Best->RegionId;
    return FGamePlatformResult::Success();
}
FGamePlatformResult UGamePlatformWorldSubsystem::UpdateObserver(TWeakObjectPtr<UObject> Owner,const FVector& P,const FGuid& Generation)
{
    check(IsInGameThread());
    if(!CanMutate()||!Owns(Owner)||Generation!=Snapshot.Context.ContextGeneration)return RegionError(TEXT("ObserverScopeMismatch"));
    FGamePlatformId Current;auto Result=QueryRegion(P,Current);if(!Result.IsSuccess())return Result;
    auto* Record=Observers.FindByPredicate([&](const auto& V){return V.Owner==Owner;});
    if(!Record){Record=&Observers.Add_GetRef({Owner,P,{}});}else Record->Position=P;
    if(!(Record->Region==Current))
    {
        if(Record->Region.IsValid())PendingEvents.Add({Generation,Owner,Record->Region,false});
        if(Current.IsValid())PendingEvents.Add({Generation,Owner,Current,true});
        Record->Region=Current;
    }
    // Context.RegionId仅在恰好一个明确观察者时有意义，多观察者不任取一个。
    Snapshot.Context.RegionId=Observers.Num()==1?Current:FGamePlatformId{};
    return Result;
}
void UGamePlatformWorldSubsystem::UpdateRegions()
{
    Regions.RemoveAll([this](const auto& V){return !Owns(V.Value.Provider);});
    Observers.RemoveAll([this](const auto& V){return !Owns(V.Owner);});
    for(auto It=Subscriptions.CreateIterator();It;++It)if(!Owns(It.Value()->Owner))It.RemoveCurrent();
    const auto Copy=Observers;
    for(const auto& Observer:Copy)
    {
        auto Result=UpdateObserver(Observer.Owner,Observer.Position,Snapshot.Context.ContextGeneration);
        if(!Result.IsSuccess())Fail(Result.Code,Result.Message);
    }
    if(Observers.Num()!=1)Snapshot.Context.RegionId={};
}
FGamePlatformWorldRegistration UGamePlatformWorldSubsystem::SubscribeRegions(TWeakObjectPtr<UObject> Owner,TFunction<void(const FGamePlatformRegionEvent&)> Callback)
{
    check(IsInGameThread());if(!CanMutate()||!Owns(Owner)||!Callback)return {};
    const auto Handle=NewRegistration();auto Entry=MakeShared<FWorldRegionSubscription>();Entry->Owner=Owner;Entry->Callback=MoveTemp(Callback);
    Subscriptions.Add(Handle.RegistrationId,MoveTemp(Entry));return Handle;
}
bool UGamePlatformWorldSubsystem::Unregister(const FGamePlatformWorldRegistration& Handle)
{
    check(IsInGameThread());if(!CanMutate()||!Handle.IsValid()||Handle.ContextGeneration!=Snapshot.Context.ContextGeneration)return false;
    return Subscriptions.Remove(Handle.RegistrationId)+Contributors.Remove(Handle.RegistrationId)>0;
}
FGamePlatformWorldRegistration UGamePlatformWorldSubsystem::RegisterReadinessContributor(TWeakObjectPtr<UObject> Owner,TSharedRef<IGamePlatformWorldReadinessContributor> Contributor,FGamePlatformResult& Out)
{
    check(IsInGameThread());Out=RegionError(TEXT("InvalidContributorOwner"));if(!CanMutate()||!Owns(Owner))return {};
    const auto Handle=NewRegistration();Contributors.Add(Handle.RegistrationId,{Owner,Contributor});Out=FGamePlatformResult::Success();return Handle;
}
FGamePlatformWorldStreamingHandle UGamePlatformWorldSubsystem::RequestStreaming(const FGamePlatformWorldStreamingRequest& Request,FGamePlatformResult& Out)
{
    check(IsInGameThread());Out=RegionError(TEXT("WorldClosing"));if(!CanMutate()||!bStarted||!Streaming)return {};
    return Streaming->Request(Request,Out);
}
FGamePlatformResult UGamePlatformWorldSubsystem::CancelStreaming(const FGamePlatformWorldStreamingHandle& Handle)
{check(IsInGameThread());return CanMutate()&&Streaming?Streaming->Cancel(Handle):RegionError(TEXT("WorldClosing"));}
