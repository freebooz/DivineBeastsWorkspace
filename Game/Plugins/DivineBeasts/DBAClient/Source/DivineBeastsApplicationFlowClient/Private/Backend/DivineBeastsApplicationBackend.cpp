#include "Backend/DivineBeastsApplicationBackend.h"

#include "GamePlatformOnlineClientSubsystem.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Version/DivineBeastsContractVersion.h"

namespace
{
    EDivineBeastsOnboardingState ParseOnboarding(const FString& Value)
    {
        if (Value == TEXT("TutorialRequired"))
        {
            return EDivineBeastsOnboardingState::TutorialRequired;
        }
        if (Value == TEXT("TutorialInProgress"))
        {
            return EDivineBeastsOnboardingState::TutorialInProgress;
        }
        if (Value == TEXT("OnboardingComplete"))
        {
            return EDivineBeastsOnboardingState::OnboardingComplete;
        }
        return EDivineBeastsOnboardingState::New;
    }

    bool ParseCharacter(
        const TSharedPtr<FJsonObject>& Json,
        FDivineBeastsCharacterSummary& Out)
    {
        if (!Json.IsValid())
        {
            return false;
        }
        Out.CharacterId = Json->GetStringField(TEXT("character_id"));
        Out.HeroDefinitionId =
            FName(*Json->GetStringField(TEXT("hero_definition_id")));
        Out.CharacterName = Json->GetStringField(TEXT("character_name"));
        Out.CharacterRevision =
            static_cast<int64>(Json->GetNumberField(TEXT("character_revision")));
        Out.OnboardingState =
            ParseOnboarding(Json->GetStringField(TEXT("onboarding_state")));
        Out.Status = FName(*Json->GetStringField(TEXT("status")));
        return !Out.CharacterId.IsEmpty() &&
            !Out.HeroDefinitionId.IsNone() &&
            Out.CharacterRevision > 0;
    }

    bool DeserializeObject(
        const FString& Text,
        TSharedPtr<FJsonObject>& Out)
    {
        const TSharedRef<TJsonReader<>> Reader =
            TJsonReaderFactory<>::Create(Text);
        return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
    }

    FString GuidText(const FGuid& Guid)
    {
        return Guid.ToString(EGuidFormats::DigitsWithHyphens);
    }
}

FDivineBeastsHttpApplicationBackend::FDivineBeastsHttpApplicationBackend(
    UGamePlatformOnlineClientSubsystem* InOnline)
    : Online(InOnline)
{
    GatewayBaseUrl =
        FPlatformMisc::GetEnvironmentVariable(
            TEXT("DIVINEBEASTS_GATEWAY_BASE_URL"));
    GatewayBaseUrl.RemoveFromEnd(TEXT("/"));
}

FString FDivineBeastsHttpApplicationBackend::MakeUrl(
    const FString& RelativePath) const
{
    return GatewayBaseUrl + RelativePath;
}

FString FDivineBeastsHttpApplicationBackend::GetAuthHeader() const
{
    return Online.IsValid()
        ? Online->GetAuthorizationHeaderValueTransient()
        : FString();
}

