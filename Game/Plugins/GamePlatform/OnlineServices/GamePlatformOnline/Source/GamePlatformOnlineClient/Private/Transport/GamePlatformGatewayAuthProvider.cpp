#include "Transport/GamePlatformGatewayAuthProvider.h"

#include "Async/Async.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IGamePlatformOnlineService.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 MaxGatewayLoginNameChars = 256;
constexpr int32 MaxCredentialChars = 4096;
constexpr int32 MaxIdentityChars = 512;
constexpr int32 MaxTokenChars = 8192;

struct FResponseBodyState
{
    FCriticalSection Mutex;
    TArray<uint8> Bytes;
    bool bOverflow = false;
};

void CompleteAuthLater(
    FGamePlatformAuthCompletion Completion,
    FGamePlatformAuthProviderResult Result)
{
    AsyncTask(
        ENamedThreads::GameThread,
        [Completion = MoveTemp(Completion), Result = MoveTemp(Result)]() mutable
        {
            if (Completion)
            {
                Completion(MoveTemp(Result));
            }
        });
}

bool SerializeObject(
    const TSharedRef<FJsonObject>& Json,
    FString& Out)
{
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Out);
    return FJsonSerializer::Serialize(Json, Writer);
}

FString Utf8BytesToString(TArray<uint8> Bytes)
{
    if (Bytes.IsEmpty())
    {
        return FString();
    }
    const FUTF8ToTCHAR Converted(
        reinterpret_cast<const ANSICHAR*>(Bytes.GetData()),
        Bytes.Num());
    return FString(Converted.Length(), Converted.Get());
}
}

FGamePlatformGatewayAuthProvider::FGamePlatformGatewayAuthProvider() = default;

FGamePlatformGatewayAuthProvider::~FGamePlatformGatewayAuthProvider()
{
    CancelAll();
}

FGamePlatformResult FGamePlatformGatewayAuthProvider::Configure(
    const FGamePlatformOnlineConfiguration& InConfiguration)
{
    if (!ActiveRequests.IsEmpty())
    {
        return FGamePlatformResult::Failure(
            TEXT("OnlineProviderBusy"),
            TEXT("存在未完成在线请求时不能重新配置Gateway。"));
    }

    const FGamePlatformResult Validated =
        IGamePlatformOnlineService::ValidateConfiguration(InConfiguration);
    if (!Validated.IsSuccess())
    {
        return Validated;
    }

    // Configure阶段即验证敏感HTTP所需能力，避免第一次登录时才发现后端会自动重定向
    // 或无法实施流式响应预算。能力不明确时统一Fail Closed。
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CapabilityRequest =
        FHttpModule::Get().CreateRequest();
#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    if (!CapabilityRequest->SetRedirectPolicy(
            EHttpRequestRedirectPolicy::Reject))
    {
        return FGamePlatformResult::Failure(
            TEXT("OnlineUnsupportedTransport"),
            TEXT("当前HTTP后端不能可靠禁止敏感请求自动重定向。"));
    }
#else
    return FGamePlatformResult::Failure(
        TEXT("OnlineUnsupportedTransport"),
        TEXT("当前HTTP后端未公开敏感请求重定向控制能力。"));
#endif

    FHttpRequestStreamDelegateV2 CapabilityStream =
        FHttpRequestStreamDelegateV2::CreateLambda(
            [](void*, int64&) {});
    if (!CapabilityRequest->SetResponseBodyReceiveStreamDelegateV2(
            MoveTemp(CapabilityStream)))
    {
        return FGamePlatformResult::Failure(
            TEXT("OnlineUnsupportedTransport"),
            TEXT("当前HTTP后端不能实施传输中的响应正文大小限制。"));
    }

    Configuration = InConfiguration;
    while (Configuration.ServiceOrigin.RemoveFromEnd(TEXT("/")))
    {
    }
    bConfigured = true;
    ClearTokens();
    return FGamePlatformResult::Success();
}

void FGamePlatformGatewayAuthProvider::TryAutoLogin(
    FGamePlatformAuthCompletion Completion)
{
    // 当前没有经过安全评审的系统凭据仓。明确失败，不从普通磁盘、INI或环境变量恢复RefreshToken。
    FGamePlatformAuthProviderResult Result;
    Result.Error = EGamePlatformAuthError::AuthExpired;
    CompleteAuthLater(MoveTemp(Completion), MoveTemp(Result));
}

