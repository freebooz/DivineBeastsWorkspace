#include "Server/GamePlatformHttpAdmissionProvider.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/ThreadSafeCounter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr float DefaultAdmissionTimeoutSeconds = 5.0f;
constexpr int32 MaxAdmissionResponseBytes = 32 * 1024;
constexpr int32 MaxAdmissionCredentialBytes = 16 * 1024;

float ResolveAdmissionTimeoutSeconds()
{
    const FString Configured = FPlatformMisc::GetEnvironmentVariable(
        TEXT("GAMESERVERCONTROL_ADMISSION_TIMEOUT_SECONDS"));
    if (Configured.IsEmpty())
    {
        return DefaultAdmissionTimeoutSeconds;
    }

    float Parsed = 0.0f;
    if (!FDefaultValueHelper::ParseFloat(Configured, Parsed) ||
        !FMath::IsFinite(Parsed))
    {
        return DefaultAdmissionTimeoutSeconds;
    }
    return FMath::Clamp(Parsed, 1.0f, 30.0f);
}

bool IsAllowedAdmissionBaseUrl(const FString& BaseUrl)
{
#if UE_BUILD_SHIPPING
    return BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#else
    return BaseUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) ||
        BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#endif
}

FGamePlatformServerAdmissionResult MakeAdmissionFailure(FName ErrorCode)
{
    FGamePlatformServerAdmissionResult Result;
    Result.bSucceeded = false;
    Result.ErrorCode = ErrorCode;
    return Result;
}

bool ParseCredentialObject(
    const TArray<uint8>& Credential,
    TSharedPtr<FJsonObject>& OutTicket)
{
    if (Credential.Num() < 32 ||
        Credential.Num() > MaxAdmissionCredentialBytes)
    {
        return false;
    }

    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Credential.GetData()),
        Credential.Num());
    const FString JsonText(Converted.Length(), Converted.Get());
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonText);
    return FJsonSerializer::Deserialize(Reader, OutTicket) &&
        OutTicket.IsValid();
}

FString SerializeAdmissionBody(const TSharedRef<FJsonObject>& Body)
{
    FString Content;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Content);
    FJsonSerializer::Serialize(Body, Writer);
    return Content;
}
}

