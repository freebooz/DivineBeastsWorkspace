#include "Backend/DivineBeastsApplicationBackend.h"

#include "GamePlatformOnlineClientSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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

        // Shared Gateway CharacterSummary 使用 camelCase 字段。
        // 这里使用 TryGet 系列进行完整结构校验，避免缺字段时触发断言或半提交对象。
        FString HeroDefinitionId;
        FString OnboardingState;
        FString Status;
        int64 CharacterRevision = 0;
        if (!Json->TryGetStringField(TEXT("characterId"), Out.CharacterId) ||
            !Json->TryGetStringField(TEXT("heroDefinitionId"), HeroDefinitionId) ||
            !Json->TryGetStringField(TEXT("characterName"), Out.CharacterName) ||
            !Json->TryGetNumberField(TEXT("characterRevision"), CharacterRevision) ||
            !Json->TryGetStringField(TEXT("onboardingState"), OnboardingState) ||
            !Json->TryGetStringField(TEXT("status"), Status))
        {
            return false;
        }

        Out.HeroDefinitionId = FName(*HeroDefinitionId);
        Out.CharacterRevision = CharacterRevision;
        Out.OnboardingState = ParseOnboarding(OnboardingState);
        Out.Status = FName(*Status);
        FString AppearanceProfileId;
        if (Json->TryGetStringField(TEXT("appearanceProfileId"), AppearanceProfileId) &&
            !AppearanceProfileId.IsEmpty())
        {
            Out.AppearanceProfileId = FName(*AppearanceProfileId);
        }
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
        return FJsonSerializer::Deserialize(
                   Reader,
                   Out,
                   FJsonSerializer::EFlags::StoreNumbersAsStrings) &&
            Out.IsValid();
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
}

