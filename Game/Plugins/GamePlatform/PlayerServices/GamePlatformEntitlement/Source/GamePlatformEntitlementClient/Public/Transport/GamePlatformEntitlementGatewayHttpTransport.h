#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformEntitlementClientTransport.h"

class GAMEPLATFORMENTITLEMENTCLIENT_API FGamePlatformEntitlementGatewayHttpTransport final
    : public IGamePlatformEntitlementClientTransport
    , public TSharedFromThis<
        FGamePlatformEntitlementGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformEntitlementGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    bool IsConfigured() const;

    virtual void CancelAllRequests() override;

    virtual bool BeginGetSnapshot(
        FGamePlatformEntitlementSnapshotCompletion Completion) override;

private:
    FString GatewayBaseUrl;
    FString AccessToken;

    FCriticalSection ActiveRequestsMutex;
    TArray<TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>> ActiveRequests;

    bool StartJsonRequest(
        const FString& Path,
        TFunction<void(int32, const FString&)> Completion);

    void UnregisterRequest(
        const TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>& Request);

    static bool JsonToSnapshot(
        const TSharedPtr<class FJsonObject>& Json,
        FGamePlatformEntitlementSnapshot& OutSnapshot);

    static EGamePlatformEntitlementError MapHttpError(
        int32 StatusCode);
};
