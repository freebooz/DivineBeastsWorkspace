#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "DivineBeastsApplicationFlowSubsystem.generated.h"

class UGamePlatformApplicationFlowSubsystem;
class UGamePlatformOnlineClientSubsystem;
class UGamePlatformSessionClientSubsystem;
class UGamePlatformLoadingClientSubsystem;
class IDivineBeastsApplicationBackend;
struct FGamePlatformFlowSnapshot;
struct FGamePlatformAuthSnapshot;
struct FGamePlatformSessionSnapshot;
struct FGamePlatformLoadingSnapshot;

/**
 * UDivineBeastsApplicationFlowSubsystem（神兽联盟应用流程协调子系统）。
 * 不拥有第二套CurrentState；权威节点/FlowRun/Generation全部委托GamePlatformApplicationFlow。
 */
UCLASS()
class DIVINEBEASTSAPPLICATIONFLOWCLIENT_API UDivineBeastsApplicationFlowSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool StartFlow(bool bTryAutoLogin = true);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void LoginWithCredentials(
        const FString& LoginName,
        const FString& Password);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool SubmitCharacterCreateDraft(
        const FDivineBeastsCharacterCreateDraft& Draft);

    /** 获取当前注册的项目Character Creation Catalog（角色创建目录）。 */
    bool GetCharacterCreationHeroes(
        TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool SelectPersistentCharacter(const FString& CharacterId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool RequestWorldAssignment(
        FName DesiredExperienceId,
        const FString& PreferredRegion = FString());

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void NotifyWorldObserved(
        FName ExperienceId,
        FName WorldId);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void NotifyCharacterBindingReady();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void NotifyGameplayDataReady();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void NotifyProjectReadiness();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool RequestPostMatchReturnToWorld();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void LogoutAndRestart();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|ApplicationFlow")
    FDivineBeastsFlowViewState GetViewState() const { return ViewState; }

    bool RegisterExtension(
        FName ExtensionId,
        TSharedRef<IDivineBeastsApplicationFlowExtension> Extension);
    bool UnregisterExtension(FName ExtensionId);

    FDivineBeastsFlowViewStateChangedNative& OnViewStateChanged()
    {
        return ViewStateChanged;
    }

private:
    bool RegisterProjectNodes();
    bool TransitionTo(FName NodeId);
    bool IsCurrentNode(FName NodeId) const;

    void BeginLoadProfile();
    void BeginLoadRoster();
    void BeginSelection(const FDivineBeastsCharacterSummary& Character);
    void HandleValidatedSelection(
        const FDivineBeastsValidatedSelection& Selection);
    FName DefaultExperienceForOnboarding() const;
    bool IsExperienceAllowedForCurrentProfile(FName ExperienceId) const;

    void BeginLoadingForAssignment(
        const FString& Endpoint,
        const FString& TransferTicket);
    void MarkLoadingTaskReady(FName TaskId);
    void TryCompleteWorldReady();
    void BeginRecovery(EDivineBeastsFlowError Error);

    void HandleFlowSnapshot(const FGamePlatformFlowSnapshot& Snapshot);
    void HandleAuthSnapshot(const FGamePlatformAuthSnapshot& Snapshot);
    void HandleSessionSnapshot(const FGamePlatformSessionSnapshot& Snapshot);
    void HandleLoadingSnapshot(const FGamePlatformLoadingSnapshot& Snapshot);

    void ResetProjection();
    void SetError(EDivineBeastsFlowError Error);
    void SetBusy(bool bBusy);
    void RefreshAllowedActions();
    void BroadcastView();
    void NotifyExtensionsEnteredWorld();
    void NotifyExtensionsLeavingWorld();

    UGamePlatformApplicationFlowSubsystem* PlatformFlow = nullptr;
    UGamePlatformOnlineClientSubsystem* Online = nullptr;
    UGamePlatformSessionClientSubsystem* Session = nullptr;
    UGamePlatformLoadingClientSubsystem* Loading = nullptr;

    TSharedPtr<IDivineBeastsApplicationBackend> Backend;

    FDelegateHandle FlowHandle;
    FDelegateHandle AuthHandle;
    FDelegateHandle SessionHandle;
    FDelegateHandle LoadingHandle;

    FDivineBeastsFlowViewState ViewState;
    FGuid ActiveLoadingOperationId;
    FGuid ActiveTransferOperationId;
    FString PendingEndpoint;
    FString PendingTransferTicket;
    int32 RecoveryAttempts = 0;
    int32 MaxRecoveryAttempts = 3;
    bool bRestartAfterLogout = false;

    TMap<FName, TSharedPtr<IDivineBeastsApplicationFlowExtension>> Extensions;
    FDivineBeastsFlowViewStateChangedNative ViewStateChanged;
};
