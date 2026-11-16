// Online人工Provider夹具：没有网络实现、生产URL或真实凭据；所有认证/队伍状态仍经真实Online解析与生命周期。
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Party/GamePlatformPartyClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
namespace DivineBeasts::Demo
{
struct FManualOnlineWire
{
    FName Kind;uint64 Sequence=0,Generation=0;FGamePlatformAuthenticatedCompletion Completion;
};
class FManualOnlineProvider final : public IGamePlatformOnlineAuthProvider
{
public:
    FGamePlatformAuthCompletion Authentication;FGamePlatformAuthLogoutCompletion LogoutCompletion;
    FName AuthKind;uint64 AuthSequence=0,AuthGeneration=1,WireGeneration=1,NextSequence=0,LogoutSequence=0;
    TMap<FGuid,FManualOnlineWire> Requests;TArray<FGuid> RequestOrder;
    TFunction<void()> OnPendingChanged;
    void Notify(){const auto Callback=OnPendingChanged;if(Callback)Callback();}
    void BeginAuth(FName Kind,FGamePlatformAuthCompletion C)
    {
        if(Authentication){FGamePlatformAuthProviderResult R;R.Error=EGamePlatformAuthError::AuthenticationBusy;C(R);return;}
        AuthKind=Kind;AuthSequence=++NextSequence;Authentication=MoveTemp(C);Notify();
    }
    FGamePlatformResult Configure(const FGamePlatformOnlineConfiguration&) override{return FGamePlatformResult::Success();}
    void TryAutoLogin(FGamePlatformAuthCompletion C) override{BeginAuth(TEXT("AutoLogin"),MoveTemp(C));}
    void LoginWithCredentials(const FString&,const FString&,FGamePlatformAuthCompletion C) override{BeginAuth(TEXT("Login"),MoveTemp(C));}
    void Refresh(FGamePlatformAuthCompletion C) override{BeginAuth(TEXT("RefreshAuth"),MoveTemp(C));}
    void Logout(FGamePlatformAuthLogoutCompletion C) override
    {
        if(LogoutCompletion){FGamePlatformAuthProviderLogoutResult R;R.Error=EGamePlatformAuthError::AuthenticationBusy;C(R);return;}
        LogoutSequence=++NextSequence;LogoutCompletion=MoveTemp(C);Notify();
    }
    static FName Classify(const FGamePlatformAuthenticatedRequest& R)
    {
        if(R.RelativePath.Contains(TEXT("observe")))return TEXT("PartyObserve");
        if(R.RelativePath.Contains(TEXT("invitations")))return TEXT("PartyInvitations");
        if(R.RelativePath.Contains(TEXT("party")))return R.Verb==TEXT("GET")?FName(TEXT("PartyRead")):FName(TEXT("PartyCommand"));
        if(R.RelativePath.Contains(TEXT("profile")))return R.Verb==TEXT("GET")?FName(TEXT("ProfileRead")):FName(TEXT("ProfileUpdate"));
        if(R.RelativePath.Contains(TEXT("health")) || R.RelativePath.Contains(TEXT("ready")))return TEXT("Probe");
        return TEXT("OtherDomain"); // 不回显路径/Body；这些消费者只能由操作者按完整身份交付失败。
    }
    void Store(const FGuid& Id,const FGamePlatformAuthenticatedRequest& R,FGamePlatformAuthenticatedCompletion C)
    {
        if(Requests.Contains(Id)){FGamePlatformAuthenticatedResponse Failure;Failure.Error=EGamePlatformAuthError::QueueFull;C(Failure);return;}
        FManualOnlineWire Wire;Wire.Kind=Classify(R);Wire.Sequence=++NextSequence;Wire.Generation=WireGeneration;Wire.Completion=MoveTemp(C);
        RequestOrder.Add(Id);Requests.Add(Id,MoveTemp(Wire));Notify();
    }
    void SendUnauthenticatedRequest(const FGuid& I,const FGamePlatformAuthenticatedRequest& R,FGamePlatformAuthenticatedCompletion C) override{Store(I,R,MoveTemp(C));}
    void SendAuthenticatedRequest(const FGuid& I,const FGamePlatformAuthenticatedRequest& R,FGamePlatformAuthenticatedCompletion C) override{Store(I,R,MoveTemp(C));}
    void CancelRequest(const FGuid& I) override{Requests.Remove(I);RequestOrder.Remove(I);Notify();}
    void InvalidateAuthenticationOperation() override{++AuthGeneration;Authentication={};}
    bool ApplyAuthorization(IHttpRequest&) const override{return false;}
    void CancelAll() override{++AuthGeneration;++WireGeneration;Authentication={};LogoutCompletion={};Requests.Reset();RequestOrder.Reset();Notify();}
    FString PendingText() const
    {
        FString Out=FString::Printf(TEXT("Auth=%s AuthSeq=%llu AuthGen=%llu LogoutSeq=%llu"),
            *AuthKind.ToString(),AuthSequence,AuthGeneration,LogoutSequence);
        for(const auto& Id:RequestOrder)if(const auto* W=Requests.Find(Id))Out+=FString::Printf(TEXT("；Wire=%s Seq=%llu Gen=%llu Request=%s"),
            *W->Kind.ToString(),W->Sequence,W->Generation,*Id.ToString());
        return Out;
    }
};
struct FManualOnlineCompletionIdentity
{FGamePlatformOnlineRequestHandle Handle;FName Kind;uint64 Serial=0,SourceGeneration=0;};
struct FManualOnlineView
{
    FDivineBeastsDemoRunToken Token;TWeakObjectPtr<UWorld> World;FGamePlatformAuthSnapshot Auth;

