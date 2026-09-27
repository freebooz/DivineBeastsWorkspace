#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEntitlementTypes.h"

using FGamePlatformEntitlementSnapshotCompletion =
    TFunction<void(
        FGamePlatformEntitlementSnapshot,
        EGamePlatformEntitlementError)>;

class GAMEPLATFORMENTITLEMENTCLIENT_API IGamePlatformEntitlementClientTransport
{
public:
    virtual ~IGamePlatformEntitlementClientTransport() = default;

    virtual void CancelAllRequests() = 0;

    virtual bool BeginGetSnapshot(
        FGamePlatformEntitlementSnapshotCompletion Completion) = 0;
};
