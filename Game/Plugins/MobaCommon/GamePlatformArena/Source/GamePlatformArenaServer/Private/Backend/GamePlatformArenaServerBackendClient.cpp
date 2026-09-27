#include "Backend/GamePlatformArenaServerBackendClient.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
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

    bool ParseAssignment(const FString& Body, FGamePlatformArenaAssignment& OutAssignment)
    {
        const TSharedPtr<FJsonObject> Json = ParseJsonObject(Body);
        if (!Json.IsValid()) { return false; }
        OutAssignment.MatchId = Json->GetStringField(TEXT("match_id"));
        OutAssignment.ArenaModeId = FName(*Json->GetStringField(TEXT("arena_mode_id")));
        OutAssignment.MapId = FName(*Json->GetStringField(TEXT("map_id")));
        OutAssignment.ServerRole = FName(*Json->GetStringField(TEXT("server_role")));
        OutAssignment.ExperienceId = FName(*Json->GetStringField(TEXT("experience_id")));
        OutAssignment.ProjectRuleRevision = static_cast<int32>(Json->GetNumberField(TEXT("project_rule_revision")));
        Json->TryGetStringField(TEXT("content_revision"), OutAssignment.ContentRevision);
        OutAssignment.HeroCatalogRevision = static_cast<int32>(Json->GetNumberField(TEXT("hero_catalog_revision")));
        OutAssignment.GameServerId = Json->GetStringField(TEXT("game_server_id"));
        OutAssignment.TeamSize = static_cast<int32>(Json->GetNumberField(TEXT("team_size")));
        OutAssignment.TotalPlayers = static_cast<int32>(Json->GetNumberField(TEXT("total_players")));
        OutAssignment.Roster.Reset();
        for (const TSharedPtr<FJsonValue>& Value : Json->GetArrayField(TEXT("roster")))
        {
            const TSharedPtr<FJsonObject> Row = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Row.IsValid()) { return false; }
            FGamePlatformArenaRosterSlot Slot;
            Slot.PlayerId = Row->GetStringField(TEXT("player_id"));
            Slot.CharacterId = Row->GetStringField(TEXT("character_id"));
            Slot.TeamId = FName(*Row->GetStringField(TEXT("team_id")));
            Row->TryGetStringField(TEXT("party_id"), Slot.PartyId);
            Slot.SlotIndex = static_cast<int32>(Row->GetNumberField(TEXT("slot_index")));
            OutAssignment.Roster.Add(MoveTemp(Slot));
        }
        return !OutAssignment.MatchId.IsEmpty() && !OutAssignment.GameServerId.IsEmpty();
    }

    bool ParseTicketClaims(const FString& Body, FGamePlatformArenaTransferTicketClaims& OutClaims)
    {
        const TSharedPtr<FJsonObject> Json = ParseJsonObject(Body);
        if (!Json.IsValid()) { return false; }
        OutClaims.TicketId = Json->GetStringField(TEXT("ticket_id"));
        OutClaims.PlayerId = Json->GetStringField(TEXT("player_id"));
        Json->TryGetStringField(TEXT("session_id"), OutClaims.SessionId);
        OutClaims.CharacterId = Json->GetStringField(TEXT("character_id"));
        OutClaims.MatchId = Json->GetStringField(TEXT("match_id"));
        OutClaims.DestinationServerId = Json->GetStringField(TEXT("destination_server_id"));
        FString ExpiresAt;
        if (!Json->TryGetStringField(TEXT("expires_at_utc"), ExpiresAt) || !FDateTime::ParseIso8601(*ExpiresAt, OutClaims.ExpiresAtUtc))
        {
            return false;
        }
        OutClaims.bConsumed = Json->GetBoolField(TEXT("consumed"));
        return OutClaims.bConsumed;
    }

    TSharedRef<FJsonObject> ResultToJson(const FGamePlatformArenaMatchResult& Result)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("match_id"), Result.MatchId);
        Root->SetStringField(TEXT("arena_mode_id"), Result.ArenaModeId.ToString());
        Root->SetStringField(TEXT("game_server_id"), Result.GameServerId);
        Root->SetStringField(TEXT("started_at_utc"), Result.StartedAtUtc.ToIso8601());
        Root->SetStringField(TEXT("ended_at_utc"), Result.EndedAtUtc.ToIso8601());
        Root->SetStringField(TEXT("winning_team_id"), Result.WinningTeamId.ToString());
        Root->SetStringField(TEXT("end_reason"), StaticEnum<EGamePlatformArenaMatchEndReason>()->GetNameStringByValue(static_cast<int64>(Result.EndReason)));
        Root->SetNumberField(TEXT("end_revision"), Result.EndRevision);

        TArray<TSharedPtr<FJsonValue>> Teams;
        for (const FGamePlatformArenaTeamResult& Team : Result.Teams)
        {
            TSharedRef<FJsonObject> TeamJson = MakeShared<FJsonObject>();
            TeamJson->SetStringField(TEXT("team_id"), Team.TeamId.ToString());
            TeamJson->SetNumberField(TEXT("score"), Team.Score);
            TArray<TSharedPtr<FJsonValue>> Players;
            for (const FGamePlatformArenaPlayerResult& Player : Team.Players)
            {
                TSharedRef<FJsonObject> PlayerJson = MakeShared<FJsonObject>();
                PlayerJson->SetStringField(TEXT("player_id"), Player.PlayerId);
                PlayerJson->SetStringField(TEXT("team_id"), Player.TeamId.ToString());
                PlayerJson->SetStringField(TEXT("character_id"), Player.CharacterId);
                PlayerJson->SetNumberField(TEXT("kills"), Player.Kills);
                PlayerJson->SetNumberField(TEXT("deaths"), Player.Deaths);
                PlayerJson->SetNumberField(TEXT("assists"), Player.Assists);
                PlayerJson->SetNumberField(TEXT("score"), Player.Score);
                Players.Add(MakeShared<FJsonValueObject>(PlayerJson));
            }
            TeamJson->SetArrayField(TEXT("players"), Players);
            Teams.Add(MakeShared<FJsonValueObject>(TeamJson));
        }
        Root->SetArrayField(TEXT("teams"), Teams);
        return Root;
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
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
}