void FGamePlatformGatewayAuthProvider::LoginWithCredentials(
    const FString& LoginName,
    const FString& Password,
    FGamePlatformAuthCompletion Completion)
{
    const FString NormalizedLogin = LoginName.TrimStartAndEnd();
    if (!bConfigured ||
        NormalizedLogin.IsEmpty() ||
        Password.IsEmpty() ||
        NormalizedLogin.Len() > MaxGatewayLoginNameChars ||
        Password.Len() > MaxCredentialChars)
    {
        FGamePlatformAuthProviderResult Result;
        Result.Error = bConfigured
            ? EGamePlatformAuthError::InvalidCredentials
            : EGamePlatformAuthError::ProviderUnavailable;
        CompleteAuthLater(MoveTemp(Completion), MoveTemp(Result));
        return;
    }

    const uint64 Generation = ++OperationGeneration;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("gameId"), Configuration.GameId);
    Body->SetStringField(TEXT("provider"), TEXT("password"));
    Body->SetStringField(TEXT("accountName"), NormalizedLogin);
    Body->SetStringField(TEXT("credential"), Password);
    Body->SetStringField(TEXT("clientVersion"), Configuration.ClientVersion);
    Body->SetStringField(TEXT("deviceId"), FString());

    FString Serialized;
    if (!SerializeObject(Body, Serialized))
    {
        FGamePlatformAuthProviderResult Result;
        Result.Error = EGamePlatformAuthError::InvalidRequest;
        CompleteAuthLater(MoveTemp(Completion), MoveTemp(Result));
        return;
    }

    const FGuid RequestId = FGuid::NewGuid();
    const TWeakPtr<FGamePlatformGatewayAuthProvider> WeakThis = AsShared();
    SendRequest(
        RequestId,
        TEXT("POST"),
        TEXT("/v1/auth/login"),
        Serialized,
        FString(),
        FString(),
        [WeakThis, Generation, Completion = MoveTemp(Completion)](
            FRawResponse Response) mutable
        {
            const TSharedPtr<FGamePlatformGatewayAuthProvider> Self =
                WeakThis.Pin();
            if (!Self.IsValid() ||
                Self->OperationGeneration != Generation)
            {
                return;
            }

            FGamePlatformAuthProviderResult Result;
            if (Response.bTransportSuccess &&
                Response.HttpCode == 200 &&
                Self->ParseAndCommitLoginResponse(Response.Body, Result))
            {
                Result.bSuccess = true;
                Result.Error = EGamePlatformAuthError::None;
            }
            else
            {
                Self->ClearTokens();
                Result.Error = MapAuthError(
                    Response.HttpCode,
                    Response.bTransportSuccess,
                    false,
                    Response.bResponseTooLarge);
            }

            if (Completion)
            {
                Completion(MoveTemp(Result));
            }
        });
}

