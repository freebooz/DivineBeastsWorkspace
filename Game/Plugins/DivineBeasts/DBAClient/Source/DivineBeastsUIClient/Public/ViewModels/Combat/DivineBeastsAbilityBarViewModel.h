#pragma once

#include "CoreMinimal.h"
#include "Components/GamePlatformSlotWidget.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "DivineBeastsAbilityBarViewModel.generated.h"

class UDivineBeastsAbilityLoadoutComponent;
class UDivineBeastsAbilityUIProfile;
class UDivineBeastsCharacterComponent;
class UGamePlatformAbilitySystemComponent;
class UAbilitySystemComponent;
struct FOnAttributeChangeData;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;
struct FGameplayAbilitySpec;
struct FGamePlatformAbilityAvatarBindingSnapshot;
struct FDivineBeastsAbilityLoadoutState;
struct FStreamableHandle;

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

    FDivineBeastsAbilityBarChangedNative SlotsChanged;
};
