// 平台服务器HTTP准入适配；模块拥有Provider，每次请求独立拥有有界接收缓冲与一次完成门。
// 接收线程仅操作受锁缓冲；解析/权威处理/取消/关闭通知统一在游戏线程，关闭先移账本再解绑。
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
#include "Server/AdmissionResponseBudget.h"

namespace
{
constexpr float DefaultAdmissionTimeoutSeconds = 5.0f;
constexpr int32 MaxAdmissionResponseBytes = 32 * 1024;
constexpr int32 MaxAdmissionCredentialBytes = 16 * 1024;
/** HTTP接收线程持有有界缓冲；游戏线程完成时在锁下取出，绝不先无界接收后检查。 */
struct FAdmissionResponseBody
{
    FCriticalSection Mutex;
    TArray<uint8> Bytes;
    bool bHasOverflow = false;
};

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

    if (bIsClosing || Requests.Contains(OperationId))
    {
        if (Completion) Completion(MakeAdmissionFailure(bIsClosing ? TEXT("ServerAdmissionProviderClosed") : TEXT("ServerAdmissionOperationDuplicate")));
        return;
    }

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

    FString TicketId;
    if (!Ticket->TryGetStringField(TEXT("ticketId"), TicketId) ||
        TicketId.IsEmpty() ||
        TicketId != ReservationId)
    {
        // RPC外层ReservationId只承担幂等/关联作用，但必须与签名票据自身身份一致；
        // 不允许两套票据身份进入后续HTTP请求和VerifiedAdmission投影。
        Proof.ResetSensitive();
        Completion(MakeAdmissionFailure(
            TEXT("ServerAdmissionTicketIdentityMismatch")));
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
    // Shutdown与完成均在游戏线程串行；接收回调只访问独立共享缓冲，避免裸Provider跨线程悬空。
    Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    const auto ResponseBody = MakeShared<FAdmissionResponseBody, ESPMode::ThreadSafe>();
    FHttpRequestStreamDelegateV2 Receive = FHttpRequestStreamDelegateV2::CreateLambda(
        [ResponseBody](void* Data, int64& InOutLength)
        {
            if (!Data || InOutLength <= 0) return;
            FScopeLock Lock(&ResponseBody->Mutex);
            if (ResponseBody->bHasOverflow || !GamePlatform::Server::CanAcceptAdmissionResponseBytes(ResponseBody->Bytes.Num(), InOutLength, MaxAdmissionResponseBytes))
            { ResponseBody->bHasOverflow = true; InOutLength = 0; return; }
            ResponseBody->Bytes.Append(static_cast<const uint8*>(Data), static_cast<int32>(InOutLength));
        });
    if (!Request->SetResponseBodyReceiveStreamDelegateV2(MoveTemp(Receive)))
    { Completion(MakeAdmissionFailure(TEXT("ServerAdmissionResponseLimitUnavailable"))); return; }

    const TSharedRef<FGamePlatformServerAdmissionCompletion, ESPMode::ThreadSafe>
        SharedCompletion =
            MakeShared<
                FGamePlatformServerAdmissionCompletion,
                ESPMode::ThreadSafe>(MoveTemp(Completion));
    const TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> CompletionGate =
        MakeShared<FThreadSafeCounter, ESPMode::ThreadSafe>();

    const auto CompleteOnce =
        [SharedCompletion, CompletionGate](
            FGamePlatformServerAdmissionResult Result)
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
         CompleteOnce,
         ResponseBody](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bSucceeded) mutable
        {
            UntrackRequest(OperationId);

            TArray<uint8> Bytes;
            bool bOverflow = false;
            { FScopeLock Lock(&ResponseBody->Mutex); Bytes = MoveTemp(ResponseBody->Bytes); bOverflow = ResponseBody->bHasOverflow; }
            if (bOverflow)
            { CompleteOnce(MakeAdmissionFailure(TEXT("ServerAdmissionResponseTooLarge"))); return; }

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

    TrackRequest(OperationId, Request, CompleteOnce);
    if (!Request->ProcessRequest())
    {
        // 启动失败也必须解除裸Provider完成委托；该请求此后不再属于关闭账本。
        Request->OnProcessRequestComplete().Unbind();
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

FGamePlatformHttpAdmissionProvider::~FGamePlatformHttpAdmissionProvider() { Shutdown(); }

void FGamePlatformHttpAdmissionProvider::Shutdown()
{
    check(IsInGameThread());
    TArray<FTrackedRequest> Pending;
    {
        FScopeLock Lock(&RequestsMutex);
        if (bIsClosing) return;
        bIsClosing = true;
        for (auto& Pair : Requests) Pending.Add(MoveTemp(Pair.Value));
        Requests.Empty();
    }
    // 必须先解绑全部请求，再调用任何取消完成；完成回调重入不能看到半关闭账本。
    for (auto& Operation : Pending)
        if (Operation.Request) { Operation.Request->OnProcessRequestComplete().Unbind(); Operation.Request->CancelRequest(); }
    for (auto& Operation : Pending)
        if (Operation.Completion) Operation.Completion(MakeAdmissionFailure(TEXT("ServerAdmissionCancelled")));
}
void FGamePlatformHttpAdmissionProvider::CancelOperation(const FGuid& OperationId)
{
    check(IsInGameThread());
    FTrackedRequest Operation;
    { FScopeLock Lock(&RequestsMutex); if (!Requests.RemoveAndCopyValue(OperationId, Operation)) return; }
    if (Operation.Request) { Operation.Request->OnProcessRequestComplete().Unbind(); Operation.Request->CancelRequest(); }
    if (Operation.Completion) Operation.Completion(MakeAdmissionFailure(TEXT("ServerAdmissionCancelled")));
}
void FGamePlatformHttpAdmissionProvider::TrackRequest(const FGuid& OperationId, const FHttpRequestPtr& Request,
    FGamePlatformServerAdmissionCompletion Completion)
{
    FScopeLock Lock(&RequestsMutex);
    check(!bIsClosing && !Requests.Contains(OperationId));
    FTrackedRequest Operation; Operation.Request = Request; Operation.Completion = MoveTemp(Completion);
    Requests.Add(OperationId, MoveTemp(Operation));
}
void FGamePlatformHttpAdmissionProvider::UntrackRequest(const FGuid& OperationId)
{
    FScopeLock Lock(&RequestsMutex);
    Requests.Remove(OperationId);
}