void FGamePlatformGatewayAuthProvider::Refresh(
    FGamePlatformAuthCompletion Completion)
{
    if (!bConfigured ||
        RefreshToken.IsEmpty() ||
        AccountId.IsEmpty() ||
        SessionId.IsEmpty() ||
        RefreshExpiresAt <= FDateTime::UtcNow())
    {
        ClearTokens();
        FGamePlatformAuthProviderResult Result;
        Result.Error = EGamePlatformAuthError::AuthExpired;
        CompleteAuthLater(MoveTemp(Completion), MoveTemp(Result));
        return;
    }

    const uint64 Generation = ++OperationGeneration;
    const FString PreviousAccountId = AccountId;
    const FString PreviousSessionId = SessionId;
    const FString RefreshTokenSnapshot = RefreshToken;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("refreshToken"), RefreshTokenSnapshot);

    FString Serialized;
    if (!SerializeObject(Body, Serialized))
    {
        ClearTokens();
        FGamePlatformAuthProviderResult Result;
        Result.Error = EGamePlatformAuthError::InvalidRequest;
        CompleteAuthLater(MoveTemp(Completion), MoveTemp(Result));
        return;
    }

    const FGuid RequestId = FGuid::NewGuid();
    const TWeakPtr<FGamePlatformGatewayAuthProvider> WeakThis = AsShared();
    SendRequest(
        RequestId,
        TEXT("POST"),
        TEXT("/v1/auth/refresh"),
        Serialized,
        FString(),
        FString(),
        [WeakThis,
         Generation,
         PreviousAccountId,
         PreviousSessionId,
         Completion = MoveTemp(Completion)](FRawResponse Response) mutable
        {
            const TSharedPtr<FGamePlatformGatewayAuthProvider> Self =
                WeakThis.Pin();
            if (!Self.IsValid() ||
                Self->OperationGeneration != Generation)
            {
                return;
            }

            FGamePlatformAuthProviderResult Result;
            if (Response.bTransportSuccess &&
                Response.HttpCode == 200 &&
                Self->ParseAndCommitLoginResponse(Response.Body, Result) &&
                Result.AccountId == PreviousAccountId &&
                Result.SessionId == PreviousSessionId)
            {
                Result.bSuccess = true;
                Result.Error = EGamePlatformAuthError::None;
            }
            else
            {
                // RefreshToken是轮换凭据；超时/响应丢失后旧Token可能已被消费，禁止盲重放。
                Self->ClearTokens();
                Result = {};
                Result.Error = MapAuthError(
                    Response.HttpCode,
                    Response.bTransportSuccess,
                    true,
                    Response.bResponseTooLarge);
                if (Response.bMayHaveReachedServer &&
                    (!Response.bTransportSuccess ||
                     Response.bResponseTooLarge ||
                     Response.HttpCode == 200))
                {
                    // 轮换请求可能已提交但结果不可验证时，旧RefreshToken是否仍可用不可证明。
                    Result.Error = EGamePlatformAuthError::OutcomeUnknown;
                }
            }

            if (Completion)
            {
                Completion(MoveTemp(Result));
            }
        });
}

void FGamePlatformGatewayAuthProvider::Logout(
    FGamePlatformAuthLogoutCompletion Completion)
{
    ++OperationGeneration;
    const FString RefreshTokenSnapshot = MoveTemp(RefreshToken);

    // 本地退出先于远端撤销完成；无论远端结果如何都绝不恢复旧Token。
    ClearTokens();

    if (!bConfigured || RefreshTokenSnapshot.IsEmpty())
    {
        FGamePlatformAuthProviderLogoutResult Result;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Result]() mutable
            {
                if (Completion)
                {
                    Completion(Result);
                }
            });
        return;
    }

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("refreshToken"), RefreshTokenSnapshot);
    FString Serialized;
    if (!SerializeObject(Body, Serialized))
    {
        FGamePlatformAuthProviderLogoutResult Result;
        Result.Error = EGamePlatformAuthError::InvalidRequest;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Result]() mutable
            {
                if (Completion)
                {
                    Completion(Result);
                }
            });
        return;
    }

    const FGuid RequestId = FGuid::NewGuid();
    SendRequest(
        RequestId,
        TEXT("POST"),
        TEXT("/v1/auth/logout"),
        Serialized,
        FString(),
        FString(),
        [Completion = MoveTemp(Completion)](FRawResponse Response) mutable
        {
            FGamePlatformAuthProviderLogoutResult Result;
            Result.bServerRevoked =
                Response.bTransportSuccess && Response.HttpCode == 204;
            if (!Result.bServerRevoked)
            {
                Result.Error = MapAuthError(
                    Response.HttpCode,
                    Response.bTransportSuccess,
                    false,
                    Response.bResponseTooLarge);
            }
            if (Completion)
            {
                Completion(MoveTemp(Result));
            }
        });
}

void FGamePlatformGatewayAuthProvider::SendUnauthenticatedRequest(
    const FGuid& RequestId,
    const FGamePlatformAuthenticatedRequest& Request,
    FGamePlatformAuthenticatedCompletion Completion)
{
    if (!bConfigured ||
        !RequestId.IsValid() ||
        !IsRelativePathSafe(Request.RelativePath))
    {
        FGamePlatformAuthenticatedResponse Result;
        Result.Error = EGamePlatformAuthError::InvalidRequest;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Result = MoveTemp(Result)]() mutable
            {
                if (Completion)
                {
                    Completion(MoveTemp(Result));
                }
            });
        return;
    }

    SendRequest(
        RequestId,
        Request.Verb,
        Request.RelativePath,
        Request.Body,
        FString(),
        Request.IdempotencyKey,
        [Completion = MoveTemp(Completion)](FRawResponse Response) mutable
        {
            FGamePlatformAuthenticatedResponse Result;
            Result.HttpStatusCode = Response.HttpCode;
            Result.Body = MoveTemp(Response.Body);
            Result.bMayHaveReachedServer = Response.bMayHaveReachedServer;
            Result.RetryAfterSeconds = Response.RetryAfterSeconds;
            Result.Error = MapRequestError(Response);
            if (Completion)
            {
                Completion(MoveTemp(Result));
            }
        });
}

