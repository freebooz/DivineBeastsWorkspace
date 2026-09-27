#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformProgressionClientTypes.h"
#include "GamePlatformProgressionClientSubsystem.generated.h"

class IGamePlatformProgressionClientTransport;
class UGamePlatformProgressionTrackDefinition;

DECLARE_MULTICAST_DELEGATE_FourParams(
    FGamePlatformProgressionLevelChanged,
    FName,
    FString,
    int32,
    int32);

DECLARE_MULTICAST_DELEGATE_FourParams(
    FGamePlatformProgressionXPChanged,
    FName,
    FString,
    int64,
    int64);

UCLASS()
class GAMEPLATFORMPROGRESSIONCLIENT_API UGamePlatformProgressionClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformProgressionClientTransport, ESPMode::ThreadSafe>
            InTransport);

    void ResetAccount();
    bool RefreshSnapshot();

    void RegisterTrackDefinition(
        UGamePlatformProgressionTrackDefinition* Definition);

    UFUNCTION(BlueprintPure, Category="Progression")
    EGamePlatformProgressionClientState GetState() const
    {
        return State;
    }

    UFUNCTION(BlueprintPure, Category="Progression")
    int64 GetProgressionRevision() const
    {
        return Snapshot.ProgressionRevision;
    }

    UFUNCTION(BlueprintPure, Category="Progression")
    int32 GetLevel(
        FName TrackId,
        const FString& SubjectId) const;

    UFUNCTION(BlueprintPure, Category="Progression")
    int64 GetTotalXP(
        FName TrackId,
        const FString& SubjectId) const;

    UFUNCTION(BlueprintPure, Category="Progression")
    TArray<FGamePlatformProgressionViewModel> GetViewModels() const;

    /** C++高频UI读取：按Snapshot/Definition代次复用派生ViewModel，避免重复分配和曲线计算。 */
    const TArray<FGamePlatformProgressionViewModel>& GetViewModelsView() const;

    const FGamePlatformProgressionSnapshot& GetSnapshot() const
    {
        return Snapshot;
    }

    FGamePlatformProgressionLevelChanged OnLevelChanged;
    FGamePlatformProgressionXPChanged OnXPChanged;

private:
    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;

    EGamePlatformProgressionClientState State =
        EGamePlatformProgressionClientState::Uninitialized;

    EGamePlatformProgressionError LastError =
        EGamePlatformProgressionError::None;

    FGamePlatformProgressionSnapshot Snapshot;
    uint64 SnapshotGeneration = 0;
    uint64 DefinitionGeneration = 0;

    // Derived caches（派生缓存）只由Snapshot/Definition代次驱动，不成为第二份业务真源。
    mutable uint64 CachedIndexSnapshotGeneration = ~uint64(0);
    mutable TMap<FName, TMap<FString, int32>> CachedTrackIndexById;
    mutable uint64 CachedViewSnapshotGeneration = ~uint64(0);
    mutable uint64 CachedViewDefinitionGeneration = ~uint64(0);
    mutable TArray<FGamePlatformProgressionViewModel> CachedViewModels;

    TSharedPtr<IGamePlatformProgressionClientTransport, ESPMode::ThreadSafe>
        Transport;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformProgressionTrackDefinition>>
        Definitions;

    void EnsureTrackIndexCache() const;
    void EnsureViewModelCache() const;

    void HandleSnapshotCompleted(
        uint64 ExpectedGeneration,
        FGamePlatformProgressionSnapshot NewSnapshot,
        EGamePlatformProgressionError Error);

    bool ApplySnapshot(
        const FGamePlatformProgressionSnapshot& NewSnapshot);

    const FGamePlatformProgressionTrackState* FindTrack(
        FName TrackId,
        const FString& SubjectId) const;
};
