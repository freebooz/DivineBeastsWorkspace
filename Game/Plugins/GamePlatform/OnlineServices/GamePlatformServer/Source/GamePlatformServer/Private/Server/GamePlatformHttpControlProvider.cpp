#include "Server/GamePlatformHttpControlProvider.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/ThreadSafeCounter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr float DefaultRequestTimeoutSeconds = 5.0f;
    constexpr int32 MaxAcceptedResponseBytes = 16 * 1024;

    float ResolveRequestTimeoutSeconds()
    {
        const FString Configured = FPlatformMisc::GetEnvironmentVariable(
            TEXT("GAMESERVERCONTROL_REQUEST_TIMEOUT_SECONDS"));
        if (Configured.IsEmpty())
        {
            return DefaultRequestTimeoutSeconds;
        }
        float Parsed = 0.0f;
        if (!FDefaultValueHelper::ParseFloat(Configured, Parsed) || !FMath::IsFinite(Parsed))
        {
            return DefaultRequestTimeoutSeconds;
        }
        return FMath::Clamp(Parsed, 1.0f, 30.0f);
    }

    bool IsAllowedBaseUrl(const FString& BaseUrl)
    {
#if UE_BUILD_SHIPPING
        // Shipping携带服务器Bearer凭据，必须直接使用TLS端点；开发环境允许本机HTTP联调。
        return BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#else
        return BaseUrl.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase) ||
            BaseUrl.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase);
#endif
    }

    struct FResponseBodyState
    {
        FCriticalSection Mutex;
        TArray<uint8> Bytes;
        bool bOverflow = false;
    };

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

    bool IsRetryableHttpCode(int32 Code)
    {
        return Code == 408 || Code == 425 || Code == 429 || Code >= 500;
    }

    TSharedRef<FJsonObject> IdentifierBody(const FString& GameServerId)
    {
        TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
        Body->SetStringField(TEXT("gameServerId"), GameServerId);
        return Body;
    }

    FString SerializeBody(const TSharedRef<FJsonObject>& Body)
    {
        FString Content;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Content);
        FJsonSerializer::Serialize(Body, Writer);
        return Content;
    }
}

void FGamePlatformHttpControlProvider::RegisterInstance(
    const FGamePlatformServerInstanceInfo& Instance,
    FGamePlatformServerControlCompletion Completion)
{
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("gameId"), Instance.GameId);
    Body->SetStringField(TEXT("gameServerId"), Instance.GameServerId);
    Body->SetStringField(TEXT("serverBootId"), Instance.ServerBootId);
    Body->SetStringField(TEXT("serverRoleId"), Instance.ServerRoleId);
    Body->SetStringField(TEXT("experienceId"), Instance.ExperienceId);
    Body->SetStringField(TEXT("worldId"), Instance.WorldId);
    Body->SetStringField(TEXT("regionId"), Instance.RegionId);
    Body->SetStringField(TEXT("clusterId"), Instance.ClusterId);
    Body->SetStringField(TEXT("nodeId"), Instance.NodeId);
    Body->SetStringField(TEXT("buildVersion"), Instance.BuildVersion);
    Body->SetNumberField(TEXT("protocolVersion"), Instance.ProtocolVersion);
    Body->SetStringField(TEXT("publicEndpoint"), Instance.PublicEndpoint);
    Body->SetNumberField(TEXT("capacity"), Instance.Capacity);
    SendAcceptedPost(Instance, TEXT("/internal/v1/gameservers/register"), Body, MoveTemp(Completion));
}

void FGamePlatformHttpControlProvider::SendHeartbeat(
    const FGamePlatformServerInstanceInfo& Instance,
    int32 CurrentPlayers,
    const FString& Status,
    FGamePlatformServerControlCompletion Completion)
{
    TSharedRef<FJsonObject> Body = IdentifierBody(Instance.GameServerId);
    Body->SetNumberField(TEXT("currentPlayers"), CurrentPlayers);
    Body->SetStringField(TEXT("status"), Status);
    SendAcceptedPost(Instance, TEXT("/internal/v1/gameservers/heartbeat"), Body, MoveTemp(Completion));
}

void FGamePlatformHttpControlProvider::PublishReady(
    const FGamePlatformServerInstanceInfo& Instance,
    FGamePlatformServerControlCompletion Completion)
{
    SendAcceptedPost(
        Instance,
        TEXT("/internal/v1/gameservers/ready"),
        IdentifierBody(Instance.GameServerId),
        MoveTemp(Completion));
}

void FGamePlatformHttpControlProvider::BeginDrain(
    const FGamePlatformServerInstanceInfo& Instance,
    FGamePlatformServerControlCompletion Completion)
{
    SendAcceptedPost(
        Instance,
        TEXT("/internal/v1/gameservers/drain"),
        IdentifierBody(Instance.GameServerId),
        MoveTemp(Completion));
}

