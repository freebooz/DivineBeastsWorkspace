#pragma once

#include "AbilitySystemComponent.h"
#include "Interfaces/IGamePlatformAbilityInputReceiver.h"
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
    FGamePlatformAbilityAvatarBindingChangedNative AvatarBindingChanged;    FGamePlatformAbilitySpecListChangedNative AbilitySpecListChanged;
};
