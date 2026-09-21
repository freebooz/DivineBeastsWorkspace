#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXCatalogTypes.generated.h"

/** 一条确定性的语义到 VFX Definition 映射。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    FGameplayTag SemanticTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    FGameplayTagContainer RequiredContextTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    FGameplayTagContainer BlockedContextTags;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    FPrimaryAssetId DefinitionId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXCatalogScope Scope = EGamePlatformVFXCatalogScope::Platform;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VFX")
    bool bAllowParentSemanticFallback = true;
};
