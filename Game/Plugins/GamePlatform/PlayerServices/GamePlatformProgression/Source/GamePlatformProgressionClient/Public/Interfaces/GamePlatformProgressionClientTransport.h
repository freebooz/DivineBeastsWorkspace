#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformProgressionTypes.h"

using FGamePlatformProgressionSnapshotCompletion =
    TFunction<void(
        FGamePlatformProgressionSnapshot,
        EGamePlatformProgressionError)>;

class GAMEPLATFORMPROGRESSIONCLIENT_API IGamePlatformProgressionClientTransport
{
public:
    virtual ~IGamePlatformProgressionClientTransport() = default;

    virtual void CancelAllRequests() = 0;

    virtual bool BeginGetSnapshot(
        FGamePlatformProgressionSnapshotCompletion Completion) = 0;
};
