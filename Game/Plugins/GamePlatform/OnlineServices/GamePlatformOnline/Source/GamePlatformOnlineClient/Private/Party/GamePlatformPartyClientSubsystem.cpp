// 平台普通队伍观察所有权与取消实现。仅短暂传输故障使用至多四次一次性退避；不扫描Widget、队伍或世界状态。
#include "Party/GamePlatformPartyClientSubsystem.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Async/Async.h"

void UGamePlatformPartyClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformOnlineClientSubsystem>();
    Online=GetGameInstance()->GetSubsystem<UGamePlatformOnlineClientSubsystem>();
    if(auto* Service=Online.Get())
    {
        AuthHandle=Service->OnAuthStateChanged().AddUObject(this,&UGamePlatformPartyClientSubsystem::AuthenticationChanged);
        AuthenticationChanged(Service->GetSnapshot());
    }
}
void UGamePlatformPartyClientSubsystem::Deinitialize()
{
    bStopping=true;StopObservation();
    if(auto* Service=Online.Get())Service->OnAuthStateChanged().Remove(AuthHandle);
    AuthHandle.Reset();Online.Reset();Snapshot={};Changed.Clear();Super::Deinitialize();
}
void UGamePlatformPartyClientSubsystem::StopObservation()
{
    ++ObservationGeneration;
    const auto OldRequest=Request;Request={};
    if(RetryHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(RetryHandle);RetryHandle.Reset();}
    if(auto* Service=Online.Get())Service->Cancel(OldRequest);
}
void UGamePlatformPartyClientSubsystem::AuthenticationChanged(const FGamePlatformAuthSnapshot& Authentication)
{
    const bool bAuthenticated=Authentication.State==EGamePlatformAuthState::Authenticated||Authentication.State==EGamePlatformAuthState::Refreshing;
    const auto* Service=Online.Get();
    const FString NewPlayer=bAuthenticated&&Service?Service->GetAuthentication().PlayerId:FString();
    if(AuthGeneration==Authentication.AuthGeneration&&PlayerId==NewPlayer)return;
    StopObservation();AuthGeneration=Authentication.AuthGeneration;PlayerId=NewPlayer;Cursor.Empty();Snapshot={};ConsecutiveFailures=0;
    Changed.Broadcast(Snapshot);
    if(!bStopping&&!PlayerId.IsEmpty())ContinueObservation(ObservationGeneration);
}
void UGamePlatformPartyClientSubsystem::Synchronize()
{
    if(bStopping||PlayerId.IsEmpty())return;
    StopObservation();Cursor.Empty();ConsecutiveFailures=0;ContinueObservation(ObservationGeneration);
}
void UGamePlatformPartyClientSubsystem::ContinueObservation(uint64 ExpectedGeneration)
{
    auto* Service=Online.Get();
    if(!Service||bStopping||ExpectedGeneration!=ObservationGeneration||PlayerId.IsEmpty()||Request.RequestId.IsValid())return;
    FGamePlatformOnlineRequestOptions Options;Options.Owner=this; // GameInstance订阅跨地图，页面只拥有其投影委托。
    const TWeakObjectPtr<UGamePlatformPartyClientSubsystem> Weak(this);
    Request=Service->ObserveParty(Cursor,Options,[Weak,ExpectedGeneration](const FGamePlatformPartyResult& Result)
    {if(auto* Self=Weak.Get())Self->CompleteObservation(ExpectedGeneration,Result);});
}
void UGamePlatformPartyClientSubsystem::CompleteObservation(uint64 ExpectedGeneration,const FGamePlatformPartyResult& Result)
{
    if(bStopping||ExpectedGeneration!=ObservationGeneration)return;
    Request={};
    const bool bIdentityValid=Result.ObservationScopeId==TEXT("party/")+PlayerId&&
        (!Result.Snapshot.bHasParty||Result.Snapshot.Members.ContainsByPredicate([this](const auto& Member){return Member.PlayerId==PlayerId;}));
    if(Result.bSuccess&&bIdentityValid)
    {
        const bool bNotify=!Snapshot.bSuccess||Result.bChanged||Result.bResyncRequired;
        Snapshot=Result;Cursor=Result.ObservationCursor;ConsecutiveFailures=0;
        if(bNotify)Changed.Broadcast(Snapshot);
        // 通知允许同步退出/重同步，后继请求必须重新核对代次，不递归执行可能同步完成的测试提供者。
        const TWeakObjectPtr<UGamePlatformPartyClientSubsystem> Weak(this);
        AsyncTask(ENamedThreads::GameThread,[Weak,ExpectedGeneration](){if(auto* Self=Weak.Get())Self->ContinueObservation(ExpectedGeneration);});
        return;
    }
    Snapshot.bSuccess=false;Snapshot.ErrorCode=Result.bSuccess?FName(TEXT("Party.InvalidResponse")):Result.ErrorCode;
    Changed.Broadcast(Snapshot);
    if(bStopping||ExpectedGeneration!=ObservationGeneration||++ConsecutiveFailures>4)return;
    // 只在可恢复传输错误后退避；无权限、非法响应和未提供接口必须停在明确失败状态，等待外部恢复/显式重试。
    if(Result.HttpStatusCode==400||Result.HttpStatusCode==403||Result.HttpStatusCode==404||Snapshot.ErrorCode==TEXT("Party.InvalidResponse"))return;
    const float Delay=static_cast<float>(1<<(ConsecutiveFailures-1));
    const TWeakObjectPtr<UGamePlatformPartyClientSubsystem> Weak(this);
    RetryHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,ExpectedGeneration](float)
    {
        if(auto* Self=Weak.Get())
        {
            if(ExpectedGeneration==Self->ObservationGeneration){Self->RetryHandle.Reset();Self->ContinueObservation(ExpectedGeneration);}
        }
        return false;
    }),Delay);
}
