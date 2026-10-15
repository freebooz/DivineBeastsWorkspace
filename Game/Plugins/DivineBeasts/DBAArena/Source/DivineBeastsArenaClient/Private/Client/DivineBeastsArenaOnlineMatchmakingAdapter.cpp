// 真实匹配HTTP合同适配；Online拥有凭据/刷新/同源约束，本文只持有本人观察和逻辑命令代次，不实现匹配或旅行。
#include "Client/DivineBeastsArenaOnlineMatchmakingAdapter.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Party/GamePlatformPartyClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

namespace
{
bool String(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,FString& Out){return Object&&Object->HasTypedField<EJson::String>(Key)&&Object->TryGetStringField(Key,Out);}
bool Boolean(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,bool& Out){return Object&&Object->HasTypedField<EJson::Boolean>(Key)&&Object->TryGetBoolField(Key,Out);}
bool Identifier(const FString& Value){if(Value.IsEmpty()||Value.Len()>128||Value.TrimStartAndEnd()!=Value)return false;for(TCHAR C:Value)if(C<32||C==127)return false;return true;}
bool IntegerString(const FString& Text,uint64 Limit,uint64& Out)
{if(Text.IsEmpty()||Text.Len()>20||(Text.Len()>1&&Text[0]=='0'))return false;uint64 Value=0;for(TCHAR C:Text){if(C<'0'||C>'9'||Value>(Limit-(C-'0'))/10)return false;Value=Value*10+(C-'0');}Out=Value;return true;}
bool Revision(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,int64& Out){FString Text;uint64 Number=0;if(!String(Object,Key,Text)||!IntegerString(Text,MAX_int64,Number))return false;Out=static_cast<int64>(Number);return true;}
bool Time(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,FDateTime& Out){FString Text;return String(Object,Key,Text)&&Text.Len()<=40&&Text.EndsWith(TEXT("Z"))&&FDateTime::ParseIso8601(*Text,Out)&&Out.GetTicks()>0;}
bool Parse(const FString& Body,TSharedPtr<FJsonObject>& Out){return Body.Len()<=65536&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body),Out)&&Out.IsValid();}
bool CursorValid(const FString& Cursor){TArray<FString> Parts;Cursor.ParseIntoArray(Parts,TEXT("-"),false);uint64 Value=0;return Parts.Num()==2&&IntegerString(Parts[0],MAX_uint64,Value)&&IntegerString(Parts[1],MAX_uint64,Value);}
bool Number(const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,int64& Out)
{double Value=0;if(!Object||!Object->HasTypedField<EJson::Number>(Key)||!Object->TryGetNumberField(Key,Value)||!FMath::IsFinite(Value)||Value<=0||Value>9007199254740991.0||FMath::FloorToDouble(Value)!=Value)return false;Out=static_cast<int64>(Value);return true;}
FGamePlatformAuthenticatedRequest Request(const TCHAR* Path,TSharedRef<FJsonObject> Body,const FString& Id)
{FGamePlatformAuthenticatedRequest Value;Value.Verb=TEXT("POST");Value.RelativePath=Path;Value.bIdempotent=true;Value.IdempotencyKey=Id;FJsonSerializer::Serialize(Body,TJsonWriterFactory<>::Create(&Value.Body));return Value;}
}