void FGamePlatformGatewayAuthProvider::SendAuthenticatedRequest(
    const FGuid& RequestId,
    const FGamePlatformAuthenticatedRequest& Request,
    FGamePlatformAuthenticatedCompletion Completion)
{
    if (!bConfigured ||
        AccessToken.IsEmpty() ||
        AccountId.IsEmpty() ||
        !RequestId.IsValid() ||
        !IsRelativePathSafe(Request.RelativePath))
    {
        FGamePlatformAuthenticatedResponse Result;
        Result.Error = AccessToken.IsEmpty()
            ? EGamePlatformAuthError::AuthExpired
            : EGamePlatformAuthError::InvalidRequest;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Result = MoveTemp(Result)]() mutable
            {
                if (Completion)
                {
                    Completion(MoveTemp(Result));
                }
            });
        return;
    }

    const FString Authorization =
        FString::Printf(TEXT("Bearer %s"), *AccessToken);
    const bool bSafeRead =
        Request.Verb == TEXT("GET") || Request.Verb == TEXT("HEAD");
    const bool bUnsafeWrite = !bSafeRead && !Request.bIdempotent;
    const FString IdempotencyKey = Request.IdempotencyKey;
    const TWeakPtr<FGamePlatformGatewayAuthProvider> WeakThis = AsShared();
    SendRequest(
        RequestId,
        Request.Verb,
        Request.RelativePath,
        Request.Body,
        Authorization,
        IdempotencyKey,
        [WeakThis, bUnsafeWrite, Completion = MoveTemp(Completion)](
            FRawResponse Response) mutable
        {
            if (!WeakThis.IsValid())
            {
                return;
            }

            FGamePlatformAuthenticatedResponse Result;
            Result.HttpStatusCode = Response.HttpCode;
            Result.Body = MoveTemp(Response.Body);
            Result.bMayHaveReachedServer = Response.bMayHaveReachedServer;
            Result.RetryAfterSeconds = Response.RetryAfterSeconds;
            Result.Error = MapRequestError(Response);
            // 非幂等写在已交给HTTP栈后若连接失败，不能证明服务端没有提交；
            // 返回 OutcomeUnknown，禁止业务层按普通网络失败盲目重放。
            if (bUnsafeWrite &&
                Response.bMayHaveReachedServer &&
                !Response.bTransportSuccess)
            {
                Result.Error = EGamePlatformAuthError::OutcomeUnknown;
            }
            if (Completion)
            {
                Completion(MoveTemp(Result));
            }
        });
}

void FGamePlatformGatewayAuthProvider::CancelRequest(
    const FGuid& RequestId)
{
    if (TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>* Request =
            ActiveRequests.Find(RequestId))
    {
        if (Request->IsValid())
        {
            (*Request)->CancelRequest();
        }
        ActiveRequests.Remove(RequestId);
    }
}

void FGamePlatformGatewayAuthProvider::InvalidateAuthenticationOperation()
{
    // 只推进认证操作代次，使迟到Login/AutoLogin结果在提交Token前被拒绝；
    // 不调用CancelAll，避免误伤同一GameInstance中的探测或业务请求。
    ++OperationGeneration;
    ClearTokens();
}

bool FGamePlatformGatewayAuthProvider::ApplyAuthorization(
    IHttpRequest& Request) const
{
    if (!bConfigured || AccessToken.IsEmpty())
    {
        return false;
    }
    Request.SetHeader(
        TEXT("Authorization"),
        FString::Printf(TEXT("Bearer %s"), *AccessToken));
    return true;
}

void FGamePlatformGatewayAuthProvider::CancelAll()
{
    ++OperationGeneration;
    for (const TPair<FGuid, TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>>& Pair :
         ActiveRequests)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->CancelRequest();
        }
    }
    ActiveRequests.Reset();
    ClearTokens();
}