void FGamePlatformHttpAdmissionProvider::ValidateAdmission(
    const FGamePlatformServerAdmissionTarget& Target,
    FGuid ConnectionId,
    uint64 ConnectionGeneration,
    FGamePlatformServerAdmissionProof Proof,
    FGamePlatformServerAdmissionCompletion Completion)
{
    check(IsInGameThread());

    const FGuid OperationId = Proof.OperationId;
    const FString ReservationId = Proof.ReservationId;

    if (!Target.IsValid() || !ConnectionId.IsValid() ||
        ConnectionGeneration == 0 || !Proof.IsValid() || !Completion)
    {
        if (Completion)
        {
            Completion(MakeAdmissionFailure(
                TEXT("ServerAdmissionRequestInvalid")));
        }
        Proof.ResetSensitive();
        return;
    }

    TSharedPtr<FJsonObject> Ticket;
    if (!ParseCredentialObject(Proof.Credential, Ticket))
    {
        Proof.ResetSensitive();
        Completion(MakeAdmissionFailure(
            TEXT("ServerAdmissionCredentialInvalid")));
        return;
    }

    // 原始字节在解析完成后立即清零；后续仅由本次HTTP请求正文短暂持有同一票据内容。
    Proof.ResetSensitive();

    FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(
        TEXT("GAMESERVERCONTROL_BASE_URL"));
    BaseUrl.RemoveFromEnd(TEXT("/"));
    const FString InternalToken = FPlatformMisc::GetEnvironmentVariable(
        TEXT("GAMESERVERCONTROL_INTERNAL_TOKEN"));

    if (BaseUrl.IsEmpty() || InternalToken.IsEmpty() ||
        BaseUrl.Len() > 2048 || InternalToken.Len() > 4096 ||
        !IsAllowedAdmissionBaseUrl(BaseUrl) ||
        BaseUrl.Contains(TEXT("?")) || BaseUrl.Contains(TEXT("#")) ||
        BaseUrl.Contains(TEXT("\r")) || BaseUrl.Contains(TEXT("\n")) ||
        InternalToken.Contains(TEXT("\r")) ||
        InternalToken.Contains(TEXT("\n")))
    {
        Completion(MakeAdmissionFailure(
            TEXT("ServerAdmissionConfigurationInvalid")));
        return;
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetObjectField(TEXT("ticket"), Ticket.ToSharedRef());
    Body->SetStringField(
        TEXT("destinationGameServerId"),
        Target.GameServerId);

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
    {
        Completion(MakeAdmissionFailure(
            TEXT("ServerAdmissionRedirectPolicyUnavailable")));
        return;
    }
#else
    Completion(MakeAdmissionFailure(
        TEXT("ServerAdmissionRedirectPolicyUnavailable")));
    return;
#endif

    Request->SetVerb(TEXT("POST"));
    Request->SetURL(
        BaseUrl + TEXT("/internal/v1/gameservers/validate-transfer"));
    Request->SetHeader(
        TEXT("Authorization"),
        TEXT("Bearer ") + InternalToken);
    Request->SetHeader(TEXT("X-Game-Server-Id"), Target.GameServerId);
    Request->SetHeader(
        TEXT("X-Game-Server-Boot-Id"),
        Target.ServerBootId);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetTimeout(ResolveAdmissionTimeoutSeconds());
    Request->SetContentAsString(SerializeAdmissionBody(Body));

    const TSharedRef<FGamePlatformServerAdmissionCompletion, ESPMode::ThreadSafe>
        SharedCompletion =
            MakeShared<
                FGamePlatformServerAdmissionCompletion,
                ESPMode::ThreadSafe>(MoveTemp(Completion));
    const TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> CompletionGate =
        MakeShared<FThreadSafeCounter, ESPMode::ThreadSafe>();

    const auto CompleteOnce =
        [SharedCompletion, CompletionGate](
            FGamePlatformServerAdmissionResult Result) mutable
        {
            if (CompletionGate->Increment() != 1)
            {
                return;
            }
            (*SharedCompletion)(MoveTemp(Result));
        };

    Request->OnProcessRequestComplete().BindLambda(
        [this,
         OperationId,
         Target,
         ConnectionId,
         ConnectionGeneration,
         ReservationId,
         CompleteOnce](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bSucceeded) mutable
        {
            UntrackRequest(OperationId);

            if (!bSucceeded || !Response.IsValid())
            {
                CompleteOnce(MakeAdmissionFailure(
                    TEXT("ServerAdmissionTransportFailed")));
                return;
            }

            const int32 ResponseCode = Response->GetResponseCode();
            if (ResponseCode < 200 || ResponseCode >= 300)
            {
                CompleteOnce(MakeAdmissionFailure(
                    ResponseCode == 401 || ResponseCode == 403
                        ? FName(TEXT("ServerAdmissionRejected"))
                        : FName(TEXT("ServerAdmissionBackendUnavailable"))));
                return;
            }

            const TArray<uint8>& Bytes = Response->GetContent();
            if (Bytes.IsEmpty() ||
                Bytes.Num() > MaxAdmissionResponseBytes)
            {
                CompleteOnce(MakeAdmissionFailure(
                    TEXT("ServerAdmissionResponseInvalid")));
                return;
            }

            const FUTF8ToTCHAR Converted(
                reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),
                Bytes.Num());
            const FString ResponseText(
                Converted.Length(),
                Converted.Get());
            TSharedPtr<FJsonObject> Json;
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseText);
            if (!FJsonSerializer::Deserialize(Reader, Json) ||
                !Json.IsValid())
            {
                CompleteOnce(MakeAdmissionFailure(
                    TEXT("ServerAdmissionResponseInvalid")));
                return;
            }

            FString PlayerId;
            FString SessionId;
            FString AssignmentId;
            FString WorldId;
            FString ExperienceId;
            FString GameSessionId;
            FString ServerBootId;
            int64 ProtocolVersion = 0;
            int64 SessionEpoch = 0;

            const bool bParsed =
                Json->TryGetStringField(TEXT("playerId"), PlayerId) &&
                Json->TryGetStringField(TEXT("sessionId"), SessionId) &&
                Json->TryGetStringField(
                    TEXT("assignmentId"),
                    AssignmentId) &&
                Json->TryGetStringField(
                    TEXT("destinationWorldId"),
                    WorldId) &&
                Json->TryGetStringField(
                    TEXT("destinationExperienceId"),
                    ExperienceId) &&
                Json->TryGetStringField(
                    TEXT("gameSessionId"),
                    GameSessionId) &&
                Json->TryGetStringField(
                    TEXT("destinationServerBootId"),
                    ServerBootId) &&
                Json->TryGetNumberField(
                    TEXT("destinationProtocolVersion"),
                    ProtocolVersion) &&
                Json->TryGetNumberField(
                    TEXT("sessionEpoch"),
                    SessionEpoch);

            if (!bParsed || PlayerId.IsEmpty() || SessionId.IsEmpty() ||
                AssignmentId.IsEmpty() || GameSessionId.IsEmpty() ||
                ServerBootId != Target.ServerBootId ||
                WorldId != Target.WorldId ||
                ExperienceId != Target.ExperienceId ||
                ProtocolVersion <= 0 ||
                LexToString(ProtocolVersion) != Target.ProtocolVersion ||
                SessionEpoch <= 0)
            {
                CompleteOnce(MakeAdmissionFailure(
                    TEXT("ServerAdmissionBindingMismatch")));
                return;
            }

            FGamePlatformServerVerifiedAdmission Admission;
            Admission.AdmissionId = FGuid::NewGuid();
            Admission.ConnectionId = ConnectionId;
            Admission.PlayerId = MoveTemp(PlayerId);
            Admission.SessionId = MoveTemp(SessionId);
            Admission.GameSessionId = MoveTemp(GameSessionId);
            Admission.AssignmentId = MoveTemp(AssignmentId);
            Admission.ReservationId = ReservationId;
            Admission.ServerInstanceId = Target.GameServerId;
            Admission.ServerBootId = Target.ServerBootId;
            Admission.WorldId = Target.WorldId;
            Admission.ExperienceId = Target.ExperienceId;
            Admission.ProtocolVersion = Target.ProtocolVersion;
            Admission.ConnectionGeneration = ConnectionGeneration;
            Admission.SessionEpoch =
                static_cast<uint64>(SessionEpoch);
            // Admission在当前真实连接生命周期内有效；断线/Logout必须显式Release。
            // 使用最大时间避免把仅用于一次握手的短期TransferTicket TTL误当在线会话TTL。
            Admission.AuthorityUntil = FDateTime::MaxValue();

            FGamePlatformServerAdmissionResult Result;
            Result.bSucceeded = Admission.IsStructurallyValid();
            Result.ErrorCode = Result.bSucceeded
                ? NAME_None
                : FName(TEXT("ServerAdmissionProjectionInvalid"));
            Result.Admission = MoveTemp(Admission);
            CompleteOnce(MoveTemp(Result));
        });

    TrackRequest(OperationId, Request);
    if (!Request->ProcessRequest())
    {
        UntrackRequest(OperationId);
        CompleteOnce(MakeAdmissionFailure(
            TEXT("ServerAdmissionRequestStartFailed")));
    }
}

