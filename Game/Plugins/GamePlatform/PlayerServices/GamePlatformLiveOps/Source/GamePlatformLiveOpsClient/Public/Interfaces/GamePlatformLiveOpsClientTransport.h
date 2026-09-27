#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformLiveOpsTypes.h"

using FGamePlatformLiveOpsCatalogCompletion =
    TFunction<void(
        FGamePlatformLiveOpsCatalogSnapshot,
        EGamePlatformLiveOpsError)>;

using FGamePlatformLiveOpsPlayerStateCompletion =
    TFunction<void(
        FGamePlatformLiveOpsPlayerState,
        EGamePlatformLiveOpsError)>;

using FGamePlatformLiveOpsClaimCompletion =
    TFunction<void(
        FGamePlatformLiveOpsClaimResult,
        EGamePlatformLiveOpsError)>;

class GAMEPLATFORMLIVEOPSCLIENT_API IGamePlatformLiveOpsClientTransport
{
public:
    virtual ~IGamePlatformLiveOpsClientTransport() = default;

    virtual void CancelAllRequests() = 0;

    virtual bool BeginGetCatalog(
        FGamePlatformLiveOpsCatalogCompletion Completion) = 0;

    virtual bool BeginGetPlayerState(
        FGamePlatformLiveOpsPlayerStateCompletion Completion) = 0;

    virtual bool BeginClaimSignIn(
        const FGuid& ClaimOperationId,
        FName CampaignId,
        FGamePlatformLiveOpsClaimCompletion Completion) = 0;

    virtual bool BeginQueryClaimOperation(
        const FGuid& ClaimOperationId,
        FGamePlatformLiveOpsClaimCompletion Completion) = 0;
};
