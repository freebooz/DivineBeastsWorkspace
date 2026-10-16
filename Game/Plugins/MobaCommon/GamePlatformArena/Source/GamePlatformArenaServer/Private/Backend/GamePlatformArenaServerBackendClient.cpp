#include "Backend/GamePlatformArenaServerBackendClient.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    TSharedPtr<FJsonObject> ParseJsonObject(const FString& Body)
    {
        TSharedPtr<FJsonObject> Object;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
        return FJsonSerializer::Deserialize(Reader, Object) ? Object : nullptr;
    }

    // 消费Shared控制面已经发布的PascalCase响应；先完整验证临时值，不读取不存在的snake_case旁路。
    bool ParseAssignment(const FString& Body,FGamePlatformArenaAssignment& Out)
    {
        const auto Root=ParseJsonObject(Body);const TSharedPtr<FJsonObject>* Arena=nullptr;
        auto String=[](const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,FString& Value){return Object&&Object->HasTypedField<EJson::String>(Key)&&Object->TryGetStringField(Key,Value);};
        auto Integer=[](const TSharedPtr<FJsonObject>& Object,const TCHAR* Key,int32& Value){double Number=0;if(!Object||!Object->HasTypedField<EJson::Number>(Key)||!Object->TryGetNumberField(Key,Number)||!FMath::IsFinite(Number)||Number<0||Number>MAX_int32||FMath::FloorToDouble(Number)!=Number)return false;Value=static_cast<int32>(Number);return true;};
        if(!Root||!Root->TryGetObjectField(TEXT("Arena"),Arena)||!Arena||!Arena->IsValid())return false;
        FGamePlatformArenaAssignment Next;FString Mode,Map,Role,Experience;const TArray<TSharedPtr<FJsonValue>>* Roster=nullptr;
        if(!String(*Arena,TEXT("MatchID"),Next.MatchId)||!String(*Arena,TEXT("ArenaModeID"),Mode)||!String(*Arena,TEXT("MapID"),Map)||!String(Root,TEXT("ServerRoleID"),Role)||!String(Root,TEXT("ExperienceID"),Experience)||
            !String(*Arena,TEXT("GameServerID"),Next.GameServerId)||!String(*Arena,TEXT("ContentRevision"),Next.ContentRevision)||!Integer(*Arena,TEXT("ProjectRuleRevision"),Next.ProjectRuleRevision)||!Integer(*Arena,TEXT("HeroCatalogRevision"),Next.HeroCatalogRevision)||!Integer(*Arena,TEXT("TeamSize"),Next.TeamSize)||!Integer(*Arena,TEXT("TotalPlayers"),Next.TotalPlayers)||!(*Arena)->TryGetArrayField(TEXT("Roster"),Roster)||Roster->Num()>10)return false;
        Next.ArenaModeId=FName(*Mode);Next.MapId=FName(*Map);Next.ServerRole=FName(*Role);Next.ExperienceId=FName(*Experience);
        for(const auto& Value:*Roster)
        {
            const TSharedPtr<FJsonObject>* Row=nullptr;FGamePlatformArenaRosterSlot Slot;FString Team;
            if(!Value||!Value->TryGetObject(Row)||!String(*Row,TEXT("PlayerID"),Slot.PlayerId)||!String(*Row,TEXT("CharacterID"),Slot.CharacterId)||!String(*Row,TEXT("TeamID"),Team)||!String(*Row,TEXT("PartyID"),Slot.PartyId)||!String(*Row,TEXT("HeroDefinitionID"),Slot.HeroDefinitionId)||!Integer(*Row,TEXT("SlotIndex"),Slot.SlotIndex))return false;
            Slot.TeamId=FName(*Team);if(!Slot.IsValid()||Slot.HeroDefinitionId.IsEmpty())return false;Next.Roster.Add(MoveTemp(Slot));
        }
        if(Next.MatchId.IsEmpty()||Next.GameServerId.IsEmpty()||Next.ProjectRuleRevision<1||Next.ContentRevision.IsEmpty()||Next.HeroCatalogRevision<1||Next.TeamSize<1||Next.TeamSize>5||Next.TotalPlayers!=Next.TeamSize*2||Next.Roster.Num()!=Next.TotalPlayers)return false;
        Out=MoveTemp(Next);return true;
    }

    TSharedRef<FJsonObject> ResultToJson(const FGamePlatformArenaMatchResult& Result)
    {
        auto Root=MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("MatchID"),Result.MatchId);Root->SetStringField(TEXT("ArenaModeID"),Result.ArenaModeId.ToString());Root->SetStringField(TEXT("GameServerID"),Result.GameServerId);
        Root->SetStringField(TEXT("StartedAt"),Result.StartedAtUtc.ToIso8601());Root->SetStringField(TEXT("EndedAt"),Result.EndedAtUtc.ToIso8601());
        Root->SetStringField(TEXT("WinningTeamID"),Result.WinningTeamId.IsNone()?FString():Result.WinningTeamId.ToString());
        Root->SetStringField(TEXT("EndReason"),StaticEnum<EGamePlatformArenaMatchEndReason>()->GetNameStringByValue(static_cast<int64>(Result.EndReason)));Root->SetNumberField(TEXT("EndRevision"),Result.EndRevision);
        TArray<TSharedPtr<FJsonValue>> Teams,Players;
        for(const auto& Team:Result.Teams)
        {
            auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("TeamID"),Team.TeamId.ToString());Row->SetBoolField(TEXT("Won"),!Result.WinningTeamId.IsNone()&&Team.TeamId==Result.WinningTeamId);Row->SetNumberField(TEXT("Score"),Team.Score);Teams.Add(MakeShared<FJsonValueObject>(Row));
            for(const auto& Player:Team.Players)
            {
                auto Item=MakeShared<FJsonObject>();Item->SetStringField(TEXT("PlayerID"),Player.PlayerId);Item->SetStringField(TEXT("TeamID"),Player.TeamId.ToString());Item->SetStringField(TEXT("CharacterID"),Player.CharacterId);
                Item->SetNumberField(TEXT("Kills"),Player.Kills);Item->SetNumberField(TEXT("Deaths"),Player.Deaths);Item->SetNumberField(TEXT("Assists"),Player.Assists);Item->SetNumberField(TEXT("Score"),Player.Score);Players.Add(MakeShared<FJsonValueObject>(Item));
            }
        }
        Root->SetArrayField(TEXT("Teams"),Teams);Root->SetArrayField(TEXT("Players"),Players);return Root;
    }

    FString SerializeJson(const TSharedRef<FJsonObject>& Object)
    {
        FString Body;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
        FJsonSerializer::Serialize(Object, Writer);
        return Body;
    }
}