void FDivineBeastsHttpApplicationBackend::Send(
    const FString& Verb,
    const FString& RelativePath,
    const TSharedPtr<FJsonObject>& Body,
    FRawCompletion Completion)
{
    const FString Authorization = GetAuthHeader();
    if (GatewayBaseUrl.IsEmpty() || Authorization.IsEmpty())
    {
        Completion(
            false,
            FString(),
            EDivineBeastsFlowError::AuthenticationRequired);
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    Request->SetURL(MakeUrl(RelativePath));
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Authorization"), Authorization);
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));

    if (Body.IsValid())
    {
        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&Serialized);
        FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetContentAsString(Serialized);
    }

    ActiveRequests.Add(Request);
    const TWeakPtr<FDivineBeastsHttpApplicationBackend> WeakThis =
        AsShared();
    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, Completion = MoveTemp(Completion)](
            FHttpRequestPtr CompletedRequest,
            FHttpResponsePtr Response,
            bool bTransportSuccess) mutable
        {
            const TSharedPtr<FDivineBeastsHttpApplicationBackend> Self =
                WeakThis.Pin();
            if (!Self.IsValid())
            {
                return;
            }

            Self->ActiveRequests.Remove(CompletedRequest);
            if (!bTransportSuccess || !Response.IsValid())
            {
                Completion(
                    false,
                    FString(),
                    EDivineBeastsFlowError::ProfileUnavailable);
                return;
            }

            const int32 Code = Response->GetResponseCode();
            if (Code < 200 || Code >= 300)
            {
                EDivineBeastsFlowError Error =
                    EDivineBeastsFlowError::ProfileUnavailable;
                if (Code == 401)
                {
                    Error = EDivineBeastsFlowError::AuthenticationRequired;
                }
                else if (Code == 403)
                {
                    Error = EDivineBeastsFlowError::CharacterSelectionRejected;
                }
                else if (Code == 409)
                {
                    Error = EDivineBeastsFlowError::CharacterCreateOutcomeUnknown;
                }
                else if (Code == 412)
                {
                    Error = EDivineBeastsFlowError::ContractIncompatible;
                }
                else if (Code == 503)
                {
                    Error = EDivineBeastsFlowError::WorldAssignmentUnavailable;
                }
                Completion(false, FString(), Error);
                return;
            }
            Completion(
                true,
                Response->GetContentAsString(),
                EDivineBeastsFlowError::None);
        });

    if (!Request->ProcessRequest())
    {
        ActiveRequests.Remove(Request);
        Completion(
            false,
            FString(),
            EDivineBeastsFlowError::ProfileUnavailable);
    }
}

void FDivineBeastsHttpApplicationBackend::LoadProfile(
    FDivineBeastsProfileCompletion Completion)
{
    Send(
        TEXT("GET"),
        // 玩家资料已经由共享 Gateway 契约正式定义。
        // 必须使用 Shared/Contracts/GamePlatform/OpenAPI/gateway.openapi.yaml 中的
        // GET /v1/player/profile，禁止继续调用历史项目私有路径。
        TEXT("/v1/player/profile"),
        nullptr,
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            FDivineBeastsPlayerProfile Result;
            if (!bSuccess)
            {
                Completion(false, Result, Error);
                return;
            }
            TSharedPtr<FJsonObject> Json;
            if (!DeserializeObject(Raw, Json))
            {
                Completion(
                    false,
                    Result,
                    EDivineBeastsFlowError::ProfileUnavailable);
                return;
            }
            // Shared Gateway PlayerProfile 使用 camelCase 字段：
            // playerId / revision / tutorialCompleted / defaultWorldId。
            // 当前正式契约尚未提供“最近角色/最近体验”字段，因此保持默认空值，
            // 不从不存在的 JSON 字段推断项目状态。
            double RevisionNumber = -1.0;
            bool bTutorialCompleted = false;
            if (!Json->TryGetStringField(TEXT("playerId"), Result.PlayerId) ||
                !Json->TryGetNumberField(TEXT("revision"), RevisionNumber) ||
                !Json->TryGetBoolField(
                    TEXT("tutorialCompleted"),
                    bTutorialCompleted) ||
                Result.PlayerId.IsEmpty() ||
                RevisionNumber < 0.0)
            {
                Completion(
                    false,
                    FDivineBeastsPlayerProfile(),
                    EDivineBeastsFlowError::ProfileUnavailable);
                return;
            }

            Result.ProfileRevision =
                static_cast<int64>(RevisionNumber);
            Result.OnboardingState = bTutorialCompleted
                ? EDivineBeastsOnboardingState::OnboardingComplete
                : EDivineBeastsOnboardingState::TutorialRequired;

            FString DefaultWorldId;
            if (Json->TryGetStringField(
                    TEXT("defaultWorldId"),
                    DefaultWorldId) &&
                !DefaultWorldId.IsEmpty())
            {
                Result.LastWorldId = FName(*DefaultWorldId);
            }

            Completion(true, Result, EDivineBeastsFlowError::None);
        });
}

