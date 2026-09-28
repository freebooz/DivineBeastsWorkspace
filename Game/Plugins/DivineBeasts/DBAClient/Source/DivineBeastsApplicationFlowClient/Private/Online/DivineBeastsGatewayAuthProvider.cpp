#include "Online/DivineBeastsGatewayAuthProvider.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 MaxAuthResponseBytes = 256 * 1024;
constexpr int32 MaxCredentialRequestBytes = 16 * 1024;

void CompleteLater(
    FGamePlatformAuthCompletion Completion,
    bool bSuccess,
    FString AccountId,
    EGamePlatformAuthError Error)
{
    AsyncTask(
        ENamedThreads::GameThread,
        [Completion = MoveTemp(Completion),
         bSuccess,
         AccountId = MoveTemp(AccountId),
         Error]() mutable
        {
            if (Completion)
            {
                Completion(bSuccess, AccountId, Error);
            }
        });
}

bool SerializeJson(
    const TSharedRef<FJsonObject>& Body,
    FString& Out)
{
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&Out);
    return FJsonSerializer::Serialize(Body, Writer);
}
}

FDivineBeastsGatewayAuthProvider::FDivineBeastsGatewayAuthProvider()
{
    GatewayBaseUrl =
        FPlatformMisc::GetEnvironmentVariable(
            TEXT("DIVINEBEASTS_GATEWAY_BASE_URL"));
    GatewayBaseUrl.TrimStartAndEndInline();
    while (GatewayBaseUrl.RemoveFromEnd(TEXT("/")))
    {
    }

    const FString ConfiguredVersion =
        FPlatformMisc::GetEnvironmentVariable(
            TEXT("DIVINEBEASTS_CLIENT_VERSION"));
    if (!ConfiguredVersion.TrimStartAndEnd().IsEmpty())
    {
        ClientVersion = ConfiguredVersion.TrimStartAndEnd();
    }
}

FDivineBeastsGatewayAuthProvider::~FDivineBeastsGatewayAuthProvider()
{
    CancelAll();
}

void FDivineBeastsGatewayAuthProvider::TryAutoLogin(
    FGamePlatformAuthCompletion Completion)
{
    // 第一阶段明确不实现“记住密码/明文RefreshToken落盘”。
    // 后续若接入平台安全凭据存储，应在独立安全审查后替换本分支。
    CompleteLater(
        MoveTemp(Completion),
        false,
        FString(),
        EGamePlatformAuthError::AuthExpired);
}

void FDivineBeastsGatewayAuthProvider::LoginWithCredentials(
    const FString& LoginName,
    const FString& Password,
    FGamePlatformAuthCompletion Completion)
{
    const FString NormalizedLogin = LoginName.TrimStartAndEnd();
    if (!IsAllowedGatewayBaseUrl(GatewayBaseUrl) ||
        NormalizedLogin.IsEmpty() ||
        Password.IsEmpty())
    {
        CompleteLater(
            MoveTemp(Completion),
            false,
            FString(),
            NormalizedLogin.IsEmpty() || Password.IsEmpty()
                ? EGamePlatformAuthError::InvalidCredentials
                : EGamePlatformAuthError::ProviderUnavailable);
        return;
    }

    const uint64 Generation = ++OperationGeneration;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("gameId"), GameId);
    Body->SetStringField(TEXT("provider"), TEXT("password"));
    Body->SetStringField(TEXT("accountName"), NormalizedLogin);
    Body->SetStringField(TEXT("credential"), Password);
    Body->SetStringField(TEXT("clientVersion"), ClientVersion);
    Body->SetStringField(TEXT("deviceId"), FString());

    const TWeakPtr<FDivineBeastsGatewayAuthProvider> WeakThis =
        AsShared();
    SendJsonPost(
        TEXT("/v1/auth/login"),
        Body,
        [WeakThis, Generation, Completion = MoveTemp(Completion)](
            int32 HttpCode,
            const FString& ResponseText,
            bool bTransportSuccess) mutable
        {
            const TSharedPtr<FDivineBeastsGatewayAuthProvider> Self =
                WeakThis.Pin();
            if (!Self.IsValid() ||
                Self->OperationGeneration != Generation)
            {
                return;
            }

            FString PlayerId;
            if (bTransportSuccess &&
                HttpCode == 200 &&
                Self->ParseAndCommitLoginResponse(
                    ResponseText,
                    PlayerId))
            {
                if (Completion)
                {
                    Completion(
                        true,
                        PlayerId,
                        EGamePlatformAuthError::None);
                }
                return;
            }

            // 登录失败时不保留任何上一次认证材料，避免账号切换或错误响应污染新上下文。
            Self->AccessToken.Reset();
            Self->RefreshToken.Reset();
            Self->AccountId.Reset();

            if (Completion)
            {
                Completion(
                    false,
                    FString(),
                    MapAuthError(
                        HttpCode,
                        bTransportSuccess,
                        false));
            }
        });
}

