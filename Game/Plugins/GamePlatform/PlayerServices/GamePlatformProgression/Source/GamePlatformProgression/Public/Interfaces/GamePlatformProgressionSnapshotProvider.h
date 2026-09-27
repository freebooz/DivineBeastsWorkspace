#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformProgressionTypes.h"

using FGamePlatformProgressionRequirementCompletion =
    TFunction<void(
        FGamePlatformProgressionRequirementResult,
        EGamePlatformProgressionError)>;

/**
 * Dedicated Server（专用服务器）使用的中立成长资格提供器。
 * 实现可来自PlayerData查询/缓存；共享模块不依赖HTTP。
 */
class GAMEPLATFORMPROGRESSION_API IGamePlatformProgressionSnapshotProvider
{
public:
    virtual ~IGamePlatformProgressionSnapshotProvider() = default;

    virtual bool BeginCheckRequirement(
        const FString& PlayerId,
        const FGamePlatformProgressionRequirement& Requirement,
        FGamePlatformProgressionRequirementCompletion Completion) = 0;
};
