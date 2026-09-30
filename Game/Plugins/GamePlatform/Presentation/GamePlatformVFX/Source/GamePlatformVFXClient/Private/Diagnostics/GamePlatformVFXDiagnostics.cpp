#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "GameplayTagContainer.h"
#include "Settings/GamePlatformVFXSettings.h"

DEFINE_LOG_CATEGORY(LogGamePlatformVFX);

void FGamePlatformVFXDiagnostics::CatalogMiss(const FGameplayTag& SemanticTag, FName ContextId)
{
    if (!GetDefault<UGamePlatformVFXSettings>()->bEnableDiagnostics) return;
    UE_LOG(LogGamePlatformVFX, Verbose, TEXT("VFX Catalog miss: Semantic=%s Context=%s"),
        *SemanticTag.ToString(), *ContextId.ToString());
}

void FGamePlatformVFXDiagnostics::CatalogAmbiguous(const FGameplayTag& SemanticTag, FName ContextId)
{
    if (!GetDefault<UGamePlatformVFXSettings>()->bEnableDiagnostics) return;
    UE_LOG(LogGamePlatformVFX, Error, TEXT("VFX Catalog ambiguity rejected: Semantic=%s Context=%s"),
        *SemanticTag.ToString(), *ContextId.ToString());
}

void FGamePlatformVFXDiagnostics::DefinitionLoadFailed(const FSoftObjectPath& Path)
{
    if (!GetDefault<UGamePlatformVFXSettings>()->bEnableDiagnostics) return;
    UE_LOG(LogGamePlatformVFX, Warning, TEXT("VFX Definition load failed: %s"), *Path.ToString());
}
