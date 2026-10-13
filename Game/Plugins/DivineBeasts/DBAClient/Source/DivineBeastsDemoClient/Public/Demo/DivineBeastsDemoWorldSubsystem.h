// 开发演示世界交互控制：只使用真实BirdFlock和Weather审核Actor公开API，不模拟播放或修改服务器权威。
// 所有临时参数按World作用域保存；世界关闭前恢复/释放，禁止跨世界使用旧Actor。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "Environment/DivineBeastsBirdFlockFlightSettings.h"
#include "Contracts/DivineBeastsDemoRunTypes.h"
#include "DivineBeastsDemoWorldSubsystem.generated.h"

class UDivineBeastsBirdFlockSceneComponent;
class UDivineBeastsBirdFlockBatchComponent;
class ADivineBeastsWeatherReviewController;
class UDivineBeastsDemoHubWidget;
class APlayerController;
class UUserWidget;
struct FDivineBeastsDemoWorldRunPrivateState;

/**
 * 神兽联盟插件演示世界运行上下文。DevClient/Editor且显式DBAPluginDemo时才创建。
 * 游戏世界内主动管理真效果、演示UI和资源租约；不在Server、正式流程或Commandlet创建。
 */
UCLASS()
class DIVINEBEASTSDEMOCLIENT_API UDivineBeastsDemoWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** 仅修改正式BirdFlockSceneComponent并触发Presentation/Data/VFX链路，不再调用旧ISM。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdCount(int32 Count);

    /** 调整正式鸟群场景配置的质量比例，实际GPU可见数仍由Niagara EffectType决定。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdQuality(float Fraction);
    /** 切换正式鸟群物种配置，未知物种由项目表现目录拒绝，失败回退当前配置。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdSpecies(FName SpeciesId);

    /** 可选路径模式；仅Spline模式需同世界有效Spline组件与完成相应Niagara资源。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdFlightMode(EDivineBeastsBirdFlockFlightMode Mode);

    /** 标准Boids三权重，若实际资源Schema/邻域模块未就绪则明确失败。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdFlocking(float SeparationWeight, float AlignmentWeight, float CohesionWeight);

    /** 动态切换滑翔混合；0保留原材质WPO，不代表VAT资产存在。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool ApplyBirdGlide(float GlideBlend);

    /** 批次控制只持本组件Owner/Token，不会清除其它插件演示群。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    bool StartBirdBatches();

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|VFX")
    void StopBirdBatches();

    /** 真实天气审核Actor仅在世界权威上下文允许；不能以本函数绕过专服。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo|Weather")
    bool ApplyWeather(EGamePlatformWeatherType Weather, float TransitionSeconds);

    /** 只恢复本控制器捕获的场景鸟群数值/天气；不停止Batch，失败保留记录，不影响其他实例。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Demo")
    bool ResetDemoChanges();

    /** 只恢复本控制器首次捕获的SceneComponent，不停止任何Batch或改变天气；失败保留原值供重试。 */
    bool RestoreBirdEdits();

    /** -1表示当前未做真实Niagara GPU读回；绝不返回旧ISM计数冒充真实鸟数。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetActualBirdCount() const;
    /** 已受理的项目正式服务鸟数（请求数），不代表GPU现存粒子数。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetRequestedBirdCount() const;

    /** 当前世界正式鸟群场景组件数量，不是逐鸟Actor数量。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo|VFX")
    int32 GetBirdActorCount() const;

    /** 当前世界演示请求结果，供UI显示，不存储敏感信息。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Demo")
    FText GetLastFeedback() const { return LastFeedback; }

    /** 开始本世界独立案例；同世界已有可写案例时拒绝，进入Running需真实准备完成。 */
    bool BeginScenario(const FDivineBeastsDemoScenarioDefinition& Scenario);
    /** 提交类型化命令，返回仅表示受理；结果通过OnRunChanged事件发布。 */
    bool RequestScenarioOperation(FName OperationId, const TArray<FDivineBeastsDemoParameterValue>& Parameters);
    /** 停止/取消当前运行；重复停止只等待已有清理，不重复调用适配器。 */
    bool RequestScenarioStop(const FGamePlatformResult& DesiredTerminal);
    /** 是否持有本世界可写案例；准备和清理也占用，禁止并行复位同一场景。 */
    bool HasActiveScenario() const;
    /** 当前/最近只读运行快照，保留操作和清理的不同结果。 */
    FDivineBeastsDemoRunSnapshot GetRunSnapshot() const;
    /** 活跃适配身份，用于提供者注销时取消关联运行。 */
    FName GetActiveAdapterId() const;
    /** 旧兼容界面是否仍拥有未恢复修改；新案例拒绝接管这份外部所有权。 */
    bool HasPendingLegacyEdits() const { return !OriginalBirds.IsEmpty() || bHasOriginalWeather; }
    /** 已真实加载的样板控件，只供Monolith页面的NamedSlot承载，不拥有业务权威。 */
    UUserWidget* GetSampleWidget() const { return ActiveSampleWidget.Get(); }
    /** 导出当前运行的真实待审证据；未编目/无版本失败，不填写人工签收。 */
    bool WriteCurrentEvidence(FString& OutIndexPath, FString& OutError);
    /** 只读事件，界面失活即解绑，不轮询世界或业务状态。 */
    FDivineBeastsDemoRunChanged OnRunChanged;