void FGamePlatformGatewayAuthProvider::SendRequest(
    const FGuid& RequestId,
    const FString& Verb,
    const FString& RelativePath,
    const FString& Body,
    const FString& Authorization,
    const FString& IdempotencyKey,
    FRawCompletion Completion)
{
    FRawResponse Immediate;
    if (!bConfigured ||
        !RequestId.IsValid() ||
        !IsRelativePathSafe(RelativePath) ||
        !(Verb == TEXT("GET") ||
          Verb == TEXT("HEAD") ||
          Verb == TEXT("POST") ||
          Verb == TEXT("PATCH") ||
          Verb == TEXT("PUT") ||
          Verb == TEXT("DELETE")) ||
        (Body.IsEmpty()
            ? 0
            : FTCHARToUTF8(*Body).Length()) >
            Configuration.MaxRequestBytes)
    {
        Immediate.HttpCode = 400;
        Immediate.bTransportSuccess = true;
        Immediate.bMayHaveReachedServer = false;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Immediate = MoveTemp(Immediate)]() mutable
            {
                if (Completion)
                {
                    Completion(MoveTemp(Immediate));
                }
            });
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
    {
        Immediate.bTransportSuccess = false;
        Immediate.bMayHaveReachedServer = false;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Immediate = MoveTemp(Immediate)]() mutable
            {
                if (Completion)
                {
                    Completion(MoveTemp(Immediate));
                }
            });
        return;
    }
#else
    // 敏感认证请求必须具备“禁止自动重定向”的可验证能力；未知HTTP后端Fail Closed。
    Immediate.bTransportSuccess = false;
    Immediate.bMayHaveReachedServer = false;
    AsyncTask(
        ENamedThreads::GameThread,
        [Completion = MoveTemp(Completion), Immediate = MoveTemp(Immediate)]() mutable
        {
            if (Completion)
            {
                Completion(MoveTemp(Immediate));
            }
        });
    return;
#endif

    const TSharedRef<FResponseBodyState, ESPMode::ThreadSafe> BodyState =
        MakeShared<FResponseBodyState, ESPMode::ThreadSafe>();
    FHttpRequestStreamDelegateV2 StreamDelegate =
        FHttpRequestStreamDelegateV2::CreateLambda(
            [BodyState, MaxBytes = Configuration.MaxResponseBytes](
                void* Data,
                int64& InOutLength)
            {
                if (!Data || InOutLength <= 0)
                {
                    return;
                }

                FScopeLock Lock(&BodyState->Mutex);
                const int64 Current = BodyState->Bytes.Num();
                if (Current + InOutLength > MaxBytes)
                {
                    BodyState->bOverflow = true;
                    InOutLength = 0; // 令FArchive进入错误态并终止继续接收正文。
                    return;
                }
                BodyState->Bytes.Append(
                    static_cast<const uint8*>(Data),
                    static_cast<int32>(InOutLength));
            });

    if (!Request->SetResponseBodyReceiveStreamDelegateV2(
            MoveTemp(StreamDelegate)))
    {
        Immediate.bTransportSuccess = false;
        Immediate.bMayHaveReachedServer = false;
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion), Immediate = MoveTemp(Immediate)]() mutable
            {
                if (Completion)
                {
                    Completion(MoveTemp(Immediate));
                }
            });
        return;
    }

    Request->SetURL(Configuration.ServiceOrigin + RelativePath);
    Request->SetVerb(Verb);
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    if (!Authorization.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), Authorization);
    }
    if (!IdempotencyKey.IsEmpty())
    {
        Request->SetHeader(TEXT("Idempotency-Key"), IdempotencyKey);
    }
    if (!Body.IsEmpty())
    {
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetContentAsString(Body);
    }
    Request->SetTimeout(
        static_cast<float>(Configuration.AttemptTimeoutSeconds));

    struct FCompletionGate
    {
        FCriticalSection Mutex;
        bool bCompleted = false;
        FRawCompletion Completion;
    };

    const TSharedRef<FCompletionGate, ESPMode::ThreadSafe> Gate =
        MakeShared<FCompletionGate, ESPMode::ThreadSafe>();
    Gate->Completion = MoveTemp(Completion);

    ActiveRequests.Add(RequestId, Request);
    const TWeakPtr<FGamePlatformGatewayAuthProvider> WeakThis = AsShared();

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis,
         RequestId,
         BodyState,
         Gate](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bTransportSuccess) mutable
        {
            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis,
                 RequestId,
                 BodyState,
                 Gate,
                 Response,
                 bTransportSuccess]() mutable
                {
                    const TSharedPtr<FGamePlatformGatewayAuthProvider> Self =
                        WeakThis.Pin();
                    if (!Self.IsValid())
                    {
                        return;
                    }

                    Self->ActiveRequests.Remove(RequestId);

                    FRawResponse Result;
                    Result.bTransportSuccess =
                        bTransportSuccess && Response.IsValid();
                    Result.bMayHaveReachedServer = true;
                    Result.HttpCode = Response.IsValid()
                        ? Response->GetResponseCode()
                        : 0;
                    if (Response.IsValid())
                    {
                        const FString RetryAfter =
                            Response->GetHeader(TEXT("Retry-After"));
                        double ParsedRetryAfter = 0.0;
                        if (!RetryAfter.IsEmpty() &&
                            LexTryParseString(
                                ParsedRetryAfter,
                                *RetryAfter) &&
                            FMath::IsFinite(ParsedRetryAfter) &&
                            ParsedRetryAfter >= 0.0)
                        {
                            Result.RetryAfterSeconds =
                                ParsedRetryAfter;
                        }
                    }

                    TArray<uint8> Bytes;
                    {
                        FScopeLock Lock(&BodyState->Mutex);
                        Result.bResponseTooLarge = BodyState->bOverflow;
                        Bytes = MoveTemp(BodyState->Bytes);
                    }
                    if (!Result.bResponseTooLarge)
                    {
                        Result.Body = Utf8BytesToString(MoveTemp(Bytes));
                    }

                    FRawCompletion Done;
                    {
                        FScopeLock Lock(&Gate->Mutex);
                        if (Gate->bCompleted)
                        {
                            return;
                        }
                        Gate->bCompleted = true;
                        Done = MoveTemp(Gate->Completion);
                    }
                    if (Done)
                    {
                        Done(MoveTemp(Result));
                    }
                });
        });

    if (!Request->ProcessRequest())
    {
        ActiveRequests.Remove(RequestId);
        Immediate.bTransportSuccess = false;
        Immediate.bMayHaveReachedServer = false;
        AsyncTask(
            ENamedThreads::GameThread,
            [Gate, Immediate = MoveTemp(Immediate)]() mutable
            {
                FRawCompletion Done;
                {
                    FScopeLock Lock(&Gate->Mutex);
                    if (Gate->bCompleted)
                    {
                        return;
                    }
                    Gate->bCompleted = true;
                    Done = MoveTemp(Gate->Completion);
                }
                if (Done)
                {
                    Done(MoveTemp(Immediate));
                }
            });
    }
}

