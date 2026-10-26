// 项目客户端开发施法装配；World独占动画/Data租约，订阅事实，不计算技能或伤害。
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Types/GamePlatformDataLease.h"
#include "ContentPacks/DivineBeastsPresentationContentPack.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"
#include "DivineBeastsDevelopmentCastingWorldSubsystem.generated.h"
class ACharacter;
class UAnimInstance;
class UAnimMontage;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class UGamePlatformAbilityCastComponent;
class UDivineBeastsCharacterComponent;
class UDivineBeastsPresentationClientSubsystem;
class UGamePlatformPresentationClientSubsystem;
struct FGamePlatformAbilityCastFacts;

/** 私有世界装配服务；不对外形成第二套表现接口，专服/Commandlet/非开发配置不创建。 */
UCLASS()
class UDivineBeastsDevelopmentCastingWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& World) override;
    virtual void Deinitialize() override;
private:
    // Editor原生回归仅构造受控初始状态并观察自有资源；不增加生产入口或改变施法行为。
    friend struct FDivineBeastsDevelopmentCastingTestFixture;
    /** 单掌纯表现所有权；提交中重入的取消由该Submit栈收尾，不跨回调保留容器引用。 */
    struct FPalmVFXBinding
    {
        FGuid RequestId;
        TWeakObjectPtr<USceneComponent> Anchor;
        TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> Adapter;
        int32 WorldGeneration=0;
        bool bSubmitting=false;
    };
    /** 每角色独立动作账本；停止只针对自己实际播放的Montage和Request，不能清别人的动画。 */
    struct FBinding
    {
        FDelegateHandle FactsHandle, IdentityHandle, ReadinessHandle;
        TWeakObjectPtr<UDivineBeastsCharacterComponent> Identity;
        TWeakObjectPtr<USkeletalMeshComponent> Mesh;
        /** OperationId区分同一事实窗口内的外观重建；任何外调后必须重新查表核验，不能借用TMap地址。 */
        FGuid ActivationId, OperationId, VFXRequestId;
        FName StyleName=NAME_None; // 取消保持原语义，不能误用另一种法术定义。
        TWeakObjectPtr<UAnimInstance> Anim;
        TWeakObjectPtr<UAnimMontage> Montage;
        TWeakObjectPtr<USkeletalMesh> SkeletalMesh;
        TWeakObjectPtr<USceneComponent> CastAnchor;
        /** 只用于数字1定向飞弹的左右掌；其余技能保留原单锚点/地面表现。 */
        FPalmVFXBinding Palms[2];
        /** 保存实际请求的作用域和世界代次；Travel后的清理不能借用新世界代次。 */
        TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> VFXAdapter;
        int32 VFXWorldGeneration=0;
        /** Play前后公开ID差集中唯一新增匹配实例的身份；无结果/多结果保持INDEX_NONE，绝不认领外部重播。 */
        int32 MontageInstanceId=INDEX_NONE;
        /** 同步启动/提交期间防止重复受理；启动中取消先撤非动画资源，动画清理延后到本Play返回栈。 */
        bool bStartingMontage=false, bSubmittingVFX=false;
        FTimerHandle StopTimer;
        /** 声音独立于动画/VFX资源就绪；同一施法只起播一次，取消和剩余窗口结束均收敛。 */
        FGuid AudioActivationId, AudioRequestId;
        FGameplayTag AudioSemantic;
        TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> AudioAdapter;
        int32 AudioWorldGeneration=0;
        bool bSubmittingAudio=false, bAudioAccepted=false;
        FTimerHandle AudioStopTimer;
    };
    TMap<TWeakObjectPtr<UGamePlatformAbilityCastComponent>,FBinding> Bindings;
    FGamePlatformDataLease VisualLease;
    FDelegateHandle SpawnHandle, PackHandle;
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> Presentation;
    TWeakObjectPtr<UGamePlatformPresentationClientSubsystem> PlatformPresentation;
    FGuid LifetimeToken;
    bool bReady=false, bClosing=false;
    /** 只监听本World的角色默认子组件；未知Actor不制造客户端权威实体。 */
    void ObserveActor(AActor* Actor);
    /** 只接收组件当前值快照；旧广播与已过期窗口不清理或复活后继动作。 */
    void HandleFacts(UGamePlatformAbilityCastComponent* Component,const FGamePlatformAbilityCastFacts& Facts);
    /** 复用现有请求服务，分别附着真实左右掌，传递只读施法窗口；不生成权威动作。 */
    void UpdateDirectedMissilePalms(UGamePlatformAbilityCastComponent* Component,
        const FGamePlatformAbilityCastFacts& Facts,const FGuid& OperationId,const FGuid& Token);
    /** GT只读身份/Combat和服务器时钟，返回有效窗口的已过/剩余秒数；不写权威状态。 */
    bool GetCastWindow(UGamePlatformAbilityCastComponent* Component,const FGamePlatformAbilityCastFacts& Facts,
        double& ElapsedSeconds,double& RemainingSeconds) const;
    /** 同步外调后的完整复核：世界生命周期、当前事实、操作身份和当前外观实例均须一致。 */
    bool IsCastOperationCurrent(UGamePlatformAbilityCastComponent* Component,const FGamePlatformAbilityCastFacts& Facts,
        const FGuid& OperationId,const FGuid& Token,UAnimInstance* Anim) const;
    /** 值账本先撤销再外调，取消/重入/旧回调均不能停止后来窗口。 */
    void StopCast(UGamePlatformAbilityCastComponent* Component,FGuid ExpectedOperationId=FGuid());
    /** 只释放已摘除的值快照；不在外部回调返回后访问原容器元素。 */
    void ReleaseCastResources(const FBinding& Binding);
    /** 消费同一服务器施法事实，不从弹体发射推断起手时机，不依赖可选动画或VFX。 */
    void UpdateCastAudio(UGamePlatformAbilityCastComponent* Component,
        const FGamePlatformAbilityCastFacts& Facts, double RemainingSeconds);
    void StopCastAudio(UGamePlatformAbilityCastComponent* Component, FGuid ExpectedActivation=FGuid());
    void ReleaseCastAudio(const FBinding& Binding);
    /** EndPlay/Deinitialize在任何停止/取消外调前，先解绑自己拥有的所有订阅。 */
    void DetachObservers(UGamePlatformAbilityCastComponent* Component,const FBinding& Binding);
    void RefreshFacts();
    /** 原生动画初始化只重评估仍有效事实；不轮询、不补播过期窗口。 */
    UFUNCTION() void HandleAnimInitialized();
    void HandlePack(const FDivineBeastsPresentationContentPackHandle& Handle,
        EDivineBeastsPresentationContentPackState State,const FString& Error);
    UFUNCTION() void HandleActorEndPlay(AActor* Actor,EEndPlayReason::Type Reason);
};
