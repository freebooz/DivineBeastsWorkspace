#include "Subsystems/GamePlatformWorldSubsystem.h"
#include "Engine/World.h"
namespace {
bool SameReadiness(const FGamePlatformWorldReadinessSnapshot& A,const FGamePlatformWorldReadinessSnapshot& B)
{
 return A.bWorldObjectValid==B.bWorldObjectValid&&A.bDefinitionLoaded==B.bDefinitionLoaded&&
 A.bRegionDefinitionsLoaded==B.bRegionDefinitionsLoaded&&A.RegisteredRegionProviders==B.RegisteredRegionProviders&&
 A.bMapIdentityMatched==B.bMapIdentityMatched&&A.bSessionContextMatched==B.bSessionContextMatched&&
 A.bRequiredRegionsRegistered==B.bRequiredRegionsRegistered&&A.bRequiredStreamingReady==B.bRequiredStreamingReady&&
 A.bWorldNotTearingDown==B.bWorldNotTearingDown&&A.bContributorsReady==B.bContributorsReady&&
 A.Context.ContextGeneration==B.Context.ContextGeneration&&A.Context.WorldId==B.Context.WorldId&&
 A.Context.ExperienceId==B.Context.ExperienceId&&A.Context.RegionId==B.Context.RegionId&&
 A.Context.AuthorityKind==B.Context.AuthorityKind&&A.Context.ReadinessState==B.Context.ReadinessState&&
 A.Context.MapPackageName==B.Context.MapPackageName&&A.Context.Result.Status==B.Context.Result.Status&&
 A.Context.Result.Code==B.Context.Result.Code&&A.Context.Result.Message==B.Context.Result.Message;
}
}
FGamePlatformWorldRegistration UGamePlatformWorldSubsystem::SubscribeReadiness(TWeakObjectPtr<UObject> Owner,
 TFunction<void(const FGamePlatformWorldReadinessSnapshot&)> Callback,FGamePlatformResult& Out)
{
 check(IsInGameThread());
 if(!CanMutate()||!Owns(Owner)||!Callback){Out=FGamePlatformResult::Failure(TEXT("WorldNotificationScope"),TEXT("订阅者必须属于当前有效世界"));return {};}
 auto H=NewRegistration();auto Record=MakeShared<FWorldReadinessSubscription>();Record->Owner=Owner;Record->Callback=MoveTemp(Callback);
 ReadinessSubscriptions.Add(H.RegistrationId,MoveTemp(Record));Out=FGamePlatformResult::Success();return H;
}
bool UGamePlatformWorldSubsystem::UnsubscribeReadiness(const FGamePlatformWorldRegistration& H)
{
 check(IsInGameThread());
 if(!H.IsValid()||H.ContextGeneration!=Snapshot.Context.ContextGeneration)return false;
 ReadinessSubscriptions.Remove(H.RegistrationId);return true;
}
void UGamePlatformWorldSubsystem::PublishReadiness()
{
 // 嵌套Stop已将最终快照保留在Snapshot；不递归、不丢订阅，外层只补发一次。
 if(bPublishingReadiness)return;
 TGuardValue<bool> Publishing(bPublishingReadiness,true);TGuardValue<bool> Dispatching(bDispatching,true);
 for(int32 Pass=0;Pass<2;++Pass){
  for(auto It=ReadinessSubscriptions.CreateIterator();It;++It)if(!Owns(It.Value()->Owner))It.RemoveCurrent();
  if(bHasPublishedReadiness&&SameReadiness(PublishedReadiness,Snapshot))break;
  PublishedReadiness=Snapshot;bHasPublishedReadiness=true;
  const auto Value=PublishedReadiness;const auto List=ReadinessSubscriptions;
  for(const auto& Pair:List){
   // 退出立刻截断旧普通快照；终态只发给届时仍登记且Owner存活的完整身份。
   if(Snapshot.Context.ContextGeneration!=Value.Context.ContextGeneration)break;
   if(bClosing&&Value.Context.ReadinessState!=EGamePlatformWorldReadiness::Invalidated)break;
   const auto* Current=ReadinessSubscriptions.Find(Pair.Key);
   if(Current&&*Current==Pair.Value&&Owns(Pair.Value->Owner))Pair.Value->Callback(Value);
  }
  if(Value.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated){ReadinessSubscriptions.Reset();break;}
  if(!bClosing)break; // 第二轮仅允许收敛Stop保留的Invalidated，不是一般事件重试循环。
 }
}