FGamePlatformArenaServerBackendClient::FGamePlatformArenaServerBackendClient(
    FGamePlatformArenaServerBackendConfig InConfig)
    : Config(MoveTemp(InConfig))
{
    Config.BaseUrl.RemoveFromEnd(TEXT("/"));
}

FString FGamePlatformArenaServerBackendClient::MakeUrl(const FString& RelativePath) const
{
    return Config.BaseUrl + RelativePath;
}

void FGamePlatformArenaServerBackendClient::ApplyCommonHeaders(
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe>& Request) const
{
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Config.InternalToken);
    Request->SetHeader(TEXT("X-Game-Server-Id"), Config.GameServerId);
    Request->SetHeader(TEXT("X-Game-Server-Boot-Id"),Config.ServerBootId);
    Request->SetTimeout(25.f);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
}

void FGamePlatformArenaServerBackendClient::GetAssignment(
    TFunction<void(bool, const FGamePlatformArenaAssignment&, const FString&)> InCompletion)
{
    const auto Completed=MakeShared<bool>(false);
    auto Completion=[Completed,Done=MoveTemp(InCompletion)](bool bSuccess,const FGamePlatformArenaAssignment& Assignment,const FString& Error)mutable
    {if(*Completed)return;*Completed=true;Done(bSuccess,Assignment,Error);};
    FGamePlatformArenaAssignment Empty;
    if (!Config.IsValid()) { Completion(false, Empty, TEXT("Arena backend config invalid.")); return; }
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("GET"));
    Request->SetURL(MakeUrl(TEXT("/internal/v1/gameservers/assignment?waitMilliseconds=20000&gameServerId=")+FGenericPlatformHttp::UrlEncode(Config.GameServerId)));
    ApplyCommonHeaders(Request);
    Request->OnProcessRequestComplete().BindLambda(
        [Completion](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded) mutable
        {
            FGamePlatformArenaAssignment Assignment;
            if (!bSucceeded || !Response.IsValid()) { Completion(false, Assignment, TEXT("Assignment request transport failure.")); return; }
            if(Response->GetResponseCode()==202){Completion(false,Assignment,TEXT("AssignmentPending"));return;}
            if (Response->GetResponseCode() != 200 || !ParseAssignment(Response->GetContentAsString(), Assignment))
            {
                Completion(false, Assignment, FString::Printf(TEXT("Assignment request rejected: HTTP %d"), Response->GetResponseCode()));
                return;
            }
            Completion(true, Assignment, FString());
        });
    TrackRequest(Request);
    if (!Request->ProcessRequest()) { Requests.Remove(Request);Completion(false, Empty, TEXT("Assignment request did not start.")); }
}

