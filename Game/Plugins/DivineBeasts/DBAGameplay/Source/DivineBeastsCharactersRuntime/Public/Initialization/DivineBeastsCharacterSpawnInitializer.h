#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformHeroDefinition.h"
#include "Initialization/GamePlatformCharacterInitializer.h"

/**
 * FDivineBeastsCharacterSpawnInitializer（神兽联盟角色出生初始化适配器）。
 * 只初始化已有ACharacter；绝不自行SpawnActor或Possess。
 */
class DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsCharacterSpawnInitializer
    : public IGamePlatformCharacterInitializer
{
public:
    virtual bool InitializeCharacter(
        ACharacter& Character,
        const FGamePlatformCharacterInitializationContext& Context,
        FString& OutError) override;

    /** Spawn选点前可查询已加载Definition的可信胶囊包络。 */
    static bool TryGetLoadedSpawnEnvelope(
        FName HeroDefinitionId,
        FGamePlatformCharacterSpawnEnvelope& OutEnvelope);
};
