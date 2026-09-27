#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "DivineBeastsApplicationFlowSubsystem.generated.h"

class UGamePlatformApplicationFlowSubsystem;
class UGamePlatformOnlineClientSubsystem;
class UGamePlatformSessionClientSubsystem;
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

    /**
     * 提交当前地图观察事实。必须传入本次异步工作的观察身份、后端分配的世界/体验身份和实际世界上下文；
     * 只有当前实例中已开始运行且地图包匹配的世界会被接受。旧回调返回false且不改变新操作。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow", meta=(WorldContext="WorldContextObject"))
    bool NotifyWorldObserved(
        FGuid ObservationId,
        FName ExperienceId,
        FName WorldId,
        UObject* WorldContextObject);

    /** 提交当前操作中角色与控制器绑定完成的事实；身份必须在异步操作启动时捕获。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool NotifyCharacterBindingReady(FGuid ObservationId);

    /** 提交玩法定义及必要数据已可用的事实；可选表现资源不属于此屏障。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool NotifyGameplayDataReady(FGuid ObservationId);

    /** 提交项目级进入世界检查完成的事实；实际世界仍会由屏障持续核验。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool NotifyProjectReadiness(FGuid ObservationId);

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
    bool MarkLoadingFactReady(FGuid ObservationId, FName TaskId);
    bool ReleaseLoadingOperation();
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
    IGamePlatformLoadingService* Loading = nullptr;

    TSharedPtr<IDivineBeastsApplicationBackend> Backend;

    FDelegateHandle FlowHandle;
    FDelegateHandle AuthHandle;
    FDelegateHandle SessionHandle;
    FGamePlatformLoadingHandle ActiveLoadingOperation;
    FGamePlatformLoadingRegistration LoadingSubscription;
    TArray<FGamePlatformLoadingRegistration> LoadingTaskFactories;
    TSharedPtr<class FDivineBeastsProjectLoadingContext> LoadingContext;

    FDivineBeastsFlowViewState ViewState;
    FGuid ActiveTransferOperationId;
    FString PendingEndpoint;
    FString PendingTransferTicket;
    int32 RecoveryAttempts = 0;
    int32 MaxRecoveryAttempts = 3;
    bool bRestartAfterLogout = false;

    TMap<FName, TSharedPtr<IDivineBeastsApplicationFlowExtension>> Extensions;
    FDivineBeastsFlowViewStateChangedNative ViewStateChanged;
};