void FGamePlatformArenaServerBackendClient::GetAssignment(
    TFunction<void(bool, const FGamePlatformArenaAssignment&, const FString&)> Completion)
{
    FGamePlatformArenaAssignment Empty;
    if (!Config.IsValid()) { Completion(false, Empty, TEXT("Arena backend config invalid.")); return; }
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("GET"));
    Request->SetURL(MakeUrl(TEXT("/internal/v1/arena/assignment")));
    ApplyCommonHeaders(Request);
    Request->OnProcessRequestComplete().BindLambda(
        [Completion](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded) mutable
        {
            FGamePlatformArenaAssignment Assignment;
            if (!bSucceeded || !Response.IsValid()) { Completion(false, Assignment, TEXT("Assignment request transport failure.")); return; }
            if (Response->GetResponseCode() != 200 || !ParseAssignment(Response->GetContentAsString(), Assignment))
            {
                Completion(false, Assignment, FString::Printf(TEXT("Assignment request rejected: HTTP %d"), Response->GetResponseCode()));
                return;
            }
            Completion(true, Assignment, FString());
        });
    if (!Request->ProcessRequest()) { Completion(false, Empty, TEXT("Assignment request did not start.")); }
}

void FGamePlatformArenaServerBackendClient::ValidateAndConsumeTransferTicket(
    const FString& TransferTicket,
    const FString& PlayerId,
    const FString& MatchId,
    TFunction<void(bool, const FGamePlatformArenaTransferTicketClaims&, const FString&)> Completion)
{
    FGamePlatformArenaTransferTicketClaims Empty;
    if (!Config.IsValid() || TransferTicket.IsEmpty() || PlayerId.IsEmpty() || MatchId.IsEmpty())
    {
        Completion(false, Empty, TEXT("Ticket admission arguments invalid."));
        return;
    }
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("transfer_ticket"), TransferTicket);
    Json->SetStringField(TEXT("player_id"), PlayerId);
    Json->SetStringField(TEXT("match_id"), MatchId);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("POST"));
    Request->SetURL(MakeUrl(TEXT("/internal/v1/arena/tickets/validate-consume")));
    ApplyCommonHeaders(Request);
    Request->SetContentAsString(SerializeJson(Json));
    Request->OnProcessRequestComplete().BindLambda(
        [Completion](FHttpRequestPtr, FHttpResponsePtr Response, bool bSucceeded) mutable
        {
            FGamePlatformArenaTransferTicketClaims Claims;
            if (!bSucceeded || !Response.IsValid()) { Completion(false, Claims, TEXT("Ticket validation transport failure.")); return; }
            if (Response->GetResponseCode() != 200 || !ParseTicketClaims(Response->GetContentAsString(), Claims))
            {
                Completion(false, Claims, FString::Printf(TEXT("Ticket validation rejected: HTTP %d"), Response->GetResponseCode()));
                return;
            }
            Completion(true, Claims, FString());
        });
    if (!Request->ProcessRequest()) { Completion(false, Empty, TEXT("Ticket validation request did not start.")); }
}

void FGamePlatformArenaServerBackendClient::SubmitMatchResult(
    const FGamePlatformArenaMatchResult& Result,
    TFunction<void(EGamePlatformArenaSubmitStatus, const FString&)> Completion)
{
    if (!Config.IsValid() || Result.MatchId.IsEmpty())
    {
        Completion(EGamePlatformArenaSubmitStatus::TerminalFailure, TEXT("MatchResult or backend config invalid."));
        return;
    }
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("POST"));
    Request->SetURL(MakeUrl(TEXT("/internal/v1/arena/results")));
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
            FString Status;
            if (!Json.IsValid() || !Json->TryGetStringField(TEXT("status"), Status))
            {
                Completion(EGamePlatformArenaSubmitStatus::RetryableFailure, TEXT("MatchResult response malformed; outcome unknown."));
                return;
            }
            if (Status == TEXT("committed")) { Completion(EGamePlatformArenaSubmitStatus::Committed, FString()); return; }
            if (Status == TEXT("already_committed")) { Completion(EGamePlatformArenaSubmitStatus::AlreadyCommitted, FString()); return; }
            Completion(EGamePlatformArenaSubmitStatus::RetryableFailure, TEXT("Unknown result commit status."));
        });
    if (!Request->ProcessRequest()) { Completion(EGamePlatformArenaSubmitStatus::RetryableFailure, TEXT("MatchResult request did not start.")); }
}