void FDivineBeastsGatewayAuthProvider::Refresh(
    FGamePlatformAuthCompletion Completion)
{
    if (!IsAllowedGatewayBaseUrl(GatewayBaseUrl) ||
        RefreshToken.IsEmpty() ||
        AccountId.IsEmpty())
    {
        CompleteLater(
            MoveTemp(Completion),
            false,
            FString(),
            EGamePlatformAuthError::AuthExpired);
        return;
    }

    const uint64 Generation = ++OperationGeneration;
    const FString PreviousPlayerId = AccountId;
    const FString RefreshTokenSnapshot = RefreshToken;

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("refreshToken"),
        RefreshTokenSnapshot);

    const TWeakPtr<FDivineBeastsGatewayAuthProvider> WeakThis =
        AsShared();
    SendJsonPost(
        TEXT("/v1/auth/refresh"),
        Body,
        [WeakThis,
         Generation,
         PreviousPlayerId,
         Completion = MoveTemp(Completion)](
            int32 HttpCode,
            const FString& ResponseText,
            bool bTransportSuccess) mutable
        {
            const TSharedPtr<FDivineBeastsGatewayAuthProvider> Self =
                WeakThis.Pin();
            if (!Self.IsValid() ||
                Self->OperationGeneration != Generation)
            {
                return;
            }

            FString PlayerId;
            if (bTransportSuccess &&
                HttpCode == 200 &&
                Self->ParseAndCommitLoginResponse(
                    ResponseText,
                    PlayerId) &&
                PlayerId == PreviousPlayerId)
            {
                if (Completion)
                {
                    Completion(
                        true,
                        PlayerId,
                        EGamePlatformAuthError::None);
                }
                return;
            }

            // RefreshToken 为轮换凭据。任何失败/超时/响应丢失都可能意味着旧令牌已经被服务端消费，
            // 因此必须清空本地认证并要求重新登录，禁止自动重放旧 RefreshToken。
            Self->AccessToken.Reset();
            Self->RefreshToken.Reset();
            Self->AccountId.Reset();

            if (Completion)
            {
                Completion(
                    false,
                    FString(),
                    MapAuthError(
                        HttpCode,
                        bTransportSuccess,
                        true));
            }
        });
}