bool FGamePlatformGatewayAuthProvider::ParseAndCommitLoginResponse(
    const FString& ResponseText,
    FGamePlatformAuthProviderResult& OutResult)
{
    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(ResponseText);
    if (!FJsonSerializer::Deserialize(Reader, Json) ||
        !Json.IsValid())
    {
        return false;
    }

    FString PlayerId;
    FString NewAccessToken;
    FString NewRefreshToken;
    FString NewSessionId;
    FString ExpiresAtText;
    FString RefreshExpiresAtText;
    if (!Json->TryGetStringField(TEXT("playerId"), PlayerId) ||
        !Json->TryGetStringField(TEXT("accessToken"), NewAccessToken) ||
        !Json->TryGetStringField(TEXT("refreshToken"), NewRefreshToken) ||
        !Json->TryGetStringField(TEXT("sessionId"), NewSessionId) ||
        !Json->TryGetStringField(TEXT("expiresAt"), ExpiresAtText) ||
        !Json->TryGetStringField(TEXT("refreshExpiresAt"), RefreshExpiresAtText) ||
        PlayerId.IsEmpty() ||
        PlayerId.Len() > MaxIdentityChars ||
        NewSessionId.IsEmpty() ||
        NewSessionId.Len() > MaxIdentityChars ||
        NewAccessToken.IsEmpty() ||
        NewAccessToken.Len() > MaxTokenChars ||
        NewRefreshToken.IsEmpty() ||
        NewRefreshToken.Len() > MaxTokenChars)
    {
        return false;
    }

    FDateTime NewAccessExpiry;
    FDateTime NewRefreshExpiry;
    if (!FDateTime::ParseIso8601(*ExpiresAtText, NewAccessExpiry) ||
        !FDateTime::ParseIso8601(*RefreshExpiresAtText, NewRefreshExpiry) ||
        NewAccessExpiry <= FDateTime::UtcNow() ||
        NewRefreshExpiry <= NewAccessExpiry)
    {
        return false;
    }

    AccessToken = MoveTemp(NewAccessToken);
    RefreshToken = MoveTemp(NewRefreshToken);
    AccountId = MoveTemp(PlayerId);
    SessionId = MoveTemp(NewSessionId);
    AccessExpiresAt = NewAccessExpiry;
    RefreshExpiresAt = NewRefreshExpiry;

    OutResult.AccountId = AccountId;
    OutResult.SessionId = SessionId;
    OutResult.AccessExpiresAt = AccessExpiresAt;
    OutResult.RefreshExpiresAt = RefreshExpiresAt;
    return true;
}

