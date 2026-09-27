#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Types/GamePlatformVFXTypes.h"
#include "GamePlatformVFXCatalogTypes.generated.h"

class UGamePlatformVFXDefinition;

/** Catalog 的一条确定性语义映射。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformVFXCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGameplayTag SemanticTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName ContextId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGameplayTagContainer RequiredContextTags;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FGameplayTagContainer BlockedContextTags;

    /** 上游Presentation或专门工具可直接使用的逻辑Definition ID。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName DefinitionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    TSoftObjectPtr<UGamePlatformVFXDefinition> Definition;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    int32 Priority = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    int32 Specificity = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    EGamePlatformVFXCatalogScope Scope = EGamePlatformVFXCatalogScope::Platform;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    FName PlatformId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    bool bAnyQuality = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(EditCondition="!bAnyQuality"))
    EGamePlatformVFXQualityTier QualityTier = EGamePlatformVFXQualityTier::High;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
    bool bFallback = false;
};
