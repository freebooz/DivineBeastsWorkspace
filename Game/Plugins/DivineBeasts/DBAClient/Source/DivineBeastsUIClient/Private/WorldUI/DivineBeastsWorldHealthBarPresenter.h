// 项目LocalPlayer头顶血条事件适配；仅拥有显示注册和观察句柄，不拥有玩法生命或角色。
#pragma once
#include "CoreMinimal.h"
#include "Requests/GamePlatformWorldUIRequest.h"
#include "UObject/Object.h"
#include "GameFramework/Actor.h"
#include "DivineBeastsWorldHealthBarPresenter.generated.h"
class ULocalPlayer;
class UAbilitySystemComponent;
class USceneComponent;
class UGamePlatformWorldUIService;
class UGamePlatformWorldWidgetBase;
class ADivineBeastsWorldPlayerController;
class UDivineBeastsCharacterComponent;
class AGamePlatformPlayerStateBase;
class UDecalComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UGamePlatformCombatComponent;

/** 每个已观察实体的只读缓存及成对解绑句柄；弱引用不延长Actor/World生命周期。 */
struct FDivineBeastsWorldHealthBarBinding
{
    TWeakObjectPtr<UAbilitySystemComponent> ASC;
    TWeakObjectPtr<USceneComponent> TransformSource;
    FDelegateHandle HealthHandle, MaxHealthHandle, TransformHandle; // GAS双属性通知与世界坐标回调成对解绑。
    TWeakObjectPtr<UDivineBeastsCharacterComponent> Identity;
    TWeakObjectPtr<AGamePlatformPlayerStateBase> PlayerState;
    FDelegateHandle IdentityHandle, PlayerStateHandle; // 身份就绪/公开准入变化驱动关系与化身失效检查。
    TWeakObjectPtr<UGamePlatformCombatComponent> Combat;
    FDelegateHandle CombatObservationHandle; // 可靠复制死亡/化身通知，不能依赖可丢包VFX反馈。
    FGamePlatformWorldUIRequest Request;
    bool bSelectionOnly = false; // 无生命条的交互目标只在选中期间观察，切换后立即注销并释放。
};

/** 当前世界初始化只扫描一次；后续由Spawn/Destroy、生命、位移与最终镜头事件更新，无Tick。 */
UCLASS()
class UDivineBeastsWorldHealthBarPresenter : public UObject
{
    GENERATED_BODY()
public:
    /** 主线程启动本LocalPlayer的当前世界；同世界幂等，换世界先清所有旧注册与委托。 */
    void Initialize(ULocalPlayer* InPlayer, UWorld* InWorld, TSubclassOf<UGamePlatformWorldWidgetBase> InWidgetClass,
        UMaterialInterface* InSelectionRingMaterial = nullptr);
    void Clear();
    /** 显示偏好只影响当前玩家的展示；隐藏期间继续缓存真实生命事件，重新显示无需轮询。 */
    void SetVisible(bool bInVisible);
    /** 世界最终镜头通知调用，仅投影缓存值，不读取GAS或遍历世界实体。 */
    void RefreshProjection();
    bool IsVisible() const { return bVisible; }
    int32 GetObservedCount() const { return Bindings.Num(); }
    virtual void BeginDestroy() override;
private:
    // 隔离自动化只注入观察作用域，验证真实Actor通知和换Pawn；不伪装屏幕注册或后端准入。
    friend class FDivineBeastsWorldHealthBarLifecycleTest;
    friend class FDivineBeastsPlayerNameplateEventsTest;
    friend class FDivineBeastsPlayerNameplateBudgetTest;
    friend class FDivineBeastsWorldTargetSelectionTest;
    void ObserveActor(AActor* Actor, bool bSelectionOnly = false);
    void ApplyHealth(AActor* Actor, float Health, float Maximum);
    void ReadPlayerHealth(AActor* Actor);
    void Publish(AActor* Actor);
    void RemoveActor(AActor* Actor);
    /** 本地控制器是唯一选择源；先订阅再读一次当前目标，重复同来源不会重复订阅。 */
    void BindSelectionController(ADivineBeastsWorldPlayerController* Controller);
    void RefreshSelectedTarget();
    /** 只更改本Presenter显示缓存；死亡撤销不能清除Gameplay保留的失效治疗意图。 */
    void SetSelectedTarget(AActor* Actor);
    void BindPlayerState(AActor* Actor);
    void RefreshRelations();
    FLinearColor ResolveSelectionTint(AActor* Actor) const;
    /** 持续世界UI贴花，不创建Niagara、播放器、计时器或资产加载器；只在真实变换事件更新位置。 */
    void RefreshSelectionRing();
    void ClearSelectionRing();
    UFUNCTION() void HandleActorDestroyed(AActor* Actor);
    /** 流送退出可能不Destroy；EndPlay同样解除名称板及所有观察句柄。 */
    UFUNCTION() void HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    TWeakObjectPtr<ULocalPlayer> LocalPlayer;
    TWeakObjectPtr<UWorld> ObservedWorld;
    /** 世界内换Pawn时移除新自己，重新观察仍存在的旧Pawn；不再次扫描世界。 */
    TWeakObjectPtr<AActor> ExcludedLocalPawn;
    TWeakObjectPtr<UGamePlatformWorldUIService> Service;
    TWeakObjectPtr<ADivineBeastsWorldPlayerController> SelectionController;
    TWeakObjectPtr<AActor> SelectedTarget;
    int32 SelectedAvatarGeneration = 0; // 旧Actor同地址的新化身不能继承旧UI选择。
    uint64 SelectionRevision = 0; // 同一世界同步选择回调重入时，后继选择拥有显示，旧栈不得恢复。
    FDelegateHandle SelectionHandle, WorldFactsHandle;
    /** 组合根持有真实材质引用；此处借用，平台代码没有项目路径或同步Load。 */
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> SelectionRingMaterial;
    UPROPERTY(Transient) TObjectPtr<UDecalComponent> SelectionRing;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SelectionRingInstance;
    UPROPERTY(Transient) TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass;
    TMap<TWeakObjectPtr<AActor>, FDivineBeastsWorldHealthBarBinding> Bindings;
    FDelegateHandle SpawnHandle, CleanupHandle, RootReadyHandle;
    /** 旧世界回调不能重新注册新世界名称板；每次Clear递增，不跨网络复制。 */
    uint64 ObservationGeneration = 0;
    bool bVisible = true;
};