void FDivineBeastsGatewayAuthProvider::Logout(
    TFunction<void()> Completion)
{
    const uint64 Generation = ++OperationGeneration;

    // 先完成本地退出。即使后续 Gateway 不可达，也绝不能恢复旧认证材料。
    const FString RefreshTokenSnapshot = MoveTemp(RefreshToken);
    AccessToken.Reset();
    RefreshToken.Reset();
    AccountId.Reset();

    if (!IsAllowedGatewayBaseUrl(GatewayBaseUrl) ||
        RefreshTokenSnapshot.IsEmpty())
    {
        AsyncTask(
            ENamedThreads::GameThread,
            [Completion = MoveTemp(Completion)]() mutable
            {
                if (Completion)
                {
                    Completion();
                }
            });
        return;
    }

    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(
        TEXT("refreshToken"),
        RefreshTokenSnapshot);

    const TWeakPtr<FDivineBeastsGatewayAuthProvider> WeakThis =
        AsShared();
    SendJsonPost(
        TEXT("/v1/auth/logout"),
        Body,
        [WeakThis, Generation, Completion = MoveTemp(Completion)](
            int32,
            const FString&,
            bool) mutable
        {
            const TSharedPtr<FDivineBeastsGatewayAuthProvider> Self =
                WeakThis.Pin();
            if (!Self.IsValid() ||
                Self->OperationGeneration != Generation)
            {
                return;
            }

            // Provider 接口没有远端撤销结果通道；项目公开状态只确认本地已退出。
            // 服务端撤销失败不会让旧令牌重新进入内存。
            if (Completion)
            {
                Completion();
            }
        });
}

FString
FDivineBeastsGatewayAuthProvider::GetAuthorizationHeaderValue() const
{
    return AccessToken.IsEmpty()
        ? FString()
        : FString::Printf(TEXT("Bearer %s"), *AccessToken);
}

void FDivineBeastsGatewayAuthProvider::SendJsonPost(
    const FString& RelativePath,
    const TSharedRef<FJsonObject>& Body,
    FRawHttpCompletion Completion)
{
    if (!IsAllowedGatewayBaseUrl(GatewayBaseUrl) ||
        RelativePath.IsEmpty() ||
        !RelativePath.StartsWith(TEXT("/")))
    {
        if (Completion)
        {
            Completion(0, FString(), false);
        }
        return;
    }

    FString Serialized;
    if (!SerializeJson(Body, Serialized) ||
        Serialized.IsEmpty() ||
        Serialized.Len() > MaxCredentialRequestBytes)
    {
        if (Completion)
        {
            Completion(400, FString(), true);
        }
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    Request->SetURL(GatewayBaseUrl + RelativePath);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(
        TEXT("Content-Type"),
        TEXT("application/json"));
    Request->SetHeader(
        TEXT("Accept"),
        TEXT("application/json"));
    Request->SetContentAsString(Serialized);
    Request->SetTimeout(10.0f);

    ActiveRequests.Add(Request);
    const TWeakPtr<FDivineBeastsGatewayAuthProvider> WeakThis =
        AsShared();

    Request->OnProcessRequestComplete().BindLambda(
        [WeakThis, Completion = MoveTemp(Completion)](
            FHttpRequestPtr CompletedRequest,
            FHttpResponsePtr Response,
            bool bTransportSuccess) mutable
        {
            const TSharedPtr<FDivineBeastsGatewayAuthProvider> Self =
                WeakThis.Pin();
            if (!Self.IsValid())
            {
                return;
            }

            Self->ActiveRequests.Remove(CompletedRequest);

            if (!bTransportSuccess ||
                !Response.IsValid())
            {
                if (Completion)
                {
                    Completion(0, FString(), false);
                }
                return;
            }

            const FString ResponseText =
                Response->GetContentAsString();
            if (ResponseText.Len() > MaxAuthResponseBytes)
            {
                if (Completion)
                {
                    Completion(
                        Response->GetResponseCode(),
                        FString(),
                        false);
                }
                return;
            }

            if (Completion)
            {
                Completion(
                    Response->GetResponseCode(),
                    ResponseText,
                    true);
            }
        });

    if (!Request->ProcessRequest())
    {
        ActiveRequests.Remove(Request);
        if (Completion)
        {
            Completion(0, FString(), false);
        }
    }
}

bool FDivineBeastsGatewayAuthProvider::ParseAndCommitLoginResponse(
    const FString& ResponseText,
    FString& OutPlayerId)
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
    FString SessionId;
    FString ExpiresAtText;
    FString RefreshExpiresAtText;

    if (!Json->TryGetStringField(TEXT("playerId"), PlayerId) ||
        !Json->TryGetStringField(
            TEXT("accessToken"),
            NewAccessToken) ||
        !Json->TryGetStringField(
            TEXT("refreshToken"),
            NewRefreshToken) ||
        !Json->TryGetStringField(
            TEXT("sessionId"),
            SessionId) ||
        !Json->TryGetStringField(
            TEXT("expiresAt"),
            ExpiresAtText) ||
        !Json->TryGetStringField(
            TEXT("refreshExpiresAt"),
            RefreshExpiresAtText) ||
        PlayerId.IsEmpty() ||
        NewAccessToken.IsEmpty() ||
        NewRefreshToken.IsEmpty() ||
        SessionId.IsEmpty())
    {
        return false;
    }

    FDateTime AccessExpiry;
    FDateTime RefreshExpiry;
    if (!FDateTime::ParseIso8601(*ExpiresAtText, AccessExpiry) ||
        !FDateTime::ParseIso8601(
            *RefreshExpiresAtText,
            RefreshExpiry) ||
        AccessExpiry <= FDateTime::UtcNow() ||
        RefreshExpiry <= AccessExpiry)
    {
        return false;
    }

    // 只有完整响应通过结构与时间校验后才一次性替换认证材料，避免半提交状态。
    AccessToken = MoveTemp(NewAccessToken);
    RefreshToken = MoveTemp(NewRefreshToken);
    AccountId = PlayerId;
    OutPlayerId = MoveTemp(PlayerId);
    return true;
}