void FGamePlatformHttpAdmissionProvider::ReleaseAdmission(
    const FGamePlatformServerVerifiedAdmission& Admission,
    FGamePlatformServerAdmissionCompletion Completion)
{
    check(IsInGameThread());

    FGamePlatformServerAdmissionResult Result;
    if (!Admission.IsStructurallyValid())
    {
        Result = MakeAdmissionFailure(
            TEXT("ServerAdmissionReleaseInvalid"));
    }
    else
    {
        // TransferTicket是一次性领取凭据，验证成功后不存在“退回票据”语义。
        // 玩家断线时撤销本地VerifiedAdmission即完成释放；SessionEpoch保证旧来源不能撤销新连接。
        Result.bSucceeded = true;
        Result.Admission = Admission;
    }

    if (Completion)
    {
        Completion(MoveTemp(Result));
    }
}

void FGamePlatformHttpAdmissionProvider::CancelOperation(
    const FGuid& OperationId)
{
    FHttpRequestPtr Request;
    {
        FScopeLock Lock(&RequestsMutex);
        Requests.RemoveAndCopyValue(OperationId, Request);
    }
    if (Request.IsValid())
    {
        Request->CancelRequest();
    }
}

void FGamePlatformHttpAdmissionProvider::TrackRequest(
    const FGuid& OperationId,
    const FHttpRequestPtr& Request)
{
    FScopeLock Lock(&RequestsMutex);
    Requests.Add(OperationId, Request);
}

void FGamePlatformHttpAdmissionProvider::UntrackRequest(
    const FGuid& OperationId)
{
    FScopeLock Lock(&RequestsMutex);
    Requests.Remove(OperationId);
}
