// 本文件属于DivineBeasts项目层 DivineBeastsPresentationClient，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "Context/DivineBeastsPresentationContext.h"
#include "Facts/DivineBeastsPresentationFacts.h"
#include "GamePlatformPresentationCatalog.h"
#include "GamePlatformPresentationTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsPresentationClientSubsystem.generated.h"

class UGamePlatformPresentationClientSubsystem;
class UWorld;struct FGamePlatformWeatherSnapshot;

DECLARE_MULTICAST_DELEGATE_FourParams(
    FDivineBeastsPresentationLogicalPreloadRequested,
    const FGuid&,
    FName,
    const TArray<FName>&,
    bool);

DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsPresentationLogicalPreloadCancelled,
    const FGuid&);

/** 事务可诊断通知；在记录提交/回滚后游戏线程广播，允许观察者同步取消或退出。 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDivineBeastsPresentationContentPackChanged,
    const FDivineBeastsPresentationContentPackHandle&, EDivineBeastsPresentationContentPackState, const FString&);

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

    /** 雨雪两套VFXDefinition完成真实资产预载后才注册项目天气目录，返回值只表示Loading已受理。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Weather")
    FDivineBeastsPresentationContentPackHandle ActivateWeatherVisuals(FString& OutError);

    /** 与VFX独立预载天气SFX内容包；缺音频不阻断视觉及权威天气。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Weather")
    FDivineBeastsPresentationContentPackHandle ActivateWeatherAudio(FString& OutError);

    bool DeactivateContentPack(
        const FDivineBeastsPresentationContentPackHandle& Handle);
    /** 接纳后通过状态/通知确认真正Active；无资产/错误/取消均有具体失败，Loading不是成功发布。 */
    EDivineBeastsPresentationContentPackState GetContentPackState(
        const FDivineBeastsPresentationContentPackHandle& Handle, FString& OutError) const;
    FDivineBeastsPresentationContentPackChanged& OnContentPackChanged() { return ContentPackChanged; }
    /** 默认公共VFX合同的当前配置错误；空值仅表示默认定义预载及目录发布成功。 */
    const FString& GetDefaultCatalogConfigurationError() const { return DefaultCatalogError; }

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
    friend class FDivineBeastsPresentationActivationRegressionTest;
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
        int64 Generation = 0;
        int32 RemainingLoads = 0;
        bool bSucceeded = false;
        TArray<FGamePlatformDataLease> Leases;
        FDivineBeastsPresentationContentPackHandle PackHandle;
    };
    struct FPendingPack
    {
        FDivineBeastsPresentationContentPackHandle Handle;
        FDivineBeastsPresentationContentPackFragment Fragment;
    };
    struct FPackTerminal
    {
        FDivineBeastsPresentationContentPackHandle Handle;
        EDivineBeastsPresentationContentPackState State = EDivineBeastsPresentationContentPackState::Failed;
        FString Error;
    };
    FGuid BeginLogicalPreload(FName OwnerScopeId, const TArray<FName>& DefinitionIds, bool bRequired,
        const FDivineBeastsPresentationContentPackHandle& PackHandle);
    void HandleLogicalPreloadCompleted(FGuid RequestId, int64 Generation,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void RecordPackTerminal(const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State, const FString& Error);
    void ReleaseLogicalLeases(const TArray<FGamePlatformDataLease>& Leases);

    void RegisterProjectState();
    void BeginDefaultCatalogPreload();
    void UnregisterProjectState();
    void HandleWorldCleanup(
        UWorld* World,
        bool bSessionEnded,
        bool bCleanupResources);
    void HandlePostLoadMap(UWorld* World);
    /** 只绑定当前LocalPlayer所属世界，消费第一次服务器天气快照后启动可选内容包。 */
    void BindWeatherWorld(UWorld* World);
    void UnbindWeatherWorld();
    void HandleWeatherSnapshot(const FGamePlatformWeatherSnapshot& Snapshot);
    FDivineBeastsPresentationContentPackHandle ActivateWeatherPack(bool bVisual, FString& OutError);
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
    FGuid DefaultPreloadRequestId;
    FString DefaultCatalogError;

    TMap<FGuid, FActivePack> ActivePacks;
    TMap<FGuid, FPendingPack> PendingPacks;
    TMap<FGuid, FPackTerminal> PackTerminals;
    TArray<FGuid> PackTerminalOrder;
    FGuid ScopeId;
    int64 NextPreloadGeneration = 0;
    bool bClosing = false;
    TMap<FGuid, FLogicalPreload> LogicalPreloads;
    /** 同一租约重复完成不能把RemainingLoads提前扣完；请求撤销时一并清理。 */
    TMap<FGuid, TSet<FGuid>> CompletedPreloadLeases;
    TMap<FGuid, EGamePlatformPresentationPredictionState> RequestStates;
    TArray<FGuid> RequestOrder;

    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle PostLoadMapHandle;

    /** 本地天气表现独立于游戏模式，和用户身份/World代次共同决定目录预载期限。 */
    TWeakObjectPtr<UWorld> WeatherBoundWorld;
    FDelegateHandle WeatherSnapshotHandle;
    FDivineBeastsPresentationContentPackHandle WeatherVisualHandle;
    FDivineBeastsPresentationContentPackHandle WeatherAudioHandle;
    bool bWeatherVisualAttempted = false;
    bool bWeatherAudioAttempted = false;

    FDivineBeastsPresentationLogicalPreloadRequested LogicalPreloadRequested;
    FDivineBeastsPresentationLogicalPreloadCancelled LogicalPreloadCancelled;
    FDivineBeastsPresentationContentPackChanged ContentPackChanged;
};
