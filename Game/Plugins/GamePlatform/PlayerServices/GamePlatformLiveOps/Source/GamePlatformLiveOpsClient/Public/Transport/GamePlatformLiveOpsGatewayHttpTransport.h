#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformLiveOpsClientTransport.h"

class FJsonObject;
class IHttpRequest;

class GAMEPLATFORMLIVEOPSCLIENT_API FGamePlatformLiveOpsGatewayHttpTransport final
    : public IGamePlatformLiveOpsClientTransport
    , public TSharedFromThis<
        FGamePlatformLiveOpsGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformLiveOpsGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    bool IsConfigured() const;

    virtual void CancelAllRequests() override;

    virtual bool BeginGetCatalog(
        FGamePlatformLiveOpsCatalogCompletion Completion) override;

    virtual bool BeginGetPlayerState(
        FGamePlatformLiveOpsPlayerStateCompletion Completion) override;

    virtual bool BeginClaimSignIn(
        const FGuid& ClaimOperationId,
        FName CampaignId,
        FGamePlatformLiveOpsClaimCompletion Completion) override;

    virtual bool BeginQueryClaimOperation(
        const FGuid& ClaimOperationId,
        FGamePlatformLiveOpsClaimCompletion Completion) override;

private:
    FString GatewayBaseUrl;
    FString AccessToken;

    FCriticalSection ActiveRequestsMutex;
    TArray<TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>>
        ActiveRequests;

    bool StartJsonRequest(
        const FString& Verb,
        const FString& Path,
        const TSharedPtr<FJsonObject>& Body,
        TFunction<void(int32, const FString&)> Completion);

    void UnregisterRequest(
        const TSharedPtr<IHttpRequest, ESPMode::ThreadSafe>& Request);

    static bool JsonToCatalog(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsCatalogSnapshot& OutCatalog);

    static bool JsonToPlayerState(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsPlayerState& OutState);

    static bool JsonToClaim(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsClaimResult& OutClaim);

    static bool ParseTimeWindow(
        const TSharedPtr<FJsonObject>& Json,
        FGamePlatformLiveOpsTimeWindow& OutWindow);

    static bool ParseIsoTime(
        const TSharedPtr<FJsonObject>& Json,
        const TCHAR* Field,
        FDateTime& OutTime,
        bool bRequired);

    static EGamePlatformLiveOpsError MapHttpError(
        int32 StatusCode,
        const FString& Body);
};
