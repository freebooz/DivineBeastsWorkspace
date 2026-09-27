#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/ThreadSafeCounter64.h"
#include "State/GamePlatformDebugStateProvider.h"

/**
 * GamePlatformDebug（游戏平台调试插件）中立注册表。
 * 只拥有调试元数据和弱目标引用，不拥有 Gameplay Actor。
 */
class GAMEPLATFORMDEBUG_API FGamePlatformDebugRegistry
{
public:
    static FGamePlatformDebugRegistry& Get();

    bool RegisterStateProvider(const TSharedRef<IGamePlatformDebugStateProvider>& Provider);
    void UnregisterStateProvider(FName ProviderId);
    TArray<FName> GetProviderIds() const;

    bool RegisterCommandDescriptor(const FGamePlatformDebugCommandDescriptor& Descriptor);
    void UnregisterCommandDescriptor(FName CommandName);
    TArray<FGamePlatformDebugCommandDescriptor> GetCommandDescriptors() const;

    bool SetProviderEnabled(FName ProviderId, bool bEnabled);
    bool IsProviderEnabled(FName ProviderId) const;

    bool CollectSnapshot(
        FName ProviderId,
        const FGamePlatformDebugCollectContext& Context,
        FGamePlatformDebugSnapshot& OutSnapshot);

    FGamePlatformDebugTarget ResolveDebugTarget(AActor* Actor);
    void PruneExpiredTargets();

    /** 过滤敏感字段并应用字段数、字符串长度和Payload上限。 */
    void SanitizeAndBoundSnapshot(FGamePlatformDebugSnapshot& Snapshot) const;

    /** 返回仅包含已经过过滤字段的可复制文本。 */
    FString BuildSanitizedSummary(const FGamePlatformDebugSnapshot& Snapshot) const;

    /** 强制下一次快照具有新的 Revision（修订号），不修改 Gameplay。 */
    uint64 ForceRefreshRevision();

private:
    FGamePlatformDebugRegistry() = default;

    static bool IsSensitiveField(const FGamePlatformDebugField& Field);

    mutable FCriticalSection Mutex;
    TMap<FName, TSharedPtr<IGamePlatformDebugStateProvider>> Providers;
    TMap<FName, FGamePlatformDebugCommandDescriptor> Commands;
    TMap<FName, bool> ProviderEnabled;
    TMap<TWeakObjectPtr<AActor>, FGamePlatformDebugTarget> Targets;
    FThreadSafeCounter64 RevisionCounter;
};