FDivineBeastsArenaOnlineMatchmakingAdapter::FDivineBeastsArenaOnlineMatchmakingAdapter(UGamePlatformOnlineClientSubsystem& InOnline,UObject& InOwner):Online(&InOnline),Owner(&InOwner){}
FDivineBeastsArenaOnlineMatchmakingAdapter::~FDivineBeastsArenaOnlineMatchmakingAdapter(){Stop();}
void FDivineBeastsArenaOnlineMatchmakingAdapter::Start()
{
    if(bStarted)return;bStarted=true;if(auto* Service=Online.Get())
    {const TWeakPtr<FDivineBeastsArenaOnlineMatchmakingAdapter> Weak=AsShared();AuthHandle=Service->OnAuthStateChanged().AddLambda([Weak](const FGamePlatformAuthSnapshot& State){if(auto Self=Weak.Pin())Self->AuthenticationChanged(State);});AuthenticationChanged(Service->GetSnapshot());}
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::StopRequests()
{
    ++Generation;const auto OldObservation=Observation;Observation={};auto OldCommands=MoveTemp(Commands);Commands.Reset();
    if(RetryHandle.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(RetryHandle);RetryHandle.Reset();}
    if(auto* Service=Online.Get()){Service->Cancel(OldObservation);for(const auto& Handle:OldCommands)Service->Cancel(Handle);}
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::Stop()
{bStarted=false;StopRequests();if(auto* Service=Online.Get())Service->OnAuthStateChanged().Remove(AuthHandle);AuthHandle.Reset();Snapshot={};Changed.Clear();}
void FDivineBeastsArenaOnlineMatchmakingAdapter::Resynchronize()
{
    if(!bStarted||PlayerId.IsEmpty())return;
    StopRequests();Cursor.Empty();Failures=0;Observe(Generation);
}
bool FDivineBeastsArenaOnlineMatchmakingAdapter::DecodeCommandResponse(const FString& Body,const FString& Id,FDivineBeastsArenaQueueSnapshot& Out)const
{
    TSharedPtr<FJsonObject> Object;const TSharedPtr<FJsonObject>* View=nullptr;FString Echo;
    return Parse(Body,Object)&&String(Object,TEXT("clientRequestId"),Echo)&&Echo==Id&&Object->TryGetObjectField(TEXT("snapshot"),View)&&DecodeSnapshot(*View,Out);
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::AuthenticationChanged(const FGamePlatformAuthSnapshot& State)
{
    const auto* Service=Online.Get();const bool bAuthenticated=State.State==EGamePlatformAuthState::Authenticated||State.State==EGamePlatformAuthState::Refreshing;
    const FString NewPlayer=bAuthenticated&&Service?Service->GetAuthentication().PlayerId:FString();
    if(AuthGeneration==State.AuthGeneration&&PlayerId==NewPlayer)return;
    StopRequests();AuthGeneration=State.AuthGeneration;PlayerId=NewPlayer;Cursor.Empty();Snapshot={};Failures=0;CancelRequestId.Empty();CancelTicketId.Empty();Changed.Broadcast();
    if(bStarted&&!PlayerId.IsEmpty())Observe(Generation);
}
bool FDivineBeastsArenaOnlineMatchmakingAdapter::DecodeSnapshot(const TSharedPtr<FJsonObject>& Object,FDivineBeastsArenaQueueSnapshot& Out)const
{
    FDivineBeastsArenaQueueSnapshot Next;bool bCanReady=false;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if(!String(Object,TEXT("state"),Next.Phase)||!String(Object,TEXT("ticketId"),Next.TicketId)||!String(Object,TEXT("partyId"),Next.PartyId)||!String(Object,TEXT("candidateId"),Next.CandidateId)||!String(Object,TEXT("arenaModeId"),Next.ModeId)||!String(Object,TEXT("regionId"),Next.RegionId)||!String(Object,TEXT("reasonCode"),Next.ReasonCode)||
        !Revision(Object,TEXT("revision"),Next.Revision)||!Revision(Object,TEXT("candidateRevision"),Next.CandidateRevision)||!Time(Object,TEXT("serverTimeUtc"),Next.ServerTimeUtc)||!Boolean(Object,TEXT("serviceReady"),Next.bServiceReady)||!Boolean(Object,TEXT("development"),Next.bDevelopment)||!Boolean(Object,TEXT("canCancel"),Next.bCanCancel)||!Boolean(Object,TEXT("canReply"),Next.bCanReply)||!Boolean(Object,TEXT("canReady"),bCanReady)||!Object->TryGetArrayField(TEXT("participants"),Rows)||Rows->Num()>10||!Object->HasField(TEXT("deadlineUtc")))return false;
    const TSet<FString> Phases={TEXT("idle"),TEXT("readyCheck"),TEXT("searching"),TEXT("confirmation"),TEXT("allocating"),TEXT("transferReady"),TEXT("inMatch"),TEXT("returning"),TEXT("completed"),TEXT("cancelled"),TEXT("failed")};
    if(!Phases.Contains(Next.Phase)||(!Next.TicketId.IsEmpty()&&!Identifier(Next.TicketId))||(!Next.CandidateId.IsEmpty()&&!Identifier(Next.CandidateId))||Next.RegionId.Len()>128)return false;
    if(!Object->TryGetField(TEXT("deadlineUtc"))->IsNull()&&!Time(Object,TEXT("deadlineUtc"),Next.DeadlineUtc))return false;
    TSet<FString> Players;bool bContainsSelf=false;
    for(const auto& Value:*Rows){const TSharedPtr<FJsonObject>* Row=nullptr;FString Player,Team;bool bAccepted=false,bReady=false;if(!Value||!Value->TryGetObject(Row)||!String(*Row,TEXT("playerId"),Player)||!Identifier(Player)||Players.Contains(Player)||!String(*Row,TEXT("teamId"),Team)||Team.Len()>128||!Boolean(*Row,TEXT("accepted"),bAccepted)||!Boolean(*Row,TEXT("ready"),bReady))return false;Players.Add(Player);bContainsSelf|=Player==PlayerId;Next.Accepted+=bAccepted?1:0;}
    if((Next.Phase!=TEXT("idle")&&!bContainsSelf)||(Next.bCanReply&&(Next.Phase!=TEXT("confirmation")||Next.CandidateId.IsEmpty()||Next.DeadlineUtc.GetTicks()==0))||(Next.bCanCancel&&Next.TicketId.IsEmpty()))return false;
    Next.Participants=Rows->Num();Next.bKnown=true;Out=MoveTemp(Next);return true;
}
bool FDivineBeastsArenaOnlineMatchmakingAdapter::ApplySnapshot(FDivineBeastsArenaQueueSnapshot Next)
{
    if(Snapshot.bKnown&&Snapshot.TicketId==Next.TicketId&&!Next.TicketId.IsEmpty()&&Next.Revision<Snapshot.Revision)return true;
    if(Snapshot.TicketId!=Next.TicketId){CancelTicketId.Empty();CancelRequestId.Empty();}
    Snapshot=MoveTemp(Next);Changed.Broadcast();return true;
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::Observe(uint64 Expected)
{
    auto* Service=Online.Get();if(!bStarted||!Service||!Owner.IsValid()||PlayerId.IsEmpty()||Expected!=Generation||Observation.RequestId.IsValid())return;
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("cursor"),Cursor);Body->SetNumberField(TEXT("waitMilliseconds"),Cursor.IsEmpty()?0:8000);
    FGamePlatformOnlineRequestOptions Options;Options.Owner=Owner.Get();const TWeakPtr<FDivineBeastsArenaOnlineMatchmakingAdapter> Weak=AsShared();
    Observation=Service->SendAuthenticatedRequest(Request(TEXT("/v1/matchmaking/observe"),Body,FGuid::NewGuid().ToString()),Options,[Weak,Expected](FGamePlatformAuthenticatedResponse Response)
    {
        auto Self=Weak.Pin();if(!Self||!Self->bStarted||Self->Generation!=Expected)return;Self->Observation={};TSharedPtr<FJsonObject> Object;const TSharedPtr<FJsonObject>* View=nullptr;FString Scope,NextCursor;bool bChanged=false,bResync=false;FDivineBeastsArenaQueueSnapshot Next;
        if(Response.IsSuccess()&&Parse(Response.Body,Object)&&String(Object,TEXT("scopeId"),Scope)&&Scope==TEXT("matchmaking/")+Self->PlayerId&&String(Object,TEXT("cursor"),NextCursor)&&CursorValid(NextCursor)&&Boolean(Object,TEXT("changed"),bChanged)&&Boolean(Object,TEXT("resyncRequired"),bResync)&&Object->TryGetObjectField(TEXT("snapshot"),View)&&Self->DecodeSnapshot(*View,Next))
        {
            const bool bNotify=!Self->Snapshot.bKnown||bChanged||bResync;Self->Cursor=NextCursor;Self->Failures=0;if(bNotify)Self->ApplySnapshot(MoveTemp(Next));
            AsyncTask(ENamedThreads::GameThread,[Weak,Expected](){if(auto Current=Weak.Pin())Current->Observe(Expected);});return;
        }
        Self->Snapshot.bKnown=false;Self->Changed.Broadcast();
        // 仅可恢复网络/限流故障最多四次一次性退避；非法响应/权限失败停住，不能用持续业务轮询掩盖故障。
        if(Self->Generation!=Expected||Response.IsSuccess()||++Self->Failures>4||(Response.HttpStatusCode>0&&Response.HttpStatusCode<500&&Response.HttpStatusCode!=429))return;
        Self->RetryHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak,Expected](float){if(auto Current=Weak.Pin())if(Current->Generation==Expected){Current->RetryHandle.Reset();Current->Observe(Expected);}return false;}),static_cast<float>(1<<(Self->Failures-1)));
    });
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::SendCommand(TSharedRef<FJsonObject> Body,const FString& Id,TFunction<void(bool,const FString&)> Done)
{
    auto* Service=Online.Get();if(!bStarted||!Service||!Owner.IsValid()||PlayerId.IsEmpty()||!Identifier(Id)){Done(false,TEXT("Matchmaking.Unavailable"));return;}
    FGamePlatformOnlineRequestOptions Options;Options.Owner=Owner.Get();const uint64 Expected=Generation;const TWeakPtr<FDivineBeastsArenaOnlineMatchmakingAdapter> Weak=AsShared();const auto Handle=MakeShared<FGamePlatformOnlineRequestHandle>();
    *Handle=Service->SendAuthenticatedRequest(Request(TEXT("/v1/matchmaking/commands"),Body,Id),Options,[Weak,Expected,Handle,Id,Done=MoveTemp(Done)](FGamePlatformAuthenticatedResponse Response)mutable
    {auto Self=Weak.Pin();if(!Self||Self->Generation!=Expected||!Self->bStarted)return;Self->Commands.RemoveAll([&](const auto& Item){return Item.RequestId==Handle->RequestId;});TSharedPtr<FJsonObject> Object;FDivineBeastsArenaQueueSnapshot Next;
        const bool bAccepted=Response.IsSuccess()&&Self->DecodeCommandResponse(Response.Body,Id,Next);
        if(bAccepted)Self->ApplySnapshot(MoveTemp(Next));
        if(Self->Generation!=Expected||!Self->bStarted)return;
        const bool bRejected=Response.HttpStatusCode>=400&&Response.HttpStatusCode<500&&Response.HttpStatusCode!=408&&Response.HttpStatusCode!=429;
        if(!bAccepted&&bRejected&&Id==Self->CancelRequestId){Self->CancelTicketId.Empty();Self->CancelRequestId.Empty();}
        Done(bAccepted,bAccepted?FString():bRejected?TEXT("Matchmaking.RequestRejected"):TEXT("Matchmaking.CommandUnconfirmed"));
        if(!bAccepted&&Self->Generation==Expected&&Self->bStarted)Self->Resynchronize();});
    if(Generation==Expected)Commands.Add(*Handle);else Service->Cancel(*Handle);
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::RequestMatchmaking(const FGamePlatformArenaMatchmakingRequest& Input,TFunction<void(bool,const FString&)> Done)
{
    if(!SupportsTicketLifecycle()||!Snapshot.bServiceReady||!(Snapshot.Phase==TEXT("idle")||Snapshot.Phase==TEXT("completed")||Snapshot.Phase==TEXT("cancelled")||Snapshot.Phase==TEXT("failed"))){Done(false,TEXT("Matchmaking.StateUnavailable"));return;}
    auto* Service=Online.Get();auto* Party=Service&&Service->GetGameInstance()?Service->GetGameInstance()->GetSubsystem<UGamePlatformPartyClientSubsystem>():nullptr;
    if(!Party||!Party->GetSnapshot().bSuccess||Party->GetAuthenticationGeneration()!=AuthGeneration){Done(false,TEXT("Matchmaking.PartyUnknown"));return;}
    const auto& PartyView=Party->GetSnapshot().Snapshot;
    if((PartyView.bHasParty&&PartyView.PartyId!=Input.PartyId)||(!PartyView.bHasParty&&!Input.PartyId.IsEmpty())){Done(false,TEXT("Matchmaking.PartyChanged"));return;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("action"),TEXT("start"));Body->SetStringField(TEXT("clientRequestId"),Input.ClientRequestId);Body->SetStringField(TEXT("expectedRevision"),LexToString(PartyView.bHasParty?PartyView.Revision:0));Body->SetStringField(TEXT("partyId"),Input.PartyId);Body->SetStringField(TEXT("arenaModeId"),Input.ArenaModeId.ToString());Body->SetStringField(TEXT("regionId"),Input.PreferredRegion.IsEmpty()?Snapshot.RegionId:Input.PreferredRegion);SendCommand(Body,Input.ClientRequestId,MoveTemp(Done));
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::CancelMatchmaking(const FString&,TFunction<void(bool,const FString&)> Done)
{
    if(!Snapshot.bKnown||!Snapshot.bCanCancel){Done(false,TEXT("Matchmaking.CancelUnavailable"));return;}
    if(CancelTicketId!=Snapshot.TicketId){CancelTicketId=Snapshot.TicketId;CancelRequestId=FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);CancelRevision=Snapshot.Revision;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("action"),TEXT("cancel"));Body->SetStringField(TEXT("clientRequestId"),CancelRequestId);Body->SetStringField(TEXT("ticketId"),Snapshot.TicketId);Body->SetStringField(TEXT("expectedRevision"),LexToString(CancelRevision));SendCommand(Body,CancelRequestId,MoveTemp(Done));
}
void FDivineBeastsArenaOnlineMatchmakingAdapter::RespondToMatchOffer(const FGamePlatformArenaMatchOffer& Offer,bool bAccept,const FString& Id,TFunction<void(bool,const FString&)> Done)
{
    if(!Snapshot.bKnown||!Snapshot.bCanReply||Offer.CandidateId!=Snapshot.CandidateId||Offer.Revision!=Snapshot.CandidateRevision){Done(false,TEXT("Matchmaking.OfferChanged"));return;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("action"),TEXT("reply"));Body->SetStringField(TEXT("clientRequestId"),Id);Body->SetStringField(TEXT("candidateId"),Offer.CandidateId);Body->SetStringField(TEXT("expectedRevision"),LexToString(Offer.Revision));Body->SetBoolField(TEXT("accept"),bAccept);SendCommand(Body,Id,MoveTemp(Done));
}
bool FDivineBeastsArenaOnlineMatchmakingAdapter::SupportsTicketLifecycle()const{return bStarted&&Snapshot.bKnown&&!PlayerId.IsEmpty()&&(Snapshot.bServiceReady||!Snapshot.TicketId.IsEmpty());}
EGamePlatformArenaClientFlowState FDivineBeastsArenaOnlineMatchmakingAdapter::GetObservedState()const
{
    if(!Snapshot.bKnown)return EGamePlatformArenaClientFlowState::Failed;
    if(Snapshot.Phase==TEXT("readyCheck")||Snapshot.Phase==TEXT("searching"))return EGamePlatformArenaClientFlowState::Matchmaking;
    if(Snapshot.Phase==TEXT("confirmation"))return EGamePlatformArenaClientFlowState::MatchFound;
    if(Snapshot.Phase==TEXT("allocating")||Snapshot.Phase==TEXT("transferReady"))return EGamePlatformArenaClientFlowState::Transferring;
    if(Snapshot.Phase==TEXT("inMatch"))return EGamePlatformArenaClientFlowState::InMatch;
    if(Snapshot.Phase==TEXT("returning"))return EGamePlatformArenaClientFlowState::Result;
    return EGamePlatformArenaClientFlowState::Idle;
}
FGamePlatformArenaMatchOffer FDivineBeastsArenaOnlineMatchmakingAdapter::GetObservedOffer()const
{FGamePlatformArenaMatchOffer Offer;if(Snapshot.bKnown){Offer.CandidateId=Snapshot.CandidateId;Offer.Revision=Snapshot.CandidateRevision;Offer.DeadlineUtc=Snapshot.DeadlineUtc;Offer.bCanRespond=Snapshot.bCanReply;}return Offer;}
void FDivineBeastsArenaOnlineMatchmakingAdapter::RequestMigration(const FString& Direction,const FString& Id,TFunction<void(bool,FDivineBeastsWorldAssignmentPayload)> Done)
{
    auto* Service=Online.Get();if(!Service||!Owner.IsValid()||!Snapshot.bKnown||Snapshot.CandidateId.IsEmpty()||!Identifier(Id)||(Direction!=TEXT("arena")&&Direction!=TEXT("return"))){Done(false,{});return;}
    auto Body=MakeShared<FJsonObject>();Body->SetStringField(TEXT("candidateId"),Snapshot.CandidateId);Body->SetStringField(TEXT("direction"),Direction);Body->SetStringField(TEXT("clientRequestId"),Id);
    FGamePlatformOnlineRequestOptions Options;Options.Owner=Owner.Get();const uint64 Expected=Generation;const TWeakPtr<FDivineBeastsArenaOnlineMatchmakingAdapter> Weak=AsShared();const auto Handle=MakeShared<FGamePlatformOnlineRequestHandle>();
    *Handle=Service->SendAuthenticatedRequest(Request(TEXT("/v1/matchmaking/migration"),Body,Id),Options,[Weak,Expected,Handle,Done=MoveTemp(Done)](FGamePlatformAuthenticatedResponse Response)mutable
    {
        auto Self=Weak.Pin();if(!Self||Self->Generation!=Expected||!Self->bStarted)return;Self->Commands.RemoveAll([&](const auto& Item){return Item.RequestId==Handle->RequestId;});
        FDivineBeastsWorldAssignmentPayload Value;TSharedPtr<FJsonObject> Object;FString Role,Experience,World,Map,Region;int64 Protocol=0,Epoch=0;
        if(!Response.IsSuccess()||!Parse(Response.Body,Object)||!String(Object,TEXT("assignmentId"),Value.Summary.AssignmentId)||!Identifier(Value.Summary.AssignmentId)||!String(Object,TEXT("gameServerId"),Value.Summary.GameServerId)||!Identifier(Value.Summary.GameServerId)||!String(Object,TEXT("serverRoleId"),Role)||!String(Object,TEXT("experienceId"),Experience)||!String(Object,TEXT("worldId"),World)||!String(Object,TEXT("mapId"),Map)||!String(Object,TEXT("regionId"),Region)||!String(Object,TEXT("ticketId"),Value.Summary.TicketId)||!String(Object,TEXT("characterId"),Value.Summary.CharacterId)||!String(Object,TEXT("sessionId"),Value.Summary.SessionId)||!String(Object,TEXT("gameSessionId"),Value.Summary.GameSessionId)||!String(Object,TEXT("serverBootId"),Value.Summary.ServerBootId)||!Number(Object,TEXT("protocolVersion"),Protocol)||Protocol>MAX_uint32||!Number(Object,TEXT("sessionEpoch"),Epoch)||!String(Object,TEXT("endpoint"),Value.Endpoint)||Value.Endpoint.IsEmpty()||!String(Object,TEXT("transferTicket"),Value.TransferTicket)||Value.TransferTicket.IsEmpty())
        {Done(false,{});return;}
        Value.Summary.RegionId=FName(*Region);Value.Summary.ServerRoleId=FName(*Role);Value.Summary.ExperienceId=FName(*Experience);Value.Summary.WorldId=FName(*World);Value.Summary.MapId=FName(*Map);Value.Summary.ProtocolVersion=LexToString(Protocol);Value.Summary.SessionEpoch=Epoch;Done(true,MoveTemp(Value));
    });
    if(Generation==Expected)Commands.Add(*Handle);else Service->Cancel(*Handle);
}
