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
    virtual FGamePlatformDataLease AcquireResources(const TArray<FSoftObjectPath>& ResourcePaths,
        EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
        FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult) override;
    virtual FGamePlatformResult ReleaseResources(const FGamePlatformDataLease& Lease) override;
    virtual EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const override;
    virtual FGamePlatformDataDiagnostics GetDiagnostics() const override;
private:
    TUniquePtr<FGamePlatformDataScope> Scope;
    FDelegateHandle WorldCleanupHandle;
    FTSTicker::FDelegateHandle OwnerWatchHandle;
    void Advance(FGamePlatformDataLease Lease);
    void AssetReady(FGamePlatformDataLease Lease, FPrimaryAssetId AssetId, FGamePlatformResult Result);
    /** 普通资源加载回调再次核对作用域/调用方；禁止迟到完成使释放后的请求复活。 */
    void ResourcesReady(FGamePlatformDataLease Lease, FGamePlatformResult Result);
    /** 校验签发证明和当前作用域；不把任意未知ID假定为已释放。 */
    bool HasAuthenticLease(const FGamePlatformDataLease& Lease) const;
    void Finish(FGamePlatformDataLease Lease, FGamePlatformResult Result);
    void ReleaseRequest(FGamePlatformDataLease Lease, const FString& Reason);
    void CleanupWorld(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    bool WatchOwners(float DeltaSeconds);
};
