#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGamePlatformVFX, Log, All);

class FGamePlatformVFXDiagnostics
{
public:
    static void CatalogMiss(const FGameplayTag& SemanticTag, FName ContextId);
    static void CatalogAmbiguous(const FGameplayTag& SemanticTag, FName ContextId);
    static void DefinitionLoadFailed(const FSoftObjectPath& Path);
};
