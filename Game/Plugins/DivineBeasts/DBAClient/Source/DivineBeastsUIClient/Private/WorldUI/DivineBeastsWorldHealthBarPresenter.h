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

/** 每个已观察实体的只读缓存及成对解绑句柄；弱引用不延长Actor/World生命周期。 */
struct FDivineBeastsWorldHealthBarBinding
{
    TWeakObjectPtr<UAbilitySystemComponent> ASC;
    TWeakObjectPtr<USceneComponent> TransformSource;
    FDelegateHandle HealthHandle, MaxHealthHandle, MonsterLifeHandle, TransformHandle;
    FGamePlatformWorldUIRequest Request;
};

/** 当前世界初始化只扫描一次；后续由Spawn/Destroy、生命、位移与最终镜头事件更新，无Tick。 */
UCLASS()
class UDivineBeastsWorldHealthBarPresenter : public UObject
{
    GENERATED_BODY()
public:
    /** 主线程启动本LocalPlayer的当前世界；同世界幂等，换世界先清所有旧注册与委托。 */
    void Initialize(ULocalPlayer* InPlayer, UWorld* InWorld, TSubclassOf<UGamePlatformWorldWidgetBase> InWidgetClass);
    void Clear();
    /** 显示偏好只影响当前玩家的展示；隐藏期间继续缓存真实生命事件，重新显示无需轮询。 */
    void SetVisible(bool bInVisible);
    /** 世界最终镜头通知调用，仅投影缓存值，不读取GAS或遍历世界实体。 */
    void RefreshProjection();
    bool IsVisible() const { return bVisible; }
    int32 GetObservedCount() const { return Bindings.Num(); }
    virtual void BeginDestroy() override;
private:
    void ObserveActor(AActor* Actor);
    void ApplyHealth(AActor* Actor, float Health, float Maximum);
    void ReadPlayerHealth(AActor* Actor);
    void Publish(AActor* Actor);
    void RemoveActor(AActor* Actor);
    UFUNCTION() void HandleActorDestroyed(AActor* Actor);
    /** 流送退出可能不Destroy；EndPlay同样解除名称板及所有观察句柄。 */
    UFUNCTION() void HandleActorEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    TWeakObjectPtr<ULocalPlayer> LocalPlayer;
    TWeakObjectPtr<UWorld> ObservedWorld;
    /** 世界内换Pawn时移除新自己，重新观察仍存在的旧Pawn；不再次扫描世界。 */
    TWeakObjectPtr<AActor> ExcludedLocalPawn;
    TWeakObjectPtr<UGamePlatformWorldUIService> Service;
    UPROPERTY(Transient) TSubclassOf<UGamePlatformWorldWidgetBase> WidgetClass;
    TMap<TWeakObjectPtr<AActor>, FDivineBeastsWorldHealthBarBinding> Bindings;
    FDelegateHandle SpawnHandle, CleanupHandle;
    /** 旧世界回调不能重新注册新世界名称板；每次Clear递增，不跨网络复制。 */
    uint64 ObservationGeneration = 0;
    bool bVisible = true;
};
