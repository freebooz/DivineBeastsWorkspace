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
#include "Types/GamePlatformCombatEvent.h" // 本地主机动态事件桥接的反射参数需要完整类型。
#include "DivineBeastsPresentationClientSubsystem.generated.h"

class UGamePlatformPresentationClientSubsystem;
class UGamePlatformCombatComponent;
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
    /** 旅行后本地控制器绑定可能迟于PostLoadMap；事件驱动补齐村庄音乐首次装载。 */
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

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
    /** 已经注册且加载完成时播放已审核UI音效；客户端失败只影响表现。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Audio")
    bool PlayStarterUISound(FName ActionId);
    /** 第二批客户端音频按分类预载；返回Loading句柄不代表已经听到声音。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|AudioLibrary")
    FDivineBeastsPresentationContentPackHandle ActivateAudioLibraryCategory(FName Category, FString& OutError);
    /** 客户端审核界面查询分类预载终态；Active时允许提交声音。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|AudioLibrary")
    EDivineBeastsPresentationContentPackState GetAudioLibraryCategoryState(FName Category) const;
    /** 使用已登记ClipId（例如Spell.Healing）播放一次；不接受任意uasset路径。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|AudioLibrary")
    bool PlayAudioLibraryClip(FName ClipId, FVector Location);
    /** 同一服务器确认事实的SFX使用稳定OccurrenceId，复制重放时不得叠加第二段声音。 */
    bool PlayAudioLibraryFactClip(FName ClipId, FVector Location, const FGuid& OccurrenceId);

    /** 真实世界声源锚点循环（仅允许水流A/B），调用方负责在Anchor退出时停止。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|AudioLibrary")
    bool StartAudioLibraryLoop(FName AnchorId, FName ClipId, FVector Location);
    /** 停止指定锚点所拥有的循环声音，不会取消其他锚点声音。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|AudioLibrary")
    bool StopAudioLibraryLoop(FName AnchorId);


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
    friend class FDivineBeastsHealingPresentationTest;
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
    /** 在客户端场景启动本世界的音乐/环境以及UI/法术音效定义预载，失败安全静音。 */
    void EnsureStarterAudioForWorld(UWorld* World);
    void StopStarterAudioForWorld();
    void SubmitStarterAudioForWorld();
    /** 根据复制法术阶段独立提交音效；返回平台是否受理，不依赖VFX结果。 */
    bool PlayStarterSpellSound(FName SpellName, bool bImpact, const FVector& Location);
    bool SubmitStarterOneShot(const TCHAR* Semantic, FName ContextId, const FVector& Location);
    /** 客户端进世界时按场景预热UI/法术/地表等音效；按World作用域清理，零Tick。 */
    void EnsureAudioLibraryForWorld(UWorld* World);
    void StopAudioLibraryForWorld();
    /** 只允许在已激活的分类目录中解析特定声音标签，不暴露声音资产路径。 */
    bool ResolveAudioLibraryClip(FName ClipId, FGameplayTag& OutSemantic) const;
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
    /** 首批客户端音频按播放领域分包预载，UI/技能/音乐/环境任何单类失败不相互阻塞。 */
    FDivineBeastsPresentationContentPackHandle StarterAudioPack; // UI按钮三音效
    FDivineBeastsPresentationContentPackHandle StarterSpellPack; // 原型施法三音效
    FDivineBeastsPresentationContentPackHandle StarterMusicPack; // 当前世界一段背景音乐
    FDivineBeastsPresentationContentPackHandle StarterAmbiencePack; // 新手村森林氛围
    TWeakObjectPtr<UWorld> StarterAudioWorld;
    FGuid StarterMusicRequestId;
    FGameplayTag StarterMusicTag;
    FGuid StarterAmbienceRequestId;

    /** 第二批音频库分类内容包：最多八类，全属于当前客户端World，非项目外资源池。 */
    TWeakObjectPtr<UWorld> AudioLibraryWorld;
    TMap<FName, FDivineBeastsPresentationContentPackHandle> AudioLibraryCategories;
    struct FAudioLibraryLoop
    {
        FGuid RequestId;
        FName ClipId = NAME_None;
        FVector Location = FVector::ZeroVector;
        FGameplayTag SemanticTag;
    };
    /** 仅由明确场景锚点身份管理的可取消声音；Owner退出必须Stop，不扫描世界或逐帧轮询。 */
    TMap<FName, FAudioLibraryLoop> AudioLibraryLoops;

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
        bool bFlightAudioSubmitted = false;
        bool bImpactAudioSubmitted = false;
        bool bResidueSubmitted = false; // 仅寒冰箭地面残霜成功播放一次，不参与权威碰撞。
        bool bFlightEnded = false;
    };
    TWeakObjectPtr<UWorld> DevelopmentMagicWorld;
    /** 跨同步Submit/预载通知后核对本世界绑定所有权；解绑先换代，旧调用不能尾写新账本。 */
    FGuid DevelopmentMagicBindingToken;
    FDelegateHandle DevelopmentMagicSpawnHandle, DevelopmentMagicPackHandle;
    /** 仅订阅本世界确认治疗反馈；目标实例由平台去重事实决定，旅行时成对解绑。 */
    FDelegateHandle DevelopmentHealingFeedbackHandle;
    /** 主机/单机无客户端Multicast，只订阅本World权威Combat；弱引用不延长实体生命周期。 */
    TSet<TWeakObjectPtr<UGamePlatformCombatComponent>> DevelopmentAuthorityHealingSources;
    FDivineBeastsPresentationContentPackHandle DevelopmentMagicPack;
    TMap<TWeakObjectPtr<AGamePlatformCombatProjectile>, FDevelopmentMagicFlight> DevelopmentMagicFlights;
    void BindDevelopmentMagicWorld(UWorld* World);
    void UnbindDevelopmentMagicWorld();
    void HandleDevelopmentMagicSpawn(AActor* Actor);
    /** EndPlay与内容包状态无关，缺资源/未初始化弹体也必须立即撤销订阅与账本。 */
    UFUNCTION() void HandleDevelopmentMagicEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    void HandleDevelopmentMagicFacts(AGamePlatformCombatProjectile* Actor, const FGamePlatformCombatProjectileFacts& Facts);
    /** 只将实际受疗者的同代确认事件投影为树叶视觉，不读UI选择、不补血、不回放旧世界事件。 */
    void HandleDevelopmentHealingFeedback(const FGamePlatformCombatEvent& Event);
    /** 单机/主机的权威确认治疗转交既有World反馈总线；源/目标重复通知使用同一EventId去重。 */
    UFUNCTION() void HandleDevelopmentAuthorityHealingFeedback(const FGamePlatformCombatEvent& Event);
    void HandleDevelopmentMagicPack(const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State, const FString& Error);
};