void FDivineBeastsGatewayAuthProvider::CancelAll()
{
    ++OperationGeneration;
    for (const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request :
         ActiveRequests)
    {
        if (Request.IsValid())
        {
            Request->CancelRequest();
        }
    }
    ActiveRequests.Reset();

    AccessToken.Reset();
    RefreshToken.Reset();
    AccountId.Reset();
}

bool FDivineBeastsGatewayAuthProvider::IsAllowedGatewayBaseUrl(
    const FString& Value)
{
    const FString Url = Value.TrimStartAndEnd();
    if (Url.IsEmpty() ||
        Url.Contains(TEXT("@")) ||
        Url.Contains(TEXT("?")) ||
        Url.Contains(TEXT("#")) ||
        Url.Contains(TEXT("\\")))
    {
        return false;
    }

    if (Url.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
    {
        const FString Authority = Url.Mid(8);
        return !Authority.IsEmpty() &&
            !Authority.Contains(TEXT("/"));
    }

#if !UE_BUILD_SHIPPING
    if (Url.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase))
    {
        const FString Authority = Url.Mid(7);
        if (Authority.IsEmpty() ||
            Authority.Contains(TEXT("/")))
        {
            return false;
        }

        return Authority == TEXT("127.0.0.1") ||
            Authority.StartsWith(TEXT("127.0.0.1:")) ||
            Authority == TEXT("[::1]") ||
            Authority.StartsWith(TEXT("[::1]:"));
    }
#endif

    return false;
}

EGamePlatformAuthError
FDivineBeastsGatewayAuthProvider::MapAuthError(
    int32 HttpCode,
    bool bTransportSuccess,
    bool bRefreshOperation)
{
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
    if (HttpCode == 403)
    {
        // Gateway 当前把 AUTH_ACCOUNT_DISABLED 映射为 403。
        // 平台已有 AccountLocked（账号不可用）稳定状态，第一阶段统一收敛到该状态，
        // 不把后端明确拒绝账号误判为客户端契约版本错误。
        return EGamePlatformAuthError::AccountLocked;
    }
    if (HttpCode == 423)
    {
        return EGamePlatformAuthError::AccountLocked;
    }
    if (HttpCode == 503)
    {
        return EGamePlatformAuthError::Maintenance;
    }
    if (HttpCode == 408 ||
        HttpCode == 429 ||
        HttpCode >= 500)
    {
        return EGamePlatformAuthError::NetworkUnavailable;
    }
    if (HttpCode >= 400 &&
        HttpCode < 500)
    {
        return EGamePlatformAuthError::ContractIncompatible;
    }

    return EGamePlatformAuthError::Unknown;
}
