#pragma once

#include "CoreMinimal.h"
#include "Components/GamePlatformSlotWidget.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "DivineBeastsAbilityBarViewModel.generated.h"

class UDivineBeastsAbilityLoadoutComponent;
class UDivineBeastsAbilityUIProfile;
class UDivineBeastsCharacterComponent;
class UGamePlatformAbilitySystemComponent;
class UAbilitySystemComponent;
struct FGameplayTag;
struct FActiveGameplayEffectHandle;
struct FOnAttributeChangeData;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;
struct FGameplayAbilitySpec;
struct FGamePlatformAbilityAvatarBindingSnapshot;
struct FDivineBeastsAbilityLoadoutState;
struct FStreamableHandle;

/** FDivineBeastsAbilitySlotDetails（神兽联盟技能栏单格提示信息）。
 * 纯客户端只读投影，不提供技能授予、伤害或资源成本写入权限。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsAbilitySlotDetails
{
    GENERATED_BODY()

    /** 原有通用槽位身份；用于与 FGamePlatformUISlotState 一对一关联。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FName SlotId = NAME_None;
    /** 已由服务器授权的技能逻辑编号，并非纹理资源路径。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FName AbilityId = NAME_None;
    /** 服务端实际授权的技能等级。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    int32 AbilityLevel = 0;
    /** 仅从客户端 UI Profile（技能表现资产）读取的名称/描述。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FText Description;
    /** 已复制 GAS 冷却效果的剩余/总秒数；不是网络权威计时器。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    float CooldownRemainingSeconds = 0.0f;
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    float CooldownTotalSeconds = 0.0f;
    /** 当前 UI 不能交互的原因；服务端 GAS 仍决定最终是否激活。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FText DisabledReason;
};
DECLARE_MULTICAST_DELEGATE_OneParam(
    FDivineBeastsAbilityBarChangedNative, const TArray<FGamePlatformUISlotState>&);

/**
 * UDivineBeastsAbilityBarViewModel（生肖技能栏视图模型）。
 * 只订阅服务器 OwnerOnly（仅拥有者）技能快照，从项目客户端 UI 主资产异步补充图标；
 * 不直接 GiveAbility（授予技能），不依据客户端静态列表伪造可释放技能。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsAbilityBarViewModel final
    : public UDivineBeastsViewModelBase
{
    GENERATED_BODY()

public:
    /** 切换本地玩家 Pawn/ASC 时先解绑旧作用域；无有效组件返回失败。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    bool BindToLoadout(UDivineBeastsAbilityLoadoutComponent* InLoadout);

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    void UnbindFromLoadout();

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FGamePlatformUISlotState> GetSlots() const { return Slots; }

    /** 项目技能名称/等级/冷却文字投影，蓝图按 SlotId 与平台通用槽位匹配。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FDivineBeastsAbilitySlotDetails> GetSlotDetails() const { return SlotDetails; }

    const TArray<FGamePlatformUISlotState>& GetSlotsRef() const { return Slots; }
    FDivineBeastsAbilityBarChangedNative& OnSlotsChanged() { return SlotsChanged; }

    virtual void BeginDestroy() override;

private:
    void HandleLoadoutChanged(const FDivineBeastsAbilityLoadoutState& Snapshot);
    void HandleProfileLoaded(int32 ExpectedLoadGeneration, FName ExpectedHero);
    void RefreshSlots(const FDivineBeastsAbilityLoadoutState& Snapshot);
    void ResetProfileLease();

    /** 当 GAS 能力授予复制、技能效果、气势或阻断标签变化时更新单一快照；不使用 Widget Tick。 */
    void BindToNativeAbilityEvents();
    void UnbindFromNativeAbilityEvents();
    void HandleAbilitySpecListChanged();
    void HandleAbilityAvatarBindingChanged(const FGamePlatformAbilityAvatarBindingSnapshot& Snapshot);
    void HandleGameplayEffectAdded(UAbilitySystemComponent* Source,
        const FGameplayEffectSpec& Effect, FActiveGameplayEffectHandle Handle);
    void HandleGameplayEffectRemoved(const FActiveGameplayEffect& Effect);
    void HandleMomentumChanged(const FOnAttributeChangeData& ChangeData);
    void HandleCombatTagChanged(const FGameplayTag Tag, int32 NewCount);
    void HandleCharacterReadinessChanged(bool bReady);
    void RefreshCurrentLoadout();

    /** 只读检查：当前标签只映射到一个已复制原生 AbilitySpec，具体能否激活由 GAS 判断。 */
    static bool TryResolveGrantedSpec(UGamePlatformAbilitySystemComponent& ASC,
        FGameplayTag InputTag, const FGameplayAbilitySpec*& OutSpec);

    TWeakObjectPtr<UDivineBeastsAbilityLoadoutComponent> Loadout;
    FDelegateHandle LoadoutHandle;

    /** 保留当前本地 ASC 的弱引用；所有真实授权、冷却和成本仍归 GAS。 */
    TWeakObjectPtr<UGamePlatformAbilitySystemComponent> AbilitySystem;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> CharacterIdentity;
    FDelegateHandle CharacterReadinessHandle;
    FDelegateHandle AbilitySpecHandle;
    FDelegateHandle AbilityAvatarHandle;
    FDelegateHandle GameplayEffectAddedHandle;
    FDelegateHandle GameplayEffectRemovedHandle;
    FDelegateHandle MomentumHandle;
    FDelegateHandle StunTagHandle;
    FDelegateHandle SilenceTagHandle;
    FDelegateHandle DeadTagHandle;

    /** 强引用只在本次 UI 作用域有效，回调检查代次避免跨角色污染。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsAbilityUIProfile> LoadedProfile = nullptr;

    TSharedPtr<FStreamableHandle> ProfileHandle;
    int32 LoadGeneration = 0;
    FName LoadedHeroId = NAME_None;

    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> Slots;

    /** 与 Slots 按 SlotId 对应，不增加第二套玩法权威状态。 */
    UPROPERTY(Transient)
    TArray<FDivineBeastsAbilitySlotDetails> SlotDetails;

    FDivineBeastsAbilityBarChangedNative SlotsChanged;
};
