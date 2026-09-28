#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Extensions/DivineBeastsApplicationFlowExtension.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformFlowTypes.h"
#include "DivineBeastsApplicationFlowSubsystem.generated.h"

class IGamePlatformDataService;
class IGamePlatformOnlineAuthProvider;
class IDivineBeastsApplicationBackend;
class UDivineBeastsApplicationFlowContext;
class UGamePlatformApplicationFlowSubsystem;
class UGamePlatformFlowNode;
class UGamePlatformOnlineClientSubsystem;
class UGamePlatformSessionClientSubsystem;
struct FGamePlatformAuthSnapshot;
struct FGamePlatformLoadingSnapshot;
struct FGamePlatformSessionSnapshot;

/**
 * UDivineBeastsApplicationFlowSubsystem（神兽联盟应用流程协调子系统）。
 *
 * 职责：
 * - 作为 DivineBeasts（项目层）的客户端组合根，向平台流程注册项目 NodeFactory（节点工厂）；
 * - 通过 GamePlatformData（游戏平台数据）加载真实 FlowDefinition（流程定义）并调用 StartFlow；
 * - 把 UI（用户界面）命令和领域事件转换为当前节点的精确 SubmitEvent；
 * - 把平台快照和项目上下文投影为只读 FDivineBeastsFlowViewState。
 *
 * 重要边界：
 * 本子系统不维护第二套 CurrentState（当前状态），也不允许业务代码手工 TransitionTo（跳转）。
 * FlowRun、NodeId、NodeGeneration、超时、重试、循环与终态全部由 GamePlatformApplicationFlow 唯一管理。
 *
 * 性能：
 * 初始化时缓存稳定的 GameInstance 级服务和节点工厂句柄；运行期间完全事件驱动，
 * 不创建业务 Tick、不逐帧 GetSubsystem、不重复加载流程定义，也不逐帧广播 UI 状态。
 */
UCLASS()
class DIVINEBEASTSAPPLICATIONFLOWCLIENT_API UDivineBeastsApplicationFlowSubsystem final
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 创建项目流程上下文并异步加载正式FlowDefinition；只有真实Data Lease成功后才启动平台流程。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool StartFlow(bool bTryAutoLogin = true);

    /** 自动登录命令；当前只允许在 Authentication（认证）节点触发。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void TryAutoLogin();

    /** 人工认证命令；只调用Online领域服务，不直接推进流程。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void LoginWithCredentials(const FString& LoginName, const FString& Password);

    /** 提交角色创建草稿；验证通过后向当前CharacterEntry节点发送CreateCharacter具名事件。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool SubmitCharacterCreateDraft(const FDivineBeastsCharacterCreateDraft& Draft);

    /** 获取当前唯一的项目Character Creation Catalog（角色创建目录）。 */
    bool GetCharacterCreationHeroes(
        TArray<FGamePlatformCharacterCreationHeroDescriptor>& OutHeroes) const;

    /** 提交已有持久角色选择；真正所有权和版本仍以后端验证为准。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool SelectPersistentCharacter(const FString& CharacterId);

    /**
     * 在InWorld（世界内）状态请求进入另一个项目体验。
     * 这里只冻结目标Experience/Region并提交流程事件，服务器实例、Endpoint和Ticket仍由后端权威分配。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool RequestWorldAssignment(
        FName DesiredExperienceId,
        const FString& PreferredRegion = FString());

    /**
     * 提交当前地图观察事实。必须携带本次世界进入的ObservationId和后端分配的世界/体验身份。
     * 只有当前GameInstance中已经BeginPlay且与WorldDefinition一致的真实世界才被接受。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow", meta=(WorldContext="WorldContextObject"))
    bool NotifyWorldObserved(
        FGuid ObservationId,
        FName ExperienceId,
        FName WorldId,
        UObject* WorldContextObject);

    /** 报告当前操作的角色与控制器绑定完成；同时向Session报告ControllerReady本地可信事实。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool NotifyCharacterBindingReady(FGuid ObservationId);

    /** 报告玩法定义和必要运行数据已经可用；可选表现资源不属于强制屏障。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool NotifyGameplayDataReady(FGuid ObservationId);

    /** 报告项目级进入世界检查完成；该事实不能替代Session准入或真实World核验。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool NotifyProjectReadiness(FGuid ObservationId);

    /** 比赛结束后重新向后端申请新的OpenWorld分配，绝不复用赛前Endpoint或TransferTicket。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    bool RequestPostMatchReturnToWorld();

    /** 取消当前流程和异步工作，退出Online认证；LoggedOut事件到达后按原策略重新启动。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|ApplicationFlow")
    void LogoutAndRestart();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|ApplicationFlow")
    FDivineBeastsFlowViewState GetViewState() const { return ViewState; }

    /** 可选竞技等模块按GameInstance注册项目流程扩展；核心DBAClient不反向依赖DBAArena。 */
    bool RegisterExtension(
        FName ExtensionId,
        TSharedRef<IDivineBeastsApplicationFlowExtension> Extension);
    bool UnregisterExtension(FName ExtensionId);

    FDivineBeastsFlowViewStateChangedNative& OnViewStateChanged()
    {
        return ViewStateChanged;
    }

