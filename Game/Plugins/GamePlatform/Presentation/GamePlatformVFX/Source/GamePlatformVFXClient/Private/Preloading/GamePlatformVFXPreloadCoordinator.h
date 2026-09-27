#pragma once

#include "Definitions/GamePlatformVFXDefinition.h"
#include "Types/GamePlatformVFXPreloadHandle.h"

struct FStreamableHandle;

/**
 * 基于 GamePlatformData（平台数据）的异步 Definition 预加载协调器。
 * Requests 中保留 FStreamableHandle（流式句柄）作为资源 Lease（租约），
 * 直到 Cancel/Reset，避免加载完成后资源立即失去持有关系。
 */
class FGamePlatformVFXPreloadCoordinator
{
public:
    FGamePlatformVFXPreloadHandle RequestDefinition(
        const TSoftObjectPtr<UGamePlatformVFXDefinition>& Definition,
        TFunction<void(UGamePlatformVFXDefinition*)> Completion,
        FName PlatformId = NAME_None,
        EGamePlatformVFXQualityTier QualityTier = EGamePlatformVFXQualityTier::High);

    bool Cancel(const FGamePlatformVFXPreloadHandle& Handle);
    void Reset();

private:
    TMap<FGuid, TSharedPtr<FStreamableHandle>> Requests;
};