void FDivineBeastsHttpApplicationBackend::LoadRoster(
    FDivineBeastsRosterCompletion Completion)
{
    Send(
        TEXT("GET"),
        TEXT("/v1/divinebeasts/characters"),
        nullptr,
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            TArray<FDivineBeastsCharacterSummary> Result;
            if (!bSuccess)
            {
                Completion(false, Result, Error);
                return;
            }

            TArray<TSharedPtr<FJsonValue>> Values;
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Raw);
            if (!FJsonSerializer::Deserialize(Reader, Values))
            {
                Completion(
                    false,
                    Result,
                    EDivineBeastsFlowError::CharacterRosterUnavailable);
                return;
            }

            for (const TSharedPtr<FJsonValue>& Value : Values)
            {
                FDivineBeastsCharacterSummary Character;
                if (!Value.IsValid() ||
                    !ParseCharacter(Value->AsObject(), Character))
                {
                    Completion(
                        false,
                        TArray<FDivineBeastsCharacterSummary>(),
                        EDivineBeastsFlowError::CharacterRosterUnavailable);
                    return;
                }
                Result.Add(MoveTemp(Character));
            }

            Completion(true, MoveTemp(Result), EDivineBeastsFlowError::None);
        });
}

void FDivineBeastsHttpApplicationBackend::CreateCharacter(
    const FDivineBeastsCharacterCreateDraft& Draft,
    const FGuid& OperationId,
    FDivineBeastsCharacterCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("operation_id"), GuidText(OperationId));
    Body->SetStringField(
        TEXT("hero_definition_id"),
        Draft.HeroDefinitionId.ToString());
    Body->SetStringField(TEXT("character_name"), Draft.CharacterName);
    Body->SetStringField(
        TEXT("expected_catalog_revision"),
        FDivineBeastsContractVersion::GetGeneratedRevision());
    Body->SetStringField(
        TEXT("expected_contract_version"),
        FDivineBeastsContractVersion::GetCurrentVersion());

    TSharedPtr<FJsonObject> Appearance = MakeShared<FJsonObject>();
    for (const TPair<FString, FString>& Pair : Draft.AppearanceSelection)
    {
        Appearance->SetStringField(Pair.Key, Pair.Value);
    }
    Body->SetObjectField(TEXT("appearance_selection"), Appearance);

    Send(
        TEXT("POST"),
        TEXT("/v1/divinebeasts/characters"),
        Body,
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            FDivineBeastsCharacterSummary Character;
            if (!bSuccess)
            {
                Completion(false, Character, Error);
                return;
            }
            TSharedPtr<FJsonObject> Json;
            if (!DeserializeObject(Raw, Json) ||
                !ParseCharacter(Json, Character))
            {
                Completion(
                    false,
                    Character,
                    EDivineBeastsFlowError::CharacterCreateRejected);
                return;
            }
            Completion(
                true,
                Character,
                EDivineBeastsFlowError::None);
        });
}

void FDivineBeastsHttpApplicationBackend::SelectPersistentCharacter(
    const FString& CharacterId,
    int64 ExpectedRevision,
    const FGuid& SelectionRequestId,
    FDivineBeastsSelectionCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("selection_request_id"),
        GuidText(SelectionRequestId));
    Body->SetStringField(TEXT("character_id"), CharacterId);
    Body->SetNumberField(
        TEXT("expected_character_revision"),
        static_cast<double>(ExpectedRevision));

    Send(
        TEXT("POST"),
        TEXT("/v1/divinebeasts/characters/select"),
        Body,
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            FDivineBeastsValidatedSelection Result;
            if (!bSuccess)
            {
                Completion(false, Result, Error);
                return;
            }
            TSharedPtr<FJsonObject> Json;
            if (!DeserializeObject(Raw, Json))
            {
                Completion(
                    false,
                    Result,
                    EDivineBeastsFlowError::CharacterSelectionRejected);
                return;
            }
            Result.SelectionRequestId =
                Json->GetStringField(TEXT("selection_request_id"));
            Result.ProfileRevision =
                static_cast<int64>(
                    Json->GetNumberField(TEXT("profile_revision")));
            const TSharedPtr<FJsonObject>* CharacterJson = nullptr;
            if (!Json->TryGetObjectField(
                    TEXT("character"),
                    CharacterJson) ||
                !CharacterJson ||
                !ParseCharacter(*CharacterJson, Result.Character))
            {
                Completion(
                    false,
                    FDivineBeastsValidatedSelection(),
                    EDivineBeastsFlowError::CharacterSelectionRejected);
                return;
            }
            Completion(true, Result, EDivineBeastsFlowError::None);
        });
}