private:
    /** 注册本GameInstance全部项目ExecutorId；任一失败时回滚本次已注册句柄。 */
    bool RegisterNodeFactories();
    void UnregisterNodeFactories();

    /** 按ExecutorId创建本Run独立节点；简单异步步骤复用平台Callback节点，长期等待使用项目Passive节点。 */
    UGamePlatformFlowNode* CreateProjectNode(FName ExecutorId, UGameInstance& Owner);

    /** 平台Callback节点的统一执行入口；简单步骤复用这一函数，避免为每个HTTP请求创建额外UObject类型。 */
    void ExecuteProjectNode(
        FName ExecutorId,
        const FGamePlatformFlowContext& Context,
        FGamePlatformFlowCompletion Complete);
    /** 每次已开始尝试都会调用；只清理当前步骤拥有的请求，不推进流程。 */
    void FinishProjectNode(FName ExecutorId, EGamePlatformFlowFinishReason Reason);

    /** 释放尚未转移给ApplicationFlow的流程定义租约；活动Run的根租约由平台流程持有和释放。 */
    void ReleasePendingFlowDefinitionLease();

    /** 当前公开快照必须完整匹配Token才允许异步回调修改项目上下文。 */
    bool IsCurrentToken(const FGamePlatformFlowNodeToken& Token) const;
    bool IsCurrentNode(FName NodeId) const;
    FGamePlatformFlowNodeToken GetCurrentNodeToken() const;

    /** 只允许当前已开始节点提交事件；平台NodeGeneration负责O(1)拒绝旧回调。 */
    bool SubmitCurrentNodeEvent(
        FName ExpectedNodeId,
        FGamePlatformFlowNodeResult Event,
        FGamePlatformResult* OutResult = nullptr);

    FName DefaultExperienceForOnboarding() const;
    bool IsExperienceAllowedForCurrentProfile(FName ExperienceId) const;

    /** 启动世界Loading与真实Session传输；成功只表示两个领域操作已被接纳，不表示WorldReady。 */
    FGamePlatformResult BeginLoadingForAssignment();
    bool MarkLoadingFactReady(FGuid ObservationId, FName TaskId);
    bool ReleaseLoadingOperation();
    void TryCompleteWorldReady();
    /** 把已经发生的本地World/Controller事实同步给Session；带重入保护，不轮询。 */
    void TryReportLocalFactsToSession();
    void FailWorldReady(EDivineBeastsFlowError Error);

    void HandleFlowSnapshot(const FGamePlatformFlowSnapshot& Snapshot);
    void HandleFlowFinished(const FGamePlatformFlowSnapshot& Snapshot);
    void HandleAuthSnapshot(const FGamePlatformAuthSnapshot& Snapshot);
    void HandleSessionSnapshot(const FGamePlatformSessionSnapshot& Snapshot);
    void HandleLoadingSnapshot(const FGamePlatformLoadingSnapshot& Snapshot);

    /** 把跨节点业务数据同步到UI只读投影；只在领域事件发生时调用，不做逐帧复制。 */
    void RefreshProjectionFromContext();
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
    IGamePlatformDataService* Data = nullptr;

    TSharedPtr<IDivineBeastsApplicationBackend> Backend;
    /** 项目层真实 Gateway 认证适配；令牌仅保存在 Provider 私有内存，不进入 UObject/ViewState。 */
    TSharedPtr<IGamePlatformOnlineAuthProvider> AuthProvider;

    /** GameInstance作用域的项目流程载荷；不保存World/Actor/Widget强引用。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsApplicationFlowContext> FlowContext;

    FGamePlatformDataLease PendingFlowDefinitionLease;
    FGamePlatformFlowHandle ActiveFlow;
    TArray<FGamePlatformFlowFactoryHandle> FactoryHandles;

    FDelegateHandle FlowSnapshotHandle;
    FDelegateHandle FlowFinishedHandle;
    FDelegateHandle AuthHandle;
    FDelegateHandle SessionHandle;

    FGamePlatformLoadingHandle ActiveLoadingOperation;
    FGamePlatformLoadingRegistration LoadingSubscription;
    TArray<FGamePlatformLoadingRegistration> LoadingTaskFactories;
    TSharedPtr<class FDivineBeastsProjectLoadingContext> LoadingContext;

    FDivineBeastsFlowViewState ViewState;
    FGuid ActiveTransferOperationId;
    uint64 StartRequestGeneration = 0;
    uint64 LastAutoLoginNodeGeneration = 0;
    uint64 LastEnteredInWorldNodeGeneration = 0;
    int32 MaxRecoveryAttempts = 3;
    bool bRestartAfterLogout = false;
    bool bSynchronizingSessionFacts = false;

    TMap<FName, TSharedPtr<IDivineBeastsApplicationFlowExtension>> Extensions;
    /** 注册时排序一次，进入/离开世界的热路径直接迭代，避免每次GetKeys+Sort临时分配。 */
    TArray<FName> ExtensionOrder;
    FDivineBeastsFlowViewStateChangedNative ViewStateChanged;
};