void FGamePlatformArenaServerBackendClient::SubmitMatchResult(
    const FGamePlatformArenaMatchResult& Result,
    TFunction<void(EGamePlatformArenaSubmitStatus, const FString&)> InCompletion)
{
    const auto Completed=MakeShared<bool>(false);
    auto Completion=[Completed,Done=MoveTemp(InCompletion)](EGamePlatformArenaSubmitStatus Status,const FString& Error)mutable
    {if(*Completed)return;*Completed=true;Done(Status,Error);};
    if (!Config.IsValid() || Result.MatchId.IsEmpty())
    {
        Completion(EGamePlatformArenaSubmitStatus::TerminalFailure, TEXT("MatchResult or backend config invalid."));
        return;
    }
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("POST"));
    Request->SetURL(MakeUrl(TEXT("/internal/v1/gameservers/match-result")));
    ApplyCommonHeaders(Request);
    Request->SetContentAsString(SerializeJson(ResultToJson(Result)));
    Request->OnProcessRequestComplete().BindLambda(
        [Completion](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded) mutable
        {
            if (!bSucceeded || !Response.IsValid())
            {
                Completion(EGamePlatformArenaSubmitStatus::RetryableFailure, TEXT("MatchResult transport outcome unknown."));
                return;
            }
            const int32 Code = Response->GetResponseCode();
            if (Code == 409) { Completion(EGamePlatformArenaSubmitStatus::Conflict, TEXT("MatchResult conflict.")); return; }
            if (Code >= 500) { Completion(EGamePlatformArenaSubmitStatus::RetryableFailure, FString::Printf(TEXT("Backend HTTP %d"), Code)); return; }
            if (Code != 200) { Completion(EGamePlatformArenaSubmitStatus::TerminalFailure, FString::Printf(TEXT("Backend HTTP %d"), Code)); return; }

            const TSharedPtr<FJsonObject> Json = ParseJsonObject(Response->GetContentAsString());
            FString ResultId;
            if(!Json||!Json->HasTypedField<EJson::String>(TEXT("ResultID"))||!Json->TryGetStringField(TEXT("ResultID"),ResultId)||ResultId.IsEmpty())
            {Completion(EGamePlatformArenaSubmitStatus::RetryableFailure,TEXT("MatchResult response malformed; outcome unknown."));return;}
            Completion(EGamePlatformArenaSubmitStatus::Committed,FString());
        });
    TrackRequest(Request);
    if (!Request->ProcessRequest()) { Requests.Remove(Request);Completion(EGamePlatformArenaSubmitStatus::RetryableFailure, TEXT("MatchResult request did not start.")); }
}

void FGamePlatformArenaServerBackendClient::CancelAll()
{
    auto Owned=MoveTemp(Requests);Requests.Reset();
    for(const auto& Request:Owned)if(Request){Request->OnProcessRequestComplete().Unbind();Request->CancelRequest();}
}

void FGamePlatformArenaServerBackendClient::TrackRequest(const TSharedRef<IHttpRequest,ESPMode::ThreadSafe>& Request)
{
    const auto Original=Request->OnProcessRequestComplete();
    const TWeakPtr<FGamePlatformArenaServerBackendClient> Weak=AsShared();
    Request->OnProcessRequestComplete().BindLambda([Weak,Original](FHttpRequestPtr Completed,FHttpResponsePtr Response,bool bSuccess)
    {if(auto Self=Weak.Pin())Self->Requests.Remove(Completed);Original.ExecuteIfBound(Completed,Response,bSuccess);});
    Requests.Add(Request);
}
