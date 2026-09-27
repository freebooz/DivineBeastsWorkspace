#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformProgressionClientTransport.h"

class GAMEPLATFORMPROGRESSIONCLIENT_API FGamePlatformProgressionGatewayHttpTransport final
    : public IGamePlatformProgressionClientTransport
    , public TSharedFromThis<
        FGamePlatformProgressionGatewayHttpTransport,
        ESPMode::ThreadSafe>
{
public:
    FGamePlatformProgressionGatewayHttpTransport(
        FString InGatewayBaseUrl,
        FString InAccessToken);

    bool IsConfigured() const;

    virtual void CancelAllRequests() override;

    virtual bool BeginGetSnapshot(
        FGamePlatformProgressionSnapshotCompletion Completion) override;

private:
    FString GatewayBaseUrl;
    FString AccessToken;

    FCriticalSection ActiveRequestsMutex;
    TArray<TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>>
        ActiveRequests;

    bool StartRequest(
        const FString& Path,
        TFunction<void(int32, const FString&)> Completion);

    void UnregisterRequest(
        const TSharedPtr<class IHttpRequest, ESPMode::ThreadSafe>& Request);

    static bool JsonToSnapshot(
        const TSharedPtr<class FJsonObject>& Json,
        FGamePlatformProgressionSnapshot& OutSnapshot);

    static bool ParseInt64String(
        const TSharedPtr<class FJsonObject>& Json,
        const TCHAR* Field,
        int64& OutValue);

    static EGamePlatformProgressionError MapHttpError(
        int32 StatusCode);
};
