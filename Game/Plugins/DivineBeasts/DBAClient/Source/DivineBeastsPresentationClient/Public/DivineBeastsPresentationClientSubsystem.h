#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "Context/DivineBeastsPresentationContext.h"
#include "Facts/DivineBeastsPresentationFacts.h"
#include "GamePlatformPresentationCatalog.h"
#include "GamePlatformPresentationTypes.h"
#include "DivineBeastsPresentationClientSubsystem.generated.h"

class UGamePlatformPresentationClientSubsystem;
class UWorld;

DECLARE_MULTICAST_DELEGATE_FourParams(
    FDivineBeastsPresentationLogicalPreloadRequested,
    const FGuid&,
    FName,
    const TArray<FName>&,
    bool);

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsPresentationLogicalPreloadCancelled,
    const FGuid&);

/**
 * UDivineBeastsPresentationClientSubsystem（神兽联盟项目表现客户端子系统）。
 * 只注册Context/Catalog/ContentPack并向平台Coordinator提交中立请求。
 */
UCLASS()
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsPresentationClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool UpdateProjectContext(
        const FDivineBeastsPresentationProjectContext& Context,
        FString& OutError);

    void ResetForAccountSwitch();

    FGamePlatformPresentationContextPatch BuildProjectContextPatch() const;

    FDivineBeastsPresentationContentPackHandle ActivateContentPack(
        const FDivineBeastsPresentationContentPackFragment& Fragment,
        FString& OutError);

    bool DeactivateContentPack(
        const FDivineBeastsPresentationContentPackHandle& Handle);

    EGamePlatformPresentationSubmitResult SubmitWorldInteractionFact(
        const FDivineBeastsWorldInteractionPresentationFact& Fact);

    EGamePlatformPresentationSubmitResult SubmitVillageFeedbackFact(
        const FDivineBeastsVillageFeedbackPresentationFact& Fact);

    FGuid RequestLogicalPreload(
        FName OwnerScopeId,
        const TArray<FName>& DefinitionIds,
        bool bRequired);

    bool CancelLogicalPreload(const FGuid& RequestId);

    int32 GetActiveContentPackCount() const { return ActivePacks.Num(); }
    int32 GetLogicalPreloadCount() const { return LogicalPreloads.Num(); }

    FDivineBeastsPresentationLogicalPreloadRequested&
    OnLogicalPreloadRequested()
    {
        return LogicalPreloadRequested;
    }

    FDivineBeastsPresentationLogicalPreloadCancelled&
    OnLogicalPreloadCancelled()
    {
        return LogicalPreloadCancelled;
    }

private:
    struct FActivePack
    {
        FDivineBeastsPresentationContentPackHandle Handle;
        FName ContentPackId = NAME_None;
        EGamePlatformPresentationContextScope LifecycleScope =
            EGamePlatformPresentationContextScope::World;
        FGamePlatformPresentationRegistrationHandle CatalogHandle;
        TArray<FGuid> PreloadRequestIds;
    };

    struct FLogicalPreload
    {
        FGuid RequestId;
        FName OwnerScopeId = NAME_None;
        TArray<FName> DefinitionIds;
        bool bRequired = false;
    };

    void RegisterProjectState();
    void UnregisterProjectState();
    void HandleWorldCleanup(
        UWorld* World,
        bool bSessionEnded,
        bool bCleanupResources);
    void HandlePostLoadMap(UWorld* World);
    void DeactivatePacksByScope(
        EGamePlatformPresentationContextScope Scope);
    void CancelAllLogicalPreloads();
    bool ShouldDispatch(
        const FGuid& RequestId,
        EGamePlatformPresentationPredictionState State);
    EGamePlatformPresentationSubmitResult SubmitProjectRequest(
        FGamePlatformPresentationRequest Request);

    UGamePlatformPresentationClientSubsystem* PlatformPresentation = nullptr;

    FDivineBeastsPresentationProjectContext ProjectContext;
    FGamePlatformPresentationRegistrationHandle ContextContributorHandle;
    FGamePlatformPresentationRegistrationHandle DefaultCatalogHandle;

    TMap<FGuid, FActivePack> ActivePacks;
    TMap<FGuid, FLogicalPreload> LogicalPreloads;
    TMap<FGuid, EGamePlatformPresentationPredictionState> RequestStates;
    TArray<FGuid> RequestOrder;

    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle PostLoadMapHandle;

    FDivineBeastsPresentationLogicalPreloadRequested LogicalPreloadRequested;
    FDivineBeastsPresentationLogicalPreloadCancelled LogicalPreloadCancelled;
};
