#pragma once
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Containers/Ticker.h"
#include "GamePlatformDataSubsystem.generated.h"

struct FGamePlatformDataScope;
/** 实例私有门面：拥有请求图与弱调用者，不拥有其他实例或外部加载。 */
UCLASS()
class UGamePlatformDataSubsystem : public UGameInstanceSubsystem, public IGamePlatformDataService
{
    GENERATED_BODY()
public:
    UGamePlatformDataSubsystem();
    UGamePlatformDataSubsystem(FVTableHelper& Helper);
    virtual ~UGamePlatformDataSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual FGamePlatformDataLease AcquireDefinition(const FPrimaryAssetId& DefinitionId,
        TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass, const TArray<FName>& Bundles,
        EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
        FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult) override;
    virtual const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease& Lease) const override;
    virtual FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease& Lease) override;
    virtual EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const override;
    virtual FGamePlatformDataDiagnostics GetDiagnostics() const override;
private:
    TUniquePtr<FGamePlatformDataScope> Scope;
    FDelegateHandle WorldCleanupHandle;
    FTSTicker::FDelegateHandle OwnerWatchHandle;
    void Advance(FGamePlatformDataLease Lease);
    void AssetReady(FGamePlatformDataLease Lease, FPrimaryAssetId AssetId, FGamePlatformResult Result);
    void Finish(FGamePlatformDataLease Lease, FGamePlatformResult Result);
    void ReleaseRequest(FGamePlatformDataLease Lease, const FString& Reason);
    void CleanupWorld(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    bool WatchOwners(float DeltaSeconds);
};
