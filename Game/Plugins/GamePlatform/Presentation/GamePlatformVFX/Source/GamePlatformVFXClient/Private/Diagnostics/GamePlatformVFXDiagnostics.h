#pragma once

#include "CoreMinimal.h"
#include "Diagnostics/GamePlatformVFXRuntimeCountRegistry.h"
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
/** NDC共享爆发事件实际写入量和失败后回退独立Niagara次数，不代表GPU粒子数。 */
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Shared Burst Writes"), STAT_GPVFX_SharedBurstWrites, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
DECLARE_DWORD_ACCUMULATOR_STAT_EXTERN(TEXT("Shared Burst Fallbacks"), STAT_GPVFX_SharedBurstFallbacks, STATGROUP_GamePlatformVFX, GAMEPLATFORMVFXCLIENT_API);
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
    /** 共享数据通道已经受理的一个瞬时视觉事件。 */
    static void SharedBurstWritten();
    /** 共享通道不可用后使用旧Niagara System进行视觉回退。 */
    static void SharedBurstFallback();
    /** 游戏线程显式注册/注销；统计进程内所有存活世界，不持有UWorld引用。 */
    static FGuid RegisterWorld();
    static void UpdateRuntimeCounts(const FGuid& WorldId, int32 TrackedInstances, int32 PendingInstances);
    static void UnregisterWorld(const FGuid& WorldId);
    static FGamePlatformVFXRuntimeCountSnapshot GetRuntimeCounts();
    /** 模块生命周期边界重置，普通世界退出保留历史同时在场峰值。 */
    static void ResetRuntimeCounts();
};
