// 本文件属于DivineBeasts项目层 DivineBeastsPresentationClient，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h" // OnEndPlay的公开参数类型，不依赖宿主PCH间接提供枚举。
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "Context/DivineBeastsPresentationContext.h"
#include "Facts/DivineBeastsPresentationFacts.h"
#include "Environment/DivineBeastsBirdFlockFlightSettings.h"
#include "GamePlatformPresentationCatalog.h"
#include "GamePlatformPresentationTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "DivineBeastsPresentationClientSubsystem.generated.h"

class UGamePlatformPresentationClientSubsystem;
class UWorld;struct FGamePlatformWeatherSnapshot;
class AGamePlatformCombatProjectile;
struct FGamePlatformCombatProjectileFacts;

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

    /** 可选Pending输出在预载广播前发布；调用方可在重入取消中识别本次申请，指针不跨调用保存。 */
    FDivineBeastsPresentationContentPackHandle ActivateContentPack(
        const FDivineBeastsPresentationContentPackFragment& Fragment,
        FString& OutError,
        FDivineBeastsPresentationContentPackHandle* OutPendingHandle = nullptr);

    /** 水体涟漪与飞溅的项目VFX目录：只有两个真实逻辑定义经GamePlatformData预载成功才发布。
     * 返回有效句柄只表示已接纳Loading，调用方须订阅内容包状态；此接口不会直接播放Niagara。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Water")
    FDivineBeastsPresentationContentPackHandle ActivateWaterVisuals(FString& OutError);

    /** 雨雪两套VFXDefinition完成真实资产预载后才注册项目天气目录，返回值只表示Loading已受理。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Weather")
    FDivineBeastsPresentationContentPackHandle ActivateWeatherVisuals(FString& OutError);

    /** 与VFX独立预载天气SFX内容包；缺音频不阻断视觉及权威天气。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Weather")
    FDivineBeastsPresentationContentPackHandle ActivateWeatherAudio(FString& OutError);

    /** 角色选择/创建共用的一份场景持续落叶；真资产异步激活成功后才播放。返回true表示受理。 */
    bool StartFrontEndFoliage(FName StyleId, const FVector& WorldOrigin,
        float GroundHeightCm, FString& OutError);
    /** 按RequestId取消持久Niagara并释放本地世界目录和Data预载租约。 */
    void StopFrontEndFoliage();

    /** 事件驱动创建/修改本地鸟群；种类Seagull/Swallow/Eagle，数量0..512，0取消该群。
     * FlockId为本世界场景锚点唯一名称；返回true只表示请求已受理，资源就绪后才提交。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Environment|BirdFlock")
    bool StartAmbientBirdFlock(FName FlockId, FName SpeciesId, int32 BirdCount,
        const FVector& WorldOrigin, FString& OutError);
    /** 场景C++入口：Owner须属于当前世界，RegistrationToken隔离注册代次；他人占用同名ID时拒绝。 */
    bool StartAmbientBirdFlockForOwner(FName FlockId, FName SpeciesId, int32 BirdCount,
        const FVector& WorldOrigin, UObject* Owner, const FGuid& RegistrationToken, FString& OutError);
    /** 高级路线、Boids与滑翔选择仍通过正式中立表现服务，失效Owner不能停止他人旧绑定。 */
    bool StartAmbientBirdFlockForOwnerAdvanced(FName FlockId, FName SpeciesId, int32 BirdCount,
        const FVector& WorldOrigin, UObject* Owner, const FGuid& RegistrationToken,
        const FDivineBeastsBirdFlockFlightSettings& FlightSettings, FString& OutError);
    /** 只撤销token匹配的自有注册；未知或旧代token返回false且不影响后来占用者。 */
    bool StopAmbientBirdFlockForOwner(FName FlockId, const FGuid& RegistrationToken);
    /** 显式按ID停止接口；场景组件必须使用带注册token的所有权接口。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Environment|BirdFlock")
    void StopAmbientBirdFlock(FName FlockId);
    /** 世界/账号退出时一次取消所有鸟群；重复调用安全，不会触发新加载。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Environment|BirdFlock")
    void StopAllAmbientBirdFlocks();
    /** 只报告请求配置数量，不冒充GPU当前实际可见数量。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Environment|BirdFlock")
    int32 GetRequestedBirdCount(FName FlockId) const;

    /** 事件驱动的本地蝴蝶/鱼群；Count为0..512、Seed为0..1000000，0撤销。
     * SpeciesId仅ButterflyAzure/FishSilver；true表示受理，真实Data资源Active后才提交。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Environment|Fauna")
    bool StartAmbientFauna(FName HabitatId, FName SpeciesId, int32 Count, int32 Seed,
        const FVector& WorldOrigin, FString& OutError);
    /** 场景Owner必须属于当前世界；注册token防止同名栖息地越权覆盖/停止。 */
    bool StartAmbientFaunaForOwner(FName HabitatId, FName SpeciesId, int32 Count, int32 Seed,
        const FVector& WorldOrigin, UObject* Owner, const FGuid& RegistrationToken, FString& OutError);
    bool StopAmbientFaunaForOwner(FName HabitatId, const FGuid& RegistrationToken);
    /** 仅解析当前Owner/token仍持有且已提交的群落；失败清空输出，防止旧注册读取后来同名请求。 */
    bool ResolveAmbientFaunaContextForOwner(FName HabitatId, const UObject* Owner,
        const FGuid& RegistrationToken, FName& OutContextId, FString& OutError) const;
    /** 显式服务入口按ID停止；组件必须走Owner/token入口。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Environment|Fauna")
    void StopAmbientFauna(FName HabitatId);
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Environment|Fauna")
    void StopAllAmbientFauna();
    /** 只返回请求配置数量，不表示GPU当前可见粒子数量。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Environment|Fauna")
    int32 GetRequestedFaunaCount(FName HabitatId) const;

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
    friend class FDivineBeastsDevelopmentMagicLifecycleTest;
    // 默认村庄鸟群回归只核验本地意图与世界清理；不绕过真实内容资格或假定GPU播放。
    friend class FDivineBeastsVillageDefaultBirdsTest;
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

    /** 真实内容包Active时调用；失败不得假装已播放。 */
    void SubmitFrontEndFoliage();
    enum class EEnvironmentFlockKind : uint8 { Bird, Fauna };
    struct FEnvironmentFlockState;
    /** Bird/Fauna共用意图事务；只保存项目值和中立RequestId，不是另一个实例执行器。 */
    bool StartEnvironmentFlock(FEnvironmentFlockState& Group, FName Id, FName SpeciesId,
        int32 Count, int32 Seed, const FVector& WorldOrigin, UObject* Owner,
        const FGuid& RegistrationToken, FString& OutError,
        const FDivineBeastsBirdFlockFlightSettings* BirdFlight = nullptr);
    bool EnsureEnvironmentFlockPack(FEnvironmentFlockState& Group, FString& OutError);
    void SubmitPendingEnvironmentFlocks(FEnvironmentFlockState& Group);
    /** 每世界一个公共VFX终态订阅；只失效当前RequestId，不在通知内自动重放。 */
    bool BindEnvironmentFlockCompletion(UWorld* World);
    void UnbindEnvironmentFlockCompletion();
    void CancelEnvironmentFlockRequest(const FEnvironmentFlockState& Group,
        const FGuid& RequestId, FName SpeciesId, FName Id);
    bool StopEnvironmentFlockForOwner(FEnvironmentFlockState& Group, FName Id, const FGuid& RegistrationToken);
    void StopEnvironmentFlock(FEnvironmentFlockState& Group, FName Id);
    void StopAllEnvironmentFlocks(FEnvironmentFlockState& Group);
    static FGameplayTag GetEnvironmentFlockTag(const FEnvironmentFlockState& Group, FName SpeciesId);
    /** 同世界Context变更先暂停全部环境群落；嵌套操作只在最外层恢复当前Desired。 */
    void SuspendEnvironmentFlocksForContextChange();
    void ResumeEnvironmentFlocksAfterContextChange();
    void StartDefaultWorldBirdFlock(UWorld* World);

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
    /** Context更新跨同步通知后须仍匹配；账号/世界退出使旧操作失效。 */
    FGuid ProjectContextUpdateToken;
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

    /** 同一份状态机供鸟群和蝴蝶/鱼群复用；平台VFX拥有实际Niagara实例及预算。 */
    struct FEnvironmentFlockRequestState
    {
        FName SpeciesId = NAME_None;
        FVector Origin = FVector::ZeroVector;
        int32 Count = 0;
        int32 Seed = 0;
        FGuid RequestId;
        /** 只存储弱Spline与标量；Fauna维持默认未启用状态。 */
        FDivineBeastsBirdFlockFlightSettings BirdFlight;
        bool bExplicitBirdFlight = false;
        /** 真终态后等待本ID显式Apply或新的Context恢复；其他群落的提交不会顺带重试。 */
        bool bAwaitingExplicitRetry = false;
        /** 注册token有效表示场景拥有；无token条目属于显式服务请求。 */
        TWeakObjectPtr<UObject> Owner;
        FGuid RegistrationToken;
        FGuid MutationToken;
    };
    struct FEnvironmentFlockState
    {
        EEnvironmentFlockKind Kind = EEnvironmentFlockKind::Bird;
        TMap<FName, FEnvironmentFlockRequestState> Requests;
        FDivineBeastsPresentationContentPackHandle Pack;
        TWeakObjectPtr<UWorld> World;
        bool bStopping = false;
    };
    FEnvironmentFlockState BirdFlocks;
    FEnvironmentFlockState AmbientFauna{EEnvironmentFlockKind::Fauna};
    /** 覆盖整个世界/账号清理；Context暂停期间仍允许合法Desired排队。 */
    bool bStoppingEnvironmentFlocks = false;
    int32 EnvironmentFlockContextSuspensions = 0;
    TWeakObjectPtr<UWorld> EnvironmentCompletionWorld;
    FDelegateHandle EnvironmentCompletionHandle;
    FGuid EnvironmentCompletionToken;

    /** 角色预览工作室共享的单组自然飘落VFX，和天气/战斗独立。 */
    FDivineBeastsPresentationContentPackHandle FrontEndFoliagePack;
    FGuid FrontEndFoliageRequestId;
    FName FrontEndFoliageStyle = NAME_None;
    FVector FrontEndFoliageOrigin = FVector::ZeroVector;
    float FrontEndFoliageGroundHeightCm = 0.f;

    /** 水体VFX目录只属于本World和本地玩家，切图/退出撤销，不进入专用服务器。 */
    FDivineBeastsPresentationContentPackHandle WaterVisualHandle;

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

    /** 本地玩家独占开发法术表现适配，不持有伤害/运动权威；全部句柄在旅行/退出撤销。 */
    struct FDevelopmentMagicFlight
    {
        FDelegateHandle FactsHandle;
        FGuid FlightRequestId;
        FName StyleId = NAME_None;
        bool bImpactSubmitted = false;
        bool bResidueSubmitted = false; // 仅寒冰箭地面残霜成功播放一次，不参与权威碰撞。
        bool bFlightEnded = false;
    };
    TWeakObjectPtr<UWorld> DevelopmentMagicWorld;
    /** 跨同步Submit/预载通知后核对本世界绑定所有权；解绑先换代，旧调用不能尾写新账本。 */
    FGuid DevelopmentMagicBindingToken;
    FDelegateHandle DevelopmentMagicSpawnHandle, DevelopmentMagicPackHandle;
    FDivineBeastsPresentationContentPackHandle DevelopmentMagicPack;
    TMap<TWeakObjectPtr<AGamePlatformCombatProjectile>, FDevelopmentMagicFlight> DevelopmentMagicFlights;
    void BindDevelopmentMagicWorld(UWorld* World);
    void UnbindDevelopmentMagicWorld();
    void HandleDevelopmentMagicSpawn(AActor* Actor);
    /** EndPlay与内容包状态无关，缺资源/未初始化弹体也必须立即撤销订阅与账本。 */
    UFUNCTION() void HandleDevelopmentMagicEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    void HandleDevelopmentMagicFacts(AGamePlatformCombatProjectile* Actor, const FGamePlatformCombatProjectileFacts& Facts);
    void HandleDevelopmentMagicPack(const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State, const FString& Error);
};
