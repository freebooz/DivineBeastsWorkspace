#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformDebugTypes.h"

/** 平台调试状态提供接口。实现必须只读、无副作用，并遵守有界采集要求。 */
class GAMEPLATFORMDEBUG_API IGamePlatformDebugStateProvider
{
public:
    virtual ~IGamePlatformDebugStateProvider() = default;

    virtual FName GetProviderId() const = 0;
    virtual bool CanCollect(const FGamePlatformDebugCollectContext& Context) const = 0;
    virtual bool CollectSnapshot(
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& OutSnapshot) const = 0;
    virtual EGamePlatformDebugProviderCost GetEstimatedCost() const = 0;
};