void FGamePlatformGatewayAuthProvider::ClearTokens()
{
    AccessToken.Reset();
    RefreshToken.Reset();
    AccountId.Reset();
    SessionId.Reset();
    AccessExpiresAt = FDateTime();
    RefreshExpiresAt = FDateTime();
}

bool FGamePlatformGatewayAuthProvider::IsRelativePathSafe(
    const FString& RelativePath)
{
    return RelativePath.StartsWith(TEXT("/")) &&
        !RelativePath.StartsWith(TEXT("//")) &&
        !RelativePath.Contains(TEXT("://")) &&
        !RelativePath.Contains(TEXT("\\")) &&
        !RelativePath.Contains(TEXT("\r")) &&
        !RelativePath.Contains(TEXT("\n")) &&
        !RelativePath.Contains(TEXT("\t")) &&
        !RelativePath.Contains(TEXT(" ")) &&
        RelativePath.Len() <= 2048;
}

EGamePlatformAuthError
FGamePlatformGatewayAuthProvider::MapAuthError(
    int32 HttpCode,
    bool bTransportSuccess,
    bool bRefreshOperation,
    bool bResponseTooLarge)
{
    if (bResponseTooLarge)
    {
        return EGamePlatformAuthError::InvalidResponse;
    }
    if (!bTransportSuccess)
    {
        return bRefreshOperation
            ? EGamePlatformAuthError::AuthExpired
            : EGamePlatformAuthError::NetworkUnavailable;
    }
    if (HttpCode == 401)
    {
        return bRefreshOperation
            ? EGamePlatformAuthError::AuthExpired
            : EGamePlatformAuthError::InvalidCredentials;
    }
    if (HttpCode == 403 || HttpCode == 423)
    {
        return EGamePlatformAuthError::AccountLocked;
    }
    if (HttpCode == 408)
    {
        return EGamePlatformAuthError::TimedOut;
    }
    if (HttpCode == 429)
    {
        return EGamePlatformAuthError::RateLimited;
    }
    if (HttpCode == 503 || HttpCode >= 500)
    {
        return EGamePlatformAuthError::ServiceUnavailable;
    }
    if (HttpCode >= 400 && HttpCode < 500)
    {
        return EGamePlatformAuthError::ContractIncompatible;
    }
    return EGamePlatformAuthError::Unknown;
}

EGamePlatformAuthError
FGamePlatformGatewayAuthProvider::MapRequestError(
    const FRawResponse& Response)
{
    if (Response.bResponseTooLarge)
    {
        return EGamePlatformAuthError::InvalidResponse;
    }
    if (!Response.bTransportSuccess)
    {
        return EGamePlatformAuthError::NetworkUnavailable;
    }
    if (Response.HttpCode >= 200 && Response.HttpCode < 300)
    {
        return EGamePlatformAuthError::None;
    }
    switch (Response.HttpCode)
    {
    case 400: return EGamePlatformAuthError::InvalidRequest;
    case 401: return EGamePlatformAuthError::AuthExpired;
    case 403: return EGamePlatformAuthError::Forbidden;
    case 404: return EGamePlatformAuthError::NotFound;
    case 408: return EGamePlatformAuthError::TimedOut;
    case 409: return EGamePlatformAuthError::Conflict;
    case 429: return EGamePlatformAuthError::RateLimited;
    case 503: return EGamePlatformAuthError::ServiceUnavailable;
    default:
        return Response.HttpCode >= 500
            ? EGamePlatformAuthError::ServiceUnavailable
            : EGamePlatformAuthError::Unknown;
    }
}