    TFunction<void(const FDivineBeastsDemoRunToken&,const FString&)> PublishObservation;
    FGamePlatformResult LastResponse;uint64 Events=0,NextSerial=0,SourceGeneration=1;bool bAlive=true;
    FGuid NativeScope;
    TMap<FGuid,TPair<FName,uint64>> Outstanding;
    void Finish(const FManualOnlineCompletionIdentity& Identity,const FGamePlatformResult& Result)
    {
        if(!bAlive || Identity.SourceGeneration!=SourceGeneration || Identity.Handle.InstanceScopeId!=NativeScope)return;
        const auto* Expected=Outstanding.Find(Identity.Handle.RequestId);
        if(!Expected || Expected->Key!=Identity.Kind || Expected->Value!=Identity.Serial)return;
        Outstanding.Remove(Identity.Handle.RequestId); // 先取走终态资格，再发布，允许Stop重入。
        if(PublishObservation)PublishObservation(Token,FString::Printf(TEXT("NativeOnline终态 Kind=%s Request=%s Serial=%llu Status=%d Code=%s"),
            *Identity.Kind.ToString(),*Identity.Handle.RequestId.ToString(),Identity.Serial,int32(Result.Status),*Result.Code.ToString()));
        Publish(Result);
    }
    void Publish(const FGamePlatformResult& R)
    {
        if(!bAlive || !World.IsValid() || World->bIsTearingDown)return;
        ++Events;LastResponse=R;
        if(PublishObservation)PublishObservation(Token,FString::Printf(TEXT("Online AuthState=%d AuthError=%d ResponseStatus=%d ResponseCode=%s Events=%llu"),
            int32(Auth.State),int32(Auth.Error),int32(R.Status),*R.Code.ToString(),Events));

    }
};
class FManualOnlineScenario final : public FPlatformManualBase
{
public:
    FManualOnlineScenario(UWorld& W,FName F):FPlatformManualBase(W,F),View(MakeShared<FManualOnlineView>()){}
    ~FManualOnlineScenario() override{CleanupService();}
private:
    FGamePlatformResult PrepareService() override
    {
        if(!GEngine)return ManualError(TEXT("Demo.OnlineEngineMissing"),TEXT("Online隔离夹具需要真实Engine宿主。"));
        Host=Instance();
        if(!Host.IsValid())return ManualError(TEXT("Demo.OnlineActualGI"),TEXT("需要真实GI。"));
        HostLease=Host->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>();
        if(!HostLease.IsValid())return ManualError(TEXT("Demo.OnlineActualCollection"),TEXT("宿主未装配真实受控子系统集合。"));
        const auto Exclusive=HostLease->Acquire(Context.Token,*SourceWorld);if(!Exclusive.IsSuccess())return Exclusive;
        Online=Host->GetSubsystem<UGamePlatformOnlineClientSubsystem>();Party=Host->GetSubsystem<UGamePlatformPartyClientSubsystem>();
        if(!Online.IsValid() || !Party.IsValid() || !Online->CaptureIdleProviderState(PreviousProvider))
            return ManualError(TEXT("Demo.OnlineNotIdle"),TEXT("真实Online/Party未就绪或认证/请求非空，不能替换机制。"));
        bCapturedProvider=true;Provider=MakeShared<FManualOnlineProvider>();Online->SetProvider(Provider);
        View->Token=Context.Token;View->World=SourceWorld;View->PublishObservation=Context.PublishObservation;View->Auth=Online->GetSnapshot();View->NativeScope=Online->GetDiagnostics().InstanceScopeId;
        TWeakPtr<FManualOnlineView> Weak(View);
        AuthChanged=Online->OnAuthStateChanged().AddLambda([Weak](const FGamePlatformAuthSnapshot& S)
        {
            auto V=Weak.Pin();if(!V || !V->bAlive || !V->World.IsValid())return;V->Auth=S;
            // 这是原生认证投影；本地Provider夹具成功不代表服务器认证。
            auto R=ManualObserved(FString::Printf(TEXT("隔离认证 State=%d Error=%d；无真实网络凭据"),int32(S.State),int32(S.Error)));
            if(S.State==EGamePlatformAuthState::Failed)R=ManualError(TEXT("Demo.NativeAuthenticationFailed"),TEXT("真实Online接受夹具失败响应后进入Failed。"));
            V->Publish(R);
        });
        Provider->OnPendingChanged=[Weak,P=TWeakPtr<FManualOnlineProvider>(Provider)](){auto V=Weak.Pin();auto Port=P.Pin();
            if(V && Port && V->bAlive && V->PublishObservation)V->PublishObservation(V->Token,Port->PendingText());};
        PartyChanged=Party->OnChanged().AddLambda([Weak](const auto& Result){if(auto V=Weak.Pin())V->Publish(
            ManualObserved(FString::Printf(TEXT("真实Party持续投影 Success=%d Changed=%d Resync=%d Http=%d Code=%s"),
                Result.bSuccess,Result.bChanged,Result.bResyncRequired,Result.HttpStatusCode,*Result.ErrorCode.ToString())));});
        return ManualObserved(TEXT("真实GI SubsystemCollection的唯一Online/Party已接；Provider原配置已捕获，退出精确恢复。"));
    }
    FGamePlatformResult CleanupService() override
    {
        View->bAlive=false;++View->SourceGeneration;View->Outstanding.Reset();
        if(Party.IsValid())Party->OnChanged().Remove(PartyChanged);
        if(Online.IsValid())
        {
            Online->OnAuthStateChanged().Remove(AuthChanged);
            if(bCapturedProvider)
            {
                const auto R=Online->RestoreIdleProviderState(PreviousProvider,Provider,ProviderRestoreLease);
                if(!R.IsSuccess())return R;
                bCapturedProvider=false; // 原生AuthGeneration通知使同GI Party停止长等待/清空旧账号。
            }
        }
        else if(bCapturedProvider)return ManualError(TEXT("Demo.OnlineRestoreServiceLost"),TEXT("原Provider尚未恢复，服务已失效。"));
        if(Provider){Provider->OnPendingChanged={};Provider->CancelAll();}Provider.Reset();Online.Reset();Party.Reset();Host.Reset();
        if(HostLease.IsValid()){const auto R=HostLease->Release(Context.Token);if(!R.IsSuccess())return R;HostLease.Reset();}
        return FGamePlatformResult::Success();
    }
    FString ResponseBody(const FManualArguments& A) const
    {
        switch(A.Int(TEXT("ResponseKind")))
        {
            case 0:return TEXT(R"({"ready":true,"service":"gatewayservice","contractVersion":"1.0.0"})");
            case 1:return FString::Printf(TEXT(R"({"playerId":"ManualAccount","gameId":"ManualGame","displayName":"ManualPlayer","dataVersion":"1","revision":"%lld","tutorialCompleted":false,"defaultWorldId":"ManualWorld","selectedCharacterId":"","ownedCharacterIds":[]})"),A.Int(TEXT("Revision")));
            case 2:return FString::Printf(TEXT(R"({"hasParty":true,"partyId":"ManualParty","leaderPlayerId":"ManualAccount","revision":"%lld","rosterLocked":false,"members":[{"playerId":"ManualAccount","displayName":"ManualPlayer","ready":false,"online":true}],"requests":[],"serverTimeUtc":"2026-10-27T00:00:00Z"})"),A.Int(TEXT("Revision")));
            case 3:return TEXT("[]");
            case 4:return TEXT(R"({"scopeId":"party/ManualAccount","cursor":"1-0","changed":true,"resyncRequired":false,"serverTimeUtc":"2026-10-27T00:00:00Z","snapshot":{"hasParty":false,"partyId":"","leaderPlayerId":"","revision":"0","rosterLocked":false,"members":[],"requests":[],"serverTimeUtc":"2026-10-27T00:00:00Z"},"invitations":[]})");
            default:return TEXT("{MalformedFixture");
        }
    }
    void ExecuteService(FName Op,const FManualArguments& A,FDivineBeastsDemoOperationCompletion Done) override
    {
        FGamePlatformResult R;
        if(Op==TEXT("Configure"))
        {
            FGamePlatformOnlineConfiguration C;C.ServiceOrigin=TEXT("https://manual.invalid");C.GameId=TEXT("ManualGame");C.ClientVersion=TEXT("1.0.0");
            C.MaxConcurrentRequests=static_cast<int32>(A.Int(TEXT("Concurrency")));C.MaxQueuedRequests=static_cast<int32>(A.Int(TEXT("QueueLimit")));
            R=Online->Configure(C);
        }
        else if(Op==TEXT("Login")){Online->LoginWithCredentials(TEXT("ManualFixtureUser"),TEXT("NOT_A_REAL_CREDENTIAL"));R=ManualObserved(TEXT("原生登录已调用，等待手动认证响应；不使用真实口令。"));}
        else if(Op==TEXT("AutoLogin")){Online->TryAutoLogin();R=ManualObserved(TEXT("原生自动登录已调用；等待手动Provider。"));}
        else if(Op==TEXT("RefreshAuth")){Online->Refresh();R=ManualObserved(TEXT("原生刷新已调用；尚未完成。"));}
        else if(Op==TEXT("Logout")){Online->Logout();R=ManualObserved(TEXT("原生退出已调用；远端撤销未确认。"));}
        else if(Op==TEXT("DeliverAuth"))
        {
            const auto P=Provider;const FName Kind=A.Id(TEXT("AuthKind"));const int64 Sequence=A.Int(TEXT("AuthSequence"));
            if(P->Authentication && Kind==P->AuthKind && Sequence>0 && static_cast<uint64>(Sequence)==P->AuthSequence)
            {
                const uint64 G=P->AuthGeneration;auto C=MoveTemp(P->Authentication);
                FGamePlatformAuthProviderResult Result;Result.bSuccess=A.Bool(TEXT("Success"));Result.AccountId=TEXT("ManualAccount");Result.SessionId=TEXT("ManualSession");
                Result.AccessExpiresAt=FDateTime::UtcNow()+FTimespan::FromHours(1);Result.RefreshExpiresAt=FDateTime::UtcNow()+FTimespan::FromHours(2);
                Result.Error=Result.bSuccess?EGamePlatformAuthError::None:EGamePlatformAuthError::InvalidCredentials;C(MoveTemp(Result));
                if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("认证交付中本运行已停止。")));return;}
                Done(Context.Token,ManualObserved(FString::Printf(TEXT("Auth %s Sequence=%lld Generation=%llu已交付；不把中间认证通知当请求终态。"),*Kind.ToString(),Sequence,G)));return;
            }
            if(P->LogoutCompletion && Kind==TEXT("Logout") && Sequence>0 && static_cast<uint64>(Sequence)==P->LogoutSequence)
            {
                auto C=MoveTemp(P->LogoutCompletion);FGamePlatformAuthProviderLogoutResult Result;Result.bServerRevoked=A.Bool(TEXT("Success"));
                Result.Error=Result.bServerRevoked?EGamePlatformAuthError::None:EGamePlatformAuthError::NetworkUnavailable;C(Result);
                Done(Context.Token,ManualObserved(TEXT("原Logout完成端已交付；无真实远程撤销证据。")));return;
            }
            Done(Context.Token,ManualError(TEXT("Demo.AuthResponseIdentity"),TEXT("AuthKind/Sequence不匹配，未消费任何完成端。")));return;
        }
        else if(Op==TEXT("DeliverRequest"))
        {
            const auto P=Provider;const FName Kind=A.Id(TEXT("WireKind"));const int64 Sequence=A.Int(TEXT("WireSequence"));
            FGuid Id;
            for(const auto& Candidate:P->RequestOrder)if(const auto* W=P->Requests.Find(Candidate))
                if(W->Kind==Kind && Sequence>0 && W->Sequence==static_cast<uint64>(Sequence) && W->Generation==P->WireGeneration){Id=Candidate;break;}
            auto* Stored=P->Requests.Find(Id);
            if(!Stored){Done(Context.Token,ManualError(TEXT("Demo.OnlineResponseIdentity"),TEXT("WireKind/Sequence/代次不匹配，未消费其它请求。")));return;}
            const auto MetadataKind=Stored->Kind;const uint64 Generation=Stored->Generation;
            auto Callback=MoveTemp(Stored->Completion);P->Requests.Remove(Id);P->RequestOrder.Remove(Id);
            FGamePlatformAuthenticatedResponse Response;Response.HttpStatusCode=static_cast<int32>(A.Int(TEXT("HttpStatus")));
            Response.Error=Response.HttpStatusCode>=200 && Response.HttpStatusCode<300?EGamePlatformAuthError::None:
                Response.HttpStatusCode==401?EGamePlatformAuthError::AuthExpired:EGamePlatformAuthError::ServiceUnavailable;
            if(MetadataKind==TEXT("OtherDomain")){Response.HttpStatusCode=503;Response.Error=EGamePlatformAuthError::ServiceUnavailable;}
            else Response.Body=ResponseBody(A);
            Callback(MoveTemp(Response));
            if(!Alive()){Done(Context.Token,FGamePlatformResult::Cancelled(TEXT("HTTP交付中运行已停止。")));return;}
            Done(Context.Token,ManualObserved(FString::Printf(TEXT("Wire %s Sequence=%lld Generation=%llu Request=%s已交付；原生可能刷新/重试/投影不变，此交付动作已结束，业务终态另由事件记录。"),
                *MetadataKind.ToString(),Sequence,Generation,*Id.ToString())));return;
        }
        else if(Op==TEXT("CancelRequest"))
        {
            const int64 Index=A.Int(TEXT("IssuedIndex"));
            if(Index<0 || Index>=IssuedRequests.Num()){Done(Context.Token,ManualError(TEXT("Demo.OnlineIssuedIndex"),TEXT("原生句柄索引不存在。")));return;}
            R=Online->Cancel(IssuedRequests[static_cast<int32>(Index)])?ManualObserved(TEXT("本原生请求取消已接纳；不表示远程写入回滚。")):ManualError(TEXT("Demo.OnlineCancelRejected"),TEXT("真实Online拒绝不存在或已终结句柄。"));
        }
        else if(Op==TEXT("SynchronizeParty")){Party->Synchronize();R=ManualObserved(TEXT("真实长期Party子系统已请求重同步；无认证时不会伪造快照。"));}
        else if(Op==TEXT("Inspect"))
        {
            R=ManualObserved(FString::Printf(TEXT("Auth=%d PendingProviderRequests=%d CachedProfile=%d；人工夹具事件=%llu"),
                int32(View->Auth.State),Provider->Requests.Num(),Online->GetCachedProfile().IsSet(),View->Events)+Provider->PendingText());
        }
        else
        {
            FGamePlatformOnlineRequestOptions Options;Options.Owner=Online.Get();TWeakPtr<FManualOnlineView> Weak(View);
            const auto Identity=MakeShared<FManualOnlineCompletionIdentity>();Identity->Kind=Op;Identity->Serial=++View->NextSerial;Identity->SourceGeneration=View->SourceGeneration;
            auto NativeComplete=[Weak,Identity](const auto& Result){if(auto V=Weak.Pin())
            {
                if(Result.Request.RequestId!=Identity->Handle.RequestId || Result.Request.InstanceScopeId!=Identity->Handle.InstanceScopeId)return;
                V->Finish(*Identity,Result.Result);
            }};
            if(Op==TEXT("Probe"))Request=Online->ProbeService(Options,NativeComplete);
            else if(Op==TEXT("GetProfile"))Request=Online->GetCurrentPlayerProfile(Options,NativeComplete);
            else if(Op==TEXT("UpdateProfile"))
            {
                FGamePlatformOnlineProfileUpdateRequest Update;Update.DisplayName=A.Id(TEXT("DisplayName")).ToString();Update.ExpectedRevision=A.Int(TEXT("Revision"));
                Update.IdempotencyKey=Context.Token.RunId.ToString(EGuidFormats::Digits);
                Request=Online->UpdateCurrentPlayerProfile(Update,Options,NativeComplete);
            }
            else if(Op==TEXT("ProtectedRead"))
            {
                FGamePlatformAuthenticatedRequest Read;Read.Verb=TEXT("GET");Read.RelativePath=TEXT("/v1/player/profile");
                Request=Online->SendAuthenticatedRequest(Read,Options,[Weak,Identity](const auto& Response){if(auto V=Weak.Pin())V->Finish(*Identity,Response.IsSuccess()?ManualObserved(TEXT("真实受控请求已收夹具成功响应。")):ManualError(TEXT("Demo.ProtectedResponseFailure"),TEXT("真实受控请求收到失败响应。")));});
            }
            else if(Op==TEXT("GetParty"))Request=Online->GetCurrentParty(Options,[Weak,Identity](const auto& Result){if(auto V=Weak.Pin())V->Finish(*Identity,Result.bSuccess?ManualObserved(TEXT("原生队伍解码成功；仅Port夹具。")):ManualError(TEXT("Demo.PartyResponseFailed"),TEXT("原生队伍响应失败。")));});
            else if(Op==TEXT("GetInvitations"))Request=Online->GetPartyInvitations(Options,[Weak,Identity](const auto& Result){if(auto V=Weak.Pin())V->Finish(*Identity,Result.bSuccess?ManualObserved(TEXT("原生邀请解码成功。")):ManualError(TEXT("Demo.InvitationResponseFailed"),TEXT("原生邀请响应失败。")));});
            else if(Op==TEXT("ObserveParty"))Request=Online->ObserveParty(TEXT(""),Options,[Weak,Identity](const auto& Result){if(auto V=Weak.Pin())V->Finish(*Identity,Result.bSuccess?ManualObserved(TEXT("原生队伍观察解码成功；非长驻Party子系统验收。")):ManualError(TEXT("Demo.ObserveResponseFailed"),TEXT("原生队伍观察响应失败。")));});
            else if(Op==TEXT("CommandParty"))
            {
                const TCHAR* Actions[]={TEXT("create"),TEXT("leave"),TEXT("invite"),TEXT("ready")};
                const int64 Choice=A.Int(TEXT("PartyAction"));
                if(Choice<0 || Choice>3){Done(Context.Token,ManualError(TEXT("Demo.PartyAction"),TEXT("动作只能create/leave/invite/ready的封闭选择。")));return;}
                FGamePlatformPartyCommand Command;Command.Action=Actions[Choice];Command.PartyId=TEXT("ManualParty");Command.TargetPlayerId=TEXT("ManualTarget");
                Command.ExpectedRevision=A.Int(TEXT("Revision"));Command.ClientRequestId=Context.Token.RunId.ToString(EGuidFormats::Digits);Command.bReady=A.Bool(TEXT("Success"));
                Request=Online->CommandParty(Command,Options,[Weak,Identity](const auto& Result){if(auto V=Weak.Pin())V->Finish(*Identity,Result.bSuccess?ManualObserved(TEXT("真实队伍命令夹具响应成功；未写远程队伍。")):ManualError(TEXT("Demo.PartyCommandFailed"),TEXT("真实队伍命令失败。")));});
            }
            else {Done(Context.Token,ManualError(TEXT("Demo.OnlineOperation"),TEXT("Online操作未在编译期白名单。")));return;}
            // Online公开门面保证后续GT终态；先记录完整原生句柄/类型/Serial，旧回调不能消费新请求。
            Identity->Handle=Request;
            if(Request.RequestId.IsValid()){IssuedRequests.Add(Request);View->Outstanding.Add(Request.RequestId,TPair<FName,uint64>(Op,Identity->Serial));}
            R=ManualObserved(TEXT("原生请求已提交；响应须按WireKind/WireSequence精确选择已派发请求，取消按IssuedIndex选择原生句柄，不代表服务就绪。"));
        }
        Done(Context.Token,R);
    }
    TWeakObjectPtr<UGameInstance> Host;
    TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> HostLease;
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TWeakObjectPtr<UGamePlatformPartyClientSubsystem> Party;
    FGamePlatformOnlineIdleProviderState PreviousProvider;FGamePlatformOnlineProviderRestoreLease ProviderRestoreLease;bool bCapturedProvider=false;
    TSharedPtr<FManualOnlineProvider> Provider;
    TSharedRef<FManualOnlineView> View;
    FGamePlatformOnlineRequestHandle Request;
    TArray<FGamePlatformOnlineRequestHandle> IssuedRequests;
    FDelegateHandle AuthChanged,PartyChanged;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformOnlineScenario(UWorld& W,FName F)
{return MakeUnique<FManualOnlineScenario>(W,F);}
}

