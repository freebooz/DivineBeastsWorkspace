#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

struct FStreamableHandle;
class UGamePlatformHeroDefinition;

/**
 * FGamePlatformHeroDefinitionLoader（平台英雄定义加载器）。
 * 内部复用GamePlatformData统一异步加载，不让具体游戏创建第二AssetManager。
 */
class GAMEPLATFORMCHARACTER_API FGamePlatformHeroDefinitionLoader
{
public:
    static TSharedPtr<FStreamableHandle> RequestDefinition(
        const FPrimaryAssetId& PrimaryAssetId,
        TFunction<void(UGamePlatformHeroDefinition*)> Completion);
};
