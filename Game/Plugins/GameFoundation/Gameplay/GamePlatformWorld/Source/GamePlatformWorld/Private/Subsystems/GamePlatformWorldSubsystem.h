#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Types/GamePlatformDataLease.h"
#include "Containers/Ticker.h"
#include "GamePlatformWorldSubsystem.generated.h"
class FGamePlatformWorldStreaming;
struct FWorldRegionRecord { FGamePlatformWorldRegistration Handle; FGamePlatformRegionProvider Value; };
struct FWorldObserverRecord { TWeakObjectPtr<UObject> Owner; FVector Position; FGamePlatformId Region; };
struct FWorldRegionSubscription { TWeakObjectPtr<UObject> Owner; TFunction<void(const FGamePlatformRegionEvent&)> Callback; };
struct FWorldContributorRecord { TWeakObjectPtr<UObject> Owner; TSharedPtr<IGamePlatformWorldReadinessContributor> Contributor; };

/** 私有世界服务：仅Game/PIE非Commandlet创建，实例间无共享可变登记表。 */
UCLASS()
class UGamePlatformWorldSubsystem final : public UWorldSubsystem, public IGamePlatformWorldService
{
    GENERATED_BODY()
public:
    UGamePlatformWorldSubsystem();
    UGamePlatformWorldSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformWorldSubsystem() override;
    virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldEndPlay(UWorld& World) override;
    virtual FGamePlatformResult InitializeDevelopment(const FPrimaryAssetId&, const FGamePlatformVersion&, FName) override;
    virtual FGamePlatformResult InitializeSessionWorld() override;
    virtual FGamePlatformWorldReadinessSnapshot GetReadiness() override;
    virtual FGamePlatformWorldRegistration RegisterRegionProvider(const FGamePlatformRegionProvider&,FGamePlatformResult&) override;
    virtual bool UnregisterRegionProvider(const FGamePlatformWorldRegistration&) override;
    virtual FGamePlatformResult QueryRegion(const FVector&,FGamePlatformId&) override;
    virtual FGamePlatformResult UpdateObserver(TWeakObjectPtr<UObject>,const FVector&,const FGuid&) override;
    virtual FGamePlatformWorldRegistration SubscribeRegions(TWeakObjectPtr<UObject>,TFunction<void(const FGamePlatformRegionEvent&)>) override;
    virtual bool Unregister(const FGamePlatformWorldRegistration&) override;
    virtual FGamePlatformWorldRegistration RegisterReadinessContributor(TWeakObjectPtr<UObject>,TSharedRef<IGamePlatformWorldReadinessContributor>,FGamePlatformResult&) override;
    virtual FGamePlatformWorldStreamingHandle RequestStreaming(const FGamePlatformWorldStreamingRequest&,FGamePlatformResult&) override;
    virtual FGamePlatformWorldStreamingResult GetStreamingState(const FGamePlatformWorldStreamingHandle&) override;
    virtual FGamePlatformResult CancelStreaming(const FGamePlatformWorldStreamingHandle&) override;
private:
    bool Tick(float DeltaSeconds);
    bool Owns(TWeakObjectPtr<UObject> Object) const;
    bool CanMutate() const;
    void Stop();
    void ReleaseOwnedResources();
    void Fail(FName Code, const FString& Message);
    void Refresh();
    void LoadRegions(const TArray<FPrimaryAssetId>& Ids);
    void UpdateRegions();
    FGamePlatformWorldRegistration NewRegistration() const;
    FGamePlatformWorldReadinessSnapshot Snapshot;
    FGamePlatformDataLease DefinitionLease;
    TMap<FPrimaryAssetId,FGamePlatformDataLease> RegionLeases;
    TMap<FPrimaryAssetId,FGamePlatformId> RegionIdentities;
    TArray<FWorldRegionRecord> Regions;
    TArray<FWorldObserverRecord> Observers;
    TMap<FGuid,TSharedPtr<FWorldRegionSubscription>> Subscriptions;
    TMap<FGuid,FWorldContributorRecord> Contributors;
    TArray<FGamePlatformRegionEvent> PendingEvents;
    TUniquePtr<FGamePlatformWorldStreaming> Streaming;
    FTSTicker::FDelegateHandle TickerHandle;
    double DeadlineSeconds = 0;
    bool bStarted = false;
    bool bClosing = false;
    bool bDispatching = false;
    bool bFailureCleanupPending = false;
};
