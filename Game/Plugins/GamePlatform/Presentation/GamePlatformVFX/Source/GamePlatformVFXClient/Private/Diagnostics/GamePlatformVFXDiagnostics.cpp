#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "GameplayTagContainer.h"
#include "Settings/GamePlatformVFXSettings.h"

DEFINE_LOG_CATEGORY(LogGamePlatformVFX);
DEFINE_STAT(STAT_GPVFX_PlayRequests);
DEFINE_STAT(STAT_GPVFX_RejectedRequests);
DEFINE_STAT(STAT_GPVFX_DedupeHits);
DEFINE_STAT(STAT_GPVFX_DefinitionCacheHits);
DEFINE_STAT(STAT_GPVFX_DefinitionCacheMisses);
DEFINE_STAT(STAT_GPVFX_CompositeChildren);
DEFINE_STAT(STAT_GPVFX_TrackedInstances);
DEFINE_STAT(STAT_GPVFX_PendingInstances);
DEFINE_STAT(STAT_GPVFX_PeakTrackedInstances);

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

void FGamePlatformVFXDiagnostics::PlayRequested()
{
    INC_DWORD_STAT(STAT_GPVFX_PlayRequests);
}

void FGamePlatformVFXDiagnostics::RequestRejected()
{
    INC_DWORD_STAT(STAT_GPVFX_RejectedRequests);
}

void FGamePlatformVFXDiagnostics::DedupeHit()
{
    INC_DWORD_STAT(STAT_GPVFX_DedupeHits);
}

void FGamePlatformVFXDiagnostics::DefinitionCacheHit()
{
    INC_DWORD_STAT(STAT_GPVFX_DefinitionCacheHits);
}

void FGamePlatformVFXDiagnostics::DefinitionCacheMiss()
{
    INC_DWORD_STAT(STAT_GPVFX_DefinitionCacheMisses);
}

void FGamePlatformVFXDiagnostics::CompositeChild()
{
    INC_DWORD_STAT(STAT_GPVFX_CompositeChildren);
}

void FGamePlatformVFXDiagnostics::UpdateRuntimeCounts(
    const int32 TrackedInstances,
    const int32 PendingInstances,
    const int32 PeakTrackedInstances)
{
    SET_DWORD_STAT(STAT_GPVFX_TrackedInstances, FMath::Max(0, TrackedInstances));
    SET_DWORD_STAT(STAT_GPVFX_PendingInstances, FMath::Max(0, PendingInstances));
    SET_DWORD_STAT(STAT_GPVFX_PeakTrackedInstances, FMath::Max(0, PeakTrackedInstances));
}