private:
    /** 演示修改前的正式组件快照；失败时原位回滚，离开世界不访问已失效组件。 */
    struct FOriginalBirdState
    {
        int32 Count = 0;
        FName Species = NAME_None;
        bool bAdvanced = false;
        FDivineBeastsBirdFlockFlightSettings Flight;
    };
    TMap<TWeakObjectPtr<UDivineBeastsBirdFlockSceneComponent>, FOriginalBirdState> OriginalBirds;
    /** 首次可写操作前记录原状态，离开世界只恢复本演示改变的组件。 */
    void CaptureOriginalBird(UDivineBeastsBirdFlockSceneComponent& Bird);
    /** 执行正式Apply并在失败时还原用户可编辑字段。 */
    bool CommitBirdChange(UDivineBeastsBirdFlockSceneComponent& Bird,
        const FOriginalBirdState& Before, FString& OutError);
    /** 有界同步演示操作：保存原值、实际调用正式VFX链路、失败恢复本地可编辑字段。 */
    bool ApplyBirdEdits(const TCHAR* Description,
        TFunctionRef<void(UDivineBeastsBirdFlockSceneComponent&)> Edit);
    TWeakObjectPtr<ADivineBeastsWeatherReviewController> OriginalWeatherActor;
    EGamePlatformWeatherType OriginalWeatherType = EGamePlatformWeatherType::Clear;
    bool bHasOriginalWeather = false;

    /** GamePlatformData（平台数据）颁发的普通资源租约，退出只释放本世界所持身份。 */
    FGamePlatformDataLease UILease;
    TWeakObjectPtr<UDivineBeastsDemoHubWidget> ActiveWidget;
    /** 新页面由LocalPlayer平台层栈拥有，不以旧HubWidget强转，不直接AddToViewport。 */
    TWeakObjectPtr<class UGamePlatformUIManagerSubsystem> DemoUIManager;
    TWeakObjectPtr<class UGamePlatformUIScreen> ActiveScreen;
    FGuid PendingScreenRequestId;
    uint64 OverlayGeneration = 0;
    bool bRegisteredDemoScreen = false;
    bool bOpeningScreen = false;
    bool bSynchronousScreenFailure = false;
    bool bOverlayClosing = false;
    /** 只在本世界演示UI存在期间接管输入；释放时恢复默认游戏输入和原鼠标显示状态。 */
    TWeakObjectPtr<APlayerController> DemoInputController;
    bool bOriginalMouseCursorVisible = false;
    TWeakObjectPtr<UWorld> SourceWorld;
    FText LastFeedback;
    /** 私有状态机、适配实例和计时器；资源租约仍由中央Data服务签发。 */
    TSharedPtr<FDivineBeastsDemoWorldRunPrivateState> RunPrivateState;
    /** 强持实际加载的样板Widget，世界退出移除/释放；不创建C++视觉树。 */
    UPROPERTY(Transient) TObjectPtr<UUserWidget> ActiveSampleWidget;
    /** 同步适配调用期间允许本案例写入；旧兼容控件不能绕过活动案例独占。 */
    bool bExecutingScenarioAdapter = false;
    /** 销毁事件先失效运行，再撤销委托/计时器/本次租约。 */
    void ShutdownScenario();
    /** 内部准备与异步完成均核对Token/代次，运行结果至多一次。 */
    void PrepareScenarioAdapter();
    void PublishRunSnapshot();
    void ArmScenarioDeadline();
    void OnScenarioDeadline();
    void OnScenarioResourcesReady(const FDivineBeastsDemoRunToken& Token,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void OnScenarioPrepared(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Result);
    void OnScenarioOperationCompleted(const FDivineBeastsDemoRunToken& Token, uint64 Serial,
        const FGamePlatformResult& Result);
    void OnScenarioStopCompleted(const FDivineBeastsDemoRunToken& Token, const FGamePlatformResult& Result);
    /** 清理本次普通资源/样板控件；不撤销其他世界的需求。 */
    FGamePlatformResult ReleaseScenarioResources();

    void RequestDemoOverlay(UWorld& World);
    void OnOverlayAssetReady(FSoftObjectPath WidgetClassPath, const FGamePlatformDataLease& Lease,
        bool bSucceeded);
    void ReleaseDemoOverlay();
    /** 新目录页面加载RootLayout/ScreenDefinition的中央租约，仅在开发世界装配。 */
    void RequestScenarioOverlay(UWorld& World);
    void OnScenarioOverlayReady(uint64 Generation, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    /** 平台打开请求按请求GUID核验；同步失败单独记录，防止受理返回值覆盖终态。 */
    UFUNCTION() void OnDemoScreenOpened(FGuid RequestId, FName ScreenId, UGamePlatformUIScreen* Screen);
    UFUNCTION() void OnDemoScreenFailed(FGuid RequestId, FName ScreenId, FText Reason);
};
