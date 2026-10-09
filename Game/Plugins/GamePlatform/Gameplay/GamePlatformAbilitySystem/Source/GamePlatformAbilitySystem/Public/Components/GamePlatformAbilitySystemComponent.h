#pragma once

#include "AbilitySystemComponent.h"
#include "Interfaces/IGamePlatformAbilityInputReceiver.h"
#include "Interfaces/IGamePlatformAbilityActivationGate.h"
#include "GamePlatformAbilitySystemComponent.generated.h"

USTRUCT(BlueprintType)
struct GAMEPLATFORMABILITYSYSTEM_API FGamePlatformAbilityAvatarBindingSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 AvatarGeneration = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bBound = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformAbilityAvatarBindingChangedNative,
    const FGamePlatformAbilityAvatarBindingSnapshot&);

/** FGamePlatformAbilitySpecListChangedNative（本地技能授权列表复制完成事件）。
 * 仅作为客户端/UI 可观察事实，不授权新技能；与服务器原生 GiveAbility 相互独立。 */
DECLARE_MULTICAST_DELEGATE(FGamePlatformAbilitySpecListChangedNative);

class AActor;

/** 平台级 ASC 基类；不包含具体游戏技能、伤害公式或输入绑定。 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMABILITYSYSTEM_API UGamePlatformAbilitySystemComponent
    : public UAbilitySystemComponent
    , public IGamePlatformAbilityInputReceiver
{
    GENERATED_BODY()

public:
    UGamePlatformAbilitySystemComponent();

    UFUNCTION(BlueprintCallable, Category="GamePlatform|AbilitySystem")
    bool BindAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

    UFUNCTION(BlueprintCallable, Category="GamePlatform|AbilitySystem")
    void ClearAbilityAvatar();

    /**
     * 项目组合根在ActorInfo就绪后注入当前Avatar的资格读取器；仅游戏线程，Owner须存活并属同一世界。
     * 不接纳空Avatar、重复其他所有者或查询重入；换Avatar自动撤销，新绑定必须重新注入。
     * Gate只读真实玩法事实，不发网络请求；ASC仅持该读取器和Owner弱引用，不拥有玩法状态。
     */
    FGamePlatformResult SetActivationGate(TWeakObjectPtr<UObject> Owner,
        TSharedRef<IGamePlatformAbilityActivationGate> Gate);
    /** 只允许原注入所有者撤销；重复撤销返回false，不影响其他实例。仅游戏线程。 */
    bool ClearActivationGate(TWeakObjectPtr<UObject> Owner);
    /** GAS实际激活前同步查询；缺失/过期/未Active都返回失败，客户端成功只允许预测尝试。 */
    FGamePlatformResult EvaluateActivationEligibility() const;

    FGamePlatformAbilityAvatarBindingSnapshot GetAvatarBindingSnapshot() const
    {
        FGamePlatformAbilityAvatarBindingSnapshot Result;
        Result.AvatarGeneration = AvatarGeneration;
        Result.bBound = AbilityActorInfo.IsValid() && AbilityActorInfo->AvatarActor.IsValid();
        return Result;
    }

    FGamePlatformAbilityAvatarBindingChangedNative& OnAvatarBindingChanged()
    {
        return AvatarBindingChanged;
    }

    /** 订阅当前 ASC 原生授权容器复制完成；跨本地玩家/Avatar 切换必须由调用方解绑。 */
    FGamePlatformAbilitySpecListChangedNative& OnAbilitySpecListChanged()
    {
        return AbilitySpecListChanged;
    }

    // IGamePlatformAbilityInputReceiver（平台能力输入接收器）
    virtual FGamePlatformAbilityInputToken GetInputToken() const override;
    virtual FGamePlatformResult AbilityInputPressed(
        FGameplayTag Tag,
        const FGamePlatformAbilityInputToken& Token) override;
    virtual FGamePlatformResult AbilityInputReleased(
        FGameplayTag Tag,
        const FGamePlatformAbilityInputToken& Token) override;
    virtual void ProcessAbilityInput() override;
    virtual void ClearAbilityInput() override;

protected:
    /** 引擎对客户端新增/删除 AbilitySpec 完成复制后再广播，避免早于真实 GAS 状态。 */
    virtual void OnRep_ActivateAbilities() override;

private:
    void BroadcastAvatarBinding();
    /** 核对本地拥有者和当前Avatar代次；输入令牌只用于拒绝旧接收器，不构成服务器授权。 */
    bool IsInputTokenCurrent(const FGamePlatformAbilityInputToken& Token) const;
    /** 精确按InputTag查找一个技能Spec；重复标签Fail Closed，避免加载顺序决定输入目标。 */
    FGameplayAbilitySpec* FindAbilitySpecByInputTag(FGameplayTag Tag, FGamePlatformResult& OutResult);
    /** 清理已不存在的Spec句柄；只扫描本组件的小型输入缓存，不访问外部注册表。 */
    void CompactInputHandles();

    /** 每个ASC实例独立输入作用域；不会复制到网络，也不能作为认证身份。 */
    FGuid InputScopeId;
    /** 新按下和持续按住的技能句柄；不保存键盘键或输入设备信息。 */
    TArray<FGameplayAbilitySpecHandle> PressedInputHandles;
    TArray<FGameplayAbilitySpecHandle> HeldInputHandles;

    int32 AvatarGeneration = 0;
    /** 非反射只读适配器；弱Owner和当前Avatar代次共同界定注入生命周期。 */
    TWeakObjectPtr<UObject> ActivationGateOwner;
    TSharedPtr<IGamePlatformAbilityActivationGate> ActivationGate;
    int32 ActivationGateGeneration = 0;
    mutable bool bEvaluatingActivationGate = false;
    FGamePlatformAbilityAvatarBindingChangedNative AvatarBindingChanged;
    /** 原生AbilitySpec复制完成事实；仅供订阅方同步只读投影，不改变Gate或权威技能授权。 */
    FGamePlatformAbilitySpecListChangedNative AbilitySpecListChanged;
};