void FDivineBeastsHttpApplicationBackend::RequestWorldAssignment(
    const FString& CharacterId,
    int64 ExpectedRevision,
    FName DesiredExperienceId,
    const FString& PreferredRegion,
    const FGuid& RequestId,
    FDivineBeastsWorldAssignmentCompletion Completion)
{
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("request_id"), GuidText(RequestId));
    Body->SetStringField(TEXT("character_id"), CharacterId);
    Body->SetStringField(
        TEXT("desired_experience_id"),
        DesiredExperienceId.ToString());
    Body->SetNumberField(
        TEXT("expected_character_revision"),
        static_cast<double>(ExpectedRevision));
    if (!PreferredRegion.IsEmpty())
    {
        Body->SetStringField(TEXT("preferred_region"), PreferredRegion);
    }

    Send(
        TEXT("POST"),
        TEXT("/v1/divinebeasts/world-entry"),
        Body,
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            FDivineBeastsWorldAssignmentPayload Result;
            if (!bSuccess)
            {
                Completion(false, Result, Error);
                return;
            }
            TSharedPtr<FJsonObject> Json;
            if (!DeserializeObject(Raw, Json))
            {
                Completion(
                    false,
                    Result,
                    EDivineBeastsFlowError::WorldAssignmentUnavailable);
                return;
            }
            Result.Summary.AssignmentId =
                Json->GetStringField(TEXT("assignment_id"));
            Result.Summary.GameServerId =
                Json->GetStringField(TEXT("game_server_id"));
            Result.Summary.ServerRoleId =
                FName(*Json->GetStringField(TEXT("server_role_id")));
            Result.Summary.ExperienceId =
                FName(*Json->GetStringField(TEXT("experience_id")));
            FString WorldId;
            Json->TryGetStringField(TEXT("world_id"), WorldId);
            Result.Summary.WorldId = FName(*WorldId);
            Result.Summary.MapId =
                FName(*Json->GetStringField(TEXT("map_id")));
            Result.Summary.RegionId =
                FName(*Json->GetStringField(TEXT("region_id")));
            Result.Summary.TicketId =
                Json->GetStringField(TEXT("ticket_id"));
            Result.Summary.CharacterId =
                Json->GetStringField(TEXT("character_id"));
            Result.Summary.SessionId =
                Json->GetStringField(TEXT("session_id"));
            Result.Endpoint =
                Json->GetStringField(TEXT("endpoint"));
            Result.TransferTicket =
                Json->GetStringField(TEXT("transfer_ticket"));

            const bool bValid =
                !Result.Summary.AssignmentId.IsEmpty() &&
                !Result.Summary.GameServerId.IsEmpty() &&
                !Result.Summary.ServerRoleId.IsNone() &&
                !Result.Summary.ExperienceId.IsNone() &&
                !Result.Summary.WorldId.IsNone() &&
                !Result.Summary.MapId.IsNone() &&
                !Result.Endpoint.IsEmpty() &&
                !Result.TransferTicket.IsEmpty();
            Completion(
                bValid,
                MoveTemp(Result),
                bValid
                    ? EDivineBeastsFlowError::None
                    : EDivineBeastsFlowError::WorldAssignmentUnavailable);
        });
}

void FDivineBeastsHttpApplicationBackend::CancelAll()
{
    for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request :
         ActiveRequests)
    {
        if (Request.IsValid())
        {
            Request->CancelRequest();
        }
    }
    ActiveRequests.Reset();
}
