// 本文件属于GamePlatform平台层 GamePlatformPresentation，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
// 注册接口按值接收类约束并提供默认值；独立编译必须看到模板完整定义。
#include "Templates/SubclassOf.h"
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
        FGamePlatformPresentationProviderHandler Handler,
        TSubclassOf<UObject> DefinitionClass = nullptr);
    /** 游戏线程读取提供者声明的定义基类；缺服务/无类型声明返回nullptr，不猜测具体播放器类。 */
    UClass* GetProviderDefinitionClass(FName ProviderId) const;

    bool UnregisterProvider(const FGuid& RegistrationId);

    FGamePlatformPresentationRegistrationHandle RegisterContextContributor(
        TSharedRef<IGamePlatformPresentationContextContributor> Contributor);

    bool UnregisterContextContributor(
        const FGamePlatformPresentationRegistrationHandle& Handle);

    FGamePlatformPresentationRegistrationHandle RegisterCatalogFragment(
        const FGamePlatformPresentationCatalogFragment& Fragment);
    /** 游戏线程只读预检；同排序键资格可能重叠即失败并定位所有者/目录/条目，不加载或发布资源。 */
    bool PreflightCatalogFragment(const FGamePlatformPresentationCatalogFragment& Fragment, FString& OutError) const;

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
        /** 仅稳定类型合同；平台不引用任何具体播放器模块，不拥有资产或实例。 */
        TWeakObjectPtr<UClass> DefinitionClass;
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
