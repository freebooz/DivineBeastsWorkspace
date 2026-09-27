#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "GamePlatformPresentationCatalog.h"
#include "GamePlatformPresentationContext.h"
#include "GamePlatformPresentationClientSubsystem.generated.h"

class UWorld;

/** Provider（表现提供者）处理器；返回true表示已经处理该请求。 */
DECLARE_DELEGATE_RetVal_OneParam(
    bool,
    FGamePlatformPresentationProviderHandler,
    const FGamePlatformPresentationRequest&);

/** 请求观察委托，用于诊断/测试；不会驱动Gameplay。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FGamePlatformPresentationRequestObserved,
    const FGamePlatformPresentationRequest&,
    EGamePlatformPresentationSubmitResult);

/**
 * UGamePlatformPresentationClientSubsystem（平台表现客户端子系统）。
 * 每个LocalPlayer（本地玩家）独立持有Provider注册表，按Priority→ProviderId确定性分发。
 */
UCLASS()
class GAMEPLATFORMPRESENTATIONCLIENT_API UGamePlatformPresentationClientSubsystem
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    FGuid RegisterProvider(
        FName ProviderId,
        int32 Priority,
        FGamePlatformPresentationProviderHandler Handler);

    bool UnregisterProvider(const FGuid& RegistrationId);

    FGamePlatformPresentationRegistrationHandle RegisterContextContributor(
        TSharedRef<IGamePlatformPresentationContextContributor> Contributor);

    bool UnregisterContextContributor(
        const FGamePlatformPresentationRegistrationHandle& Handle);

    FGamePlatformPresentationRegistrationHandle RegisterCatalogFragment(
        const FGamePlatformPresentationCatalogFragment& Fragment);

    bool UnregisterCatalogFragment(
        const FGamePlatformPresentationRegistrationHandle& Handle);

    bool BuildContext(FGamePlatformPresentationContext& InOutContext) const;

    EGamePlatformPresentationCatalogResolveResult ResolveCatalog(
        const FGameplayTag& SemanticTag,
        const FGamePlatformPresentationContext& Context,
        FGamePlatformPresentationResolvedEntry& OutResolved) const;

    EGamePlatformPresentationSubmitResult Submit(
        const FGamePlatformPresentationRequest& Request);

    int32 GetWorldGeneration() const { return WorldGeneration; }
    int32 GetProviderCount() const { return Providers.Num(); }
    int32 GetContextContributorCount() const { return ContextContributors.Num(); }
    int32 GetCatalogFragmentCount() const { return CatalogFragments.Num(); }

    FGamePlatformPresentationRequestObserved& OnRequestObserved()
    {
        return RequestObserved;
    }

private:
    struct FProviderEntry
    {
        FGuid RegistrationId;
        FName ProviderId = NAME_None;
        int32 Priority = 0;
        FGamePlatformPresentationProviderHandler Handler;
    };

    struct FContextContributorEntry
    {
        FGamePlatformPresentationRegistrationHandle Handle;
        FName ContributorId = NAME_None;
        int32 Priority = 0;
        EGamePlatformPresentationContextScope Scope =
            EGamePlatformPresentationContextScope::LocalPlayer;
        TSharedPtr<IGamePlatformPresentationContextContributor> Contributor;
    };

    struct FCatalogFragmentEntry
    {
        FGamePlatformPresentationRegistrationHandle Handle;
        FGamePlatformPresentationCatalogFragment Fragment;
    };

    void RefreshWorldGeneration();
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    TArray<FProviderEntry> Providers;
    TArray<FContextContributorEntry> ContextContributors;
    TArray<FCatalogFragmentEntry> CatalogFragments;
    TWeakObjectPtr<UWorld> BoundWorld;
    int32 WorldGeneration = 1;
    FDelegateHandle WorldCleanupHandle;
    FGamePlatformPresentationRequestObserved RequestObserved;
};
