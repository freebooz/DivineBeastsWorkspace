#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXSpawnContext.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXRequest.generated.h"

/**
 * VFX 播放请求。
 * 高层优先提供 SemanticTag + ContextTags；只有已明确知道内容时才使用 ExplicitDefinitionId。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGameplayTag SemanticTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGameplayTagContainer ContextTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FPrimaryAssetId ExplicitDefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGamePlatformVFXSpawnContext SpawnContext;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    FGamePlatformVFXParameters Parameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
    EGamePlatformVFXImportance Importance = EGamePlatformVFXImportance::Normal;

    bool IsStructurallyValid() const
    {
        return ExplicitDefinitionId.IsValid() || SemanticTag.IsValid();
    }
};
