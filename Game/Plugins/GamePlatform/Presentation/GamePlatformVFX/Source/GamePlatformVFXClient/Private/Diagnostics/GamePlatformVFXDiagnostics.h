#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Stats/Stats.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGamePlatformVFX, Log, All);

DECLARE_STATS_GROUP(TEXT("GamePlatformVFX"), STATGROUP_GamePlatformVFX, STATCAT_Advanced);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Play Requests"), STAT_GPVFX_PlayRequests, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Rejected Requests"), STAT_GPVFX_RejectedRequests, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Dedupe Hits"), STAT_GPVFX_DedupeHits, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Definition Cache Hits"), STAT_GPVFX_DefinitionCacheHits, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Definition Cache Misses"), STAT_GPVFX_DefinitionCacheMisses, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Composite Children"), STAT_GPVFX_CompositeChildren, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Tracked Instances"), STAT_GPVFX_TrackedInstances, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Pending Instances"), STAT_GPVFX_PendingInstances, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Peak Tracked Instances"), STAT_GPVFX_PeakTrackedInstances, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);

class FGamePlatformVFXDiagnostics
{
public:
    static void CatalogMiss(const FGameplayTag& SemanticTag, FName ContextId);
    static void CatalogAmbiguous(const FGameplayTag& SemanticTag, FName ContextId);
    static void DefinitionLoadFailed(const FSoftObjectPath& Path);

    static void PlayRequested();
    static void RequestRejected();
    static void DedupeHit();
    static void DefinitionCacheHit();
    static void DefinitionCacheMiss();
    static void CompositeChild();
    static void UpdateRuntimeCounts(int32 TrackedInstances, int32 PendingInstances, int32 PeakTrackedInstances);
};