void FDivineBeastsHttpApplicationBackend::Send(
    const FString& Verb,
    const FString& RelativePath,
    const TSharedPtr<FJsonObject>& Body,
    bool bIdempotent,
    FString IdempotencyKey,
    FRawCompletion Completion)
{
    UGamePlatformOnlineClientSubsystem* OnlineService = Online.Get();
    if (!OnlineService || !Completion)
    {
        if (Completion)
        {
            Completion(
                false,
                FString(),
                EDivineBeastsFlowError::AuthenticationRequired);
        }
        return;
    }

    FString Serialized;
    if (Body.IsValid())
    {
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&Serialized);
        if (!FJsonSerializer::Serialize(Body.ToSharedRef(), Writer))
        {
            Completion(
                false,
                FString(),
                EDivineBeastsFlowError::ProfileUnavailable);
            return;
        }
    }

    FGamePlatformAuthenticatedRequest Request;
    Request.Verb = Verb;
    Request.RelativePath = RelativePath;
    Request.Body = MoveTemp(Serialized);
    Request.IdempotencyKey = MoveTemp(IdempotencyKey);
    Request.bIdempotent = bIdempotent;

    // GameInstance级业务操作允许跨地图继续，但会被本Backend的CancelAll显式取消。
    FGamePlatformOnlineRequestOptions Options;
    const TSharedRef<FGamePlatformOnlineRequestHandle> Handle =
        MakeShared<FGamePlatformOnlineRequestHandle>();
    const TWeakPtr<FDivineBeastsHttpApplicationBackend> WeakThis =
        AsShared();

    *Handle = OnlineService->SendAuthenticatedRequest(
        MoveTemp(Request),
        Options,
        [WeakThis, Handle, Completion = MoveTemp(Completion)](
            FGamePlatformAuthenticatedResponse Response) mutable
        {
            const TSharedPtr<FDivineBeastsHttpApplicationBackend> Self =
                WeakThis.Pin();
            if (!Self.IsValid())
            {
                return;
            }

            Self->ActiveRequests.RemoveAll(
                [Handle](const FGamePlatformOnlineRequestHandle& Existing)
                {
                    return Existing.InstanceScopeId == Handle->InstanceScopeId &&
                        Existing.RequestId == Handle->RequestId;
                });

            if (Response.IsSuccess())
            {
                Completion(
                    true,
                    Response.Body,
                    EDivineBeastsFlowError::None);
                return;
            }

            EDivineBeastsFlowError Error =
                EDivineBeastsFlowError::ProfileUnavailable;
            switch (Response.Error)
            {
            case EGamePlatformAuthError::AuthExpired:
            case EGamePlatformAuthError::InvalidCredentials:
                Error = EDivineBeastsFlowError::AuthenticationRequired;
                break;
            case EGamePlatformAuthError::Forbidden:
            case EGamePlatformAuthError::AccountLocked:
                Error = EDivineBeastsFlowError::CharacterSelectionRejected;
                break;
            case EGamePlatformAuthError::Conflict:
            case EGamePlatformAuthError::OutcomeUnknown:
                Error = EDivineBeastsFlowError::CharacterCreateOutcomeUnknown;
                break;
            case EGamePlatformAuthError::ContractIncompatible:
            case EGamePlatformAuthError::InvalidResponse:
                Error = EDivineBeastsFlowError::ContractIncompatible;
                break;
            case EGamePlatformAuthError::ServiceUnavailable:
            case EGamePlatformAuthError::Maintenance:
                Error = EDivineBeastsFlowError::WorldAssignmentUnavailable;
                break;
            default:
                if (Response.HttpStatusCode == 401)
                {
                    Error = EDivineBeastsFlowError::AuthenticationRequired;
                }
                else if (Response.HttpStatusCode == 403)
                {
                    Error = EDivineBeastsFlowError::CharacterSelectionRejected;
                }
                else if (Response.HttpStatusCode == 409)
                {
                    Error = EDivineBeastsFlowError::CharacterCreateOutcomeUnknown;
                }
                else if (Response.HttpStatusCode == 412)
                {
                    Error = EDivineBeastsFlowError::ContractIncompatible;
                }
                else if (Response.HttpStatusCode == 503)
                {
                    Error = EDivineBeastsFlowError::WorldAssignmentUnavailable;
                }
                break;
            }

            Completion(false, FString(), Error);
        });

    ActiveRequests.Add(*Handle);
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
        true,
        FString(),
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
            int64 RevisionNumber = -1;
            bool bTutorialCompleted = false;
            if (!Json->TryGetStringField(TEXT("playerId"), Result.PlayerId) ||
                !Json->TryGetNumberField(TEXT("revision"), RevisionNumber) ||
                !Json->TryGetBoolField(
                    TEXT("tutorialCompleted"),
                    bTutorialCompleted) ||
                Result.PlayerId.IsEmpty() ||
                RevisionNumber < 0)
            {
                Completion(
                    false,
                    FDivineBeastsPlayerProfile(),
                    EDivineBeastsFlowError::ProfileUnavailable);
                return;
            }

            Result.ProfileRevision = RevisionNumber;
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
        TEXT("/v1/player/characters"),
        nullptr,
        true,
        FString(),
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
    Body->SetStringField(TEXT("creationRequestId"), GuidText(OperationId));
    Body->SetStringField(
        TEXT("heroDefinitionId"),
        Draft.HeroDefinitionId.ToString());
    Body->SetStringField(TEXT("characterName"), Draft.CharacterName);

    TSharedPtr<FJsonObject> Appearance = MakeShared<FJsonObject>();
    for (const TPair<FString, FString>& Pair : Draft.AppearanceSelection)
    {
        Appearance->SetStringField(Pair.Key, Pair.Value);
    }
    Body->SetObjectField(TEXT("appearanceSelection"), Appearance);

    Send(
        TEXT("POST"),
        TEXT("/v1/player/characters"),
        Body,
        true,
        GuidText(OperationId),
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            FDivineBeastsCharacterSummary Character;
            if (!bSuccess)
            {
                if (Error == EDivineBeastsFlowError::CharacterCreateOutcomeUnknown)
                {
                    Error = EDivineBeastsFlowError::CharacterCreateRejected;
                }
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
        TEXT("selectionRequestId"),
        GuidText(SelectionRequestId));
    Body->SetStringField(TEXT("characterId"), CharacterId);
    Body->SetField(
        TEXT("expectedCharacterRevision"),
        MakeShared<FJsonValueNumberString>(LexToString(ExpectedRevision)));

    Send(
        TEXT("POST"),
        TEXT("/v1/player/character-selection"),
        Body,
        true,
        GuidText(SelectionRequestId),
        [Completion = MoveTemp(Completion)](
            bool bSuccess,
            const FString& Raw,
            EDivineBeastsFlowError Error) mutable
        {
            FDivineBeastsValidatedSelection Result;
            if (!bSuccess)
            {
                if (Error == EDivineBeastsFlowError::CharacterCreateOutcomeUnknown)
                {
                    Error = EDivineBeastsFlowError::CharacterSelectionRejected;
                }
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
                Json->GetStringField(TEXT("selectionRequestId"));
            if (!Json->TryGetNumberField(
                    TEXT("profileRevision"),
                    Result.ProfileRevision))
            {
                Completion(
                    false,
                    FDivineBeastsValidatedSelection(),
                    EDivineBeastsFlowError::CharacterSelectionRejected);
                return;
            }
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
    Body->SetStringField(TEXT("requestId"), GuidText(RequestId));
    Body->SetStringField(TEXT("characterId"), CharacterId);
    Body->SetStringField(
        TEXT("desiredExperienceId"),
        DesiredExperienceId.ToString());
    Body->SetField(
        TEXT("expectedCharacterRevision"),
        MakeShared<FJsonValueNumberString>(FString::Printf(TEXT("%lld"), ExpectedRevision)));
    if (!PreferredRegion.IsEmpty())
    {
        Body->SetStringField(TEXT("preferredRegion"), PreferredRegion);
    }

    Send(
        TEXT("POST"),
        TEXT("/v1/divinebeasts/world-entry"),
        Body,
        true,
        GuidText(RequestId),
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
                Json->GetStringField(TEXT("assignmentId"));
            Result.Summary.GameServerId =
                Json->GetStringField(TEXT("gameServerId"));
            Result.Summary.ServerRoleId =
                FName(*Json->GetStringField(TEXT("serverRoleId")));
            Result.Summary.ExperienceId =
                FName(*Json->GetStringField(TEXT("experienceId")));
            FString WorldId;
            Json->TryGetStringField(TEXT("worldId"), WorldId);
            Result.Summary.WorldId = FName(*WorldId);
            Result.Summary.MapId =
                FName(*Json->GetStringField(TEXT("mapId")));
            Result.Summary.RegionId =
                FName(*Json->GetStringField(TEXT("regionId")));
            Result.Summary.TicketId =
                Json->GetStringField(TEXT("ticketId"));
            Result.Summary.CharacterId =
                Json->GetStringField(TEXT("characterId"));
            Result.Summary.SessionId =
                Json->GetStringField(TEXT("sessionId"));
            Result.Endpoint =
                Json->GetStringField(TEXT("endpoint"));
            Result.TransferTicket =
                Json->GetStringField(TEXT("transferTicket"));

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
    if (UGamePlatformOnlineClientSubsystem* OnlineService = Online.Get())
    {
        for (const FGamePlatformOnlineRequestHandle& Request :
             ActiveRequests)
        {
            OnlineService->Cancel(Request);
        }
    }
    ActiveRequests.Reset();
}
