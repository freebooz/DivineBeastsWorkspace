#include "Server/GamePlatformHttpControlProvider.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HAL/ThreadSafeCounter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
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
        ConfiguredServerId != Instance.GameServerId ||
        BaseUrl.Contains(TEXT("\r")) || BaseUrl.Contains(TEXT("\n")) ||
        InternalToken.Contains(TEXT("\r")) || InternalToken.Contains(TEXT("\n")))
    {
        Completion(false, FName(TEXT("ControlPlaneConfigurationMissing")));
        return;
    }

    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();
    Request->SetVerb(TEXT("POST"));
    Request->SetURL(BaseUrl + RelativePath);
    Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + InternalToken);
    Request->SetHeader(TEXT("X-Game-Server-Id"), Instance.GameServerId);
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
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
        [CompleteOnce](
            FHttpRequestPtr,
            FHttpResponsePtr Response,
            bool bSucceeded) mutable
        {
            if (!bSucceeded || !Response.IsValid())
            {
                CompleteOnce(false, FName(TEXT("ControlPlaneTransportFailed")));
                return;
            }
            if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
            {
                // 不读取或记录响应体，避免服务端错误回显凭据或玩家数据。
                CompleteOnce(false, FName(TEXT("ControlPlaneRejected")));
                return;
            }

            TSharedPtr<FJsonObject> ResponseBody;
            const TSharedRef<TJsonReader<>> Reader =
                TJsonReaderFactory<>::Create(Response->GetContentAsString());
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