void FGamePlatformHttpControlProvider::SendAcceptedPost(
    const FGamePlatformServerInstanceInfo& Instance,
    const FString& RelativePath,
    const TSharedRef<FJsonObject>& Body,
    FGamePlatformServerControlCompletion Completion)
{
    FString BaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_BASE_URL"));
    const FString InternalToken = FPlatformMisc::GetEnvironmentVariable(TEXT("GAMESERVERCONTROL_INTERNAL_TOKEN"));
    const FString ConfiguredServerId = FPlatformMisc::GetEnvironmentVariable(TEXT("GAME_SERVER_ID"));
    BaseUrl.RemoveFromEnd(TEXT("/"));
    if (BaseUrl.IsEmpty() || InternalToken.IsEmpty() ||
        ConfiguredServerId != Instance.GameServerId)
    {
        Completion(false, FName(TEXT("ControlPlaneConfigurationMissing")));
        return;
    }
    if (BaseUrl.Len() > 2048 || InternalToken.Len() > 4096 ||
        !IsAllowedBaseUrl(BaseUrl) || BaseUrl.Contains(TEXT("?")) ||
        BaseUrl.Contains(TEXT("#")) ||
        BaseUrl.Contains(TEXT("\r")) || BaseUrl.Contains(TEXT("\n")) ||
        InternalToken.Contains(TEXT("\r")) || InternalToken.Contains(TEXT("\n")))
    {
        Completion(false, FName(TEXT("ControlPlaneConfigurationInvalid")));
        return;
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
#if defined(UE_HTTP_HAS_REQUEST_REDIRECT_POLICY) && UE_HTTP_HAS_REQUEST_REDIRECT_POLICY
    // 生命周期请求携带Bearer凭据；禁止自动跨主机重定向，防止内部令牌被转发到非预期端点。
    if (!Request->SetRedirectPolicy(EHttpRequestRedirectPolicy::Reject))
    {
        Completion(false, FName(TEXT("ControlPlaneRedirectPolicyUnavailable")));
        return;
    }
#else
    Completion(false, FName(TEXT("ControlPlaneRedirectPolicyUnavailable")));
    return;
#endif
    const TSharedRef<FResponseBodyState, ESPMode::ThreadSafe> BodyState =
        MakeShared<FResponseBodyState, ESPMode::ThreadSafe>();
    FHttpRequestStreamDelegateV2 StreamDelegate =
        FHttpRequestStreamDelegateV2::CreateLambda(
            [BodyState](void* Data, int64& InOutLength)
            {
                if (!Data || InOutLength <= 0)
                {
                    return;
                }
                FScopeLock Lock(&BodyState->Mutex);
                const int64 CurrentBytes = BodyState->Bytes.Num();
                if (CurrentBytes + InOutLength > MaxAcceptedResponseBytes)
                {
                    BodyState->bOverflow = true;
                    InOutLength = 0; // 终止继续接收，防止异常控制面响应扩大服务器内存占用。
                    return;
                }
                BodyState->Bytes.Append(
                    static_cast<const uint8*>(Data),
                    static_cast<int32>(InOutLength));
            });
    if (!Request->SetResponseBodyReceiveStreamDelegateV2(MoveTemp(StreamDelegate)))
    {
        Completion(false, FName(TEXT("ControlPlaneResponseLimitUnavailable")));
        return;
    }

    Request->SetVerb(TEXT("POST"));
    Request->SetURL(BaseUrl + RelativePath);
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + InternalToken);
    Request->SetHeader(TEXT("X-Game-Server-Id"), Instance.GameServerId);
    Request->SetHeader(TEXT("X-Game-Server-Boot-Id"), Instance.ServerBootId);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Request->SetTimeout(ResolveRequestTimeoutSeconds());
    Request->SetContentAsString(SerializeBody(Body));
    const TSharedRef<FGamePlatformServerControlCompletion, ESPMode::ThreadSafe>
        SharedCompletion = MakeShared<FGamePlatformServerControlCompletion, ESPMode::ThreadSafe>(
            MoveTemp(Completion));
    const TSharedRef<FThreadSafeCounter, ESPMode::ThreadSafe> CompletionGate =
        MakeShared<FThreadSafeCounter, ESPMode::ThreadSafe>();
    const auto CompleteOnce = [SharedCompletion, CompletionGate](bool bSucceeded, FName ErrorCode)
    {
        if (CompletionGate->Increment() != 1)
        {
            return;
        }
        (*SharedCompletion)(bSucceeded, ErrorCode);
    };
    Request->OnProcessRequestComplete().BindLambda(
        [CompleteOnce, BodyState](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bSucceeded) mutable
        {
            TArray<uint8> ResponseBytes;
            bool bResponseOverflow = false;
            {
                FScopeLock Lock(&BodyState->Mutex);
                bResponseOverflow = BodyState->bOverflow;
                ResponseBytes = MoveTemp(BodyState->Bytes);
            }
            if (bResponseOverflow)
            {
                CompleteOnce(false, FName(TEXT("ControlPlaneResponseTooLarge")));
                return;
            }
            if (!bSucceeded || !Response.IsValid())
            {
                CompleteOnce(false, FName(TEXT("ControlPlaneTransportFailed")));
                return;
            }
            const int32 ResponseCode = Response->GetResponseCode();
            if (ResponseCode < 200 || ResponseCode >= 300)
            {
                // 不读取或记录响应体，避免服务端错误回显凭据或玩家数据。
                if (IsRetryableHttpCode(ResponseCode))
                {
                    CompleteOnce(false, FName(TEXT("ControlPlaneRetryableHttpStatus")));
                }
                else if (ResponseCode == 401 || ResponseCode == 403)
                {
                    CompleteOnce(false, FName(TEXT("ControlPlaneAuthorizationRejected")));
                }
                else
                {
                    CompleteOnce(false, FName(TEXT("ControlPlaneRejected")));
                }
                return;
            }

            TSharedPtr<FJsonObject> ResponseBody;
            const FString ResponseText = Utf8BytesToString(MoveTemp(ResponseBytes));
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(ResponseText);
            bool bAccepted = false;
            if (!FJsonSerializer::Deserialize(Reader, ResponseBody) ||
                !ResponseBody.IsValid() ||
                !ResponseBody->TryGetBoolField(TEXT("accepted"), bAccepted) ||
                !bAccepted)
            {
                CompleteOnce(false, FName(TEXT("ControlPlaneResponseInvalid")));
                return;
            }
            CompleteOnce(true, NAME_None);
        });

    if (!Request->ProcessRequest())
    {
        CompleteOnce(false, FName(TEXT("ControlPlaneRequestStartFailed")));
    }
}
