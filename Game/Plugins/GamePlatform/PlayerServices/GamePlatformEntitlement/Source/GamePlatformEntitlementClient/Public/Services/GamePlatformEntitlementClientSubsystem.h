#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformEntitlementTypes.h"
#include "Types/GamePlatformEntitlementClientTypes.h"
#include "GamePlatformEntitlementClientSubsystem.generated.h"

class IGamePlatformEntitlementClientTransport;

DECLARE_MULTICAST_DELEGATE(FGamePlatformEntitlementClientChanged);

UCLASS()
class GAMEPLATFORMENTITLEMENTCLIENT_API UGamePlatformEntitlementClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformEntitlementClientTransport, ESPMode::ThreadSafe> InTransport);

    void ResetAccount();
    bool RefreshSnapshot();

    UFUNCTION(BlueprintPure, Category="Entitlement")
    EGamePlatformEntitlementClientState GetState() const { return State; }

    UFUNCTION(BlueprintPure, Category="Entitlement")
    int64 GetSnapshotRevision() const { return Snapshot.Revision; }

    UFUNCTION(BlueprintPure, Category="Entitlement")
    bool HasEntitlement(FName EntitlementId) const;

    bool HasAny(const TArray<FName>& EntitlementIds) const;
    bool HasAll(const TArray<FName>& EntitlementIds) const;

    UFUNCTION(BlueprintPure, Category="Entitlement")
    TArray<FGamePlatformEntitlementViewModel> GetViewModels() const;

    /** 原生高频UI读取使用，避免重复构建ViewModel数组。 */
    const TArray<FGamePlatformEntitlementViewModel>& GetViewModelsView() const
    {
        return CachedViewModels;
    }

    UFUNCTION(BlueprintPure, Category="Entitlement")
    bool IsHeroUnlocked(FName HeroDefinitionId) const;

    UFUNCTION(BlueprintPure, Category="Entitlement")
    bool IsSkinUnlocked(FName SkinDefinitionId) const;

    const FGamePlatformEntitlementSnapshot& GetSnapshot() const
    {
        return Snapshot;
    }

    FGamePlatformEntitlementClientChanged OnChanged;

private:
    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;
    EGamePlatformEntitlementClientState State =
        EGamePlatformEntitlementClientState::Uninitialized;

    FGamePlatformEntitlementSnapshot Snapshot;

    // 由Snapshot一次构建的只读派生索引；不保存额外业务状态。
    TSet<FName> EffectiveEntitlementIds;
    TSet<FName> EffectiveHeroIds;
    TSet<FName> EffectiveSkinIds;
    TArray<FGamePlatformEntitlementViewModel> CachedViewModels;
    EGamePlatformEntitlementError LastError =
        EGamePlatformEntitlementError::None;

    TSharedPtr<IGamePlatformEntitlementClientTransport, ESPMode::ThreadSafe>
        Transport;

    void RebuildDerivedCaches();

    void HandleSnapshotCompleted(
        uint64 ExpectedGeneration,
        FGamePlatformEntitlementSnapshot NewSnapshot,
        EGamePlatformEntitlementError Error);

    bool ApplySnapshot(
        const FGamePlatformEntitlementSnapshot& NewSnapshot);
};
