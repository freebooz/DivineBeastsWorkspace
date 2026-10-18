#pragma once

#include "CoreMinimal.h"
#include "Components/GamePlatformSlotWidget.h"
#include "ViewModels/DivineBeastsViewModelBase.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformResult.h"
#include "DivineBeastsAbilityBarViewModel.generated.h"

class UDivineBeastsAbilityLoadoutComponent;
class UDivineBeastsAbilityUIProfile;
class UDivineBeastsCharacterComponent;
class UGamePlatformAbilitySystemComponent;
class AGamePlatformPlayerStateBase;
class UGamePlatformGameplayEligibilityComponent;
struct FGamePlatformGameplayEligibilitySnapshot;
class UAbilitySystemComponent;
struct FGameplayTag;
struct FActiveGameplayEffectHandle;
struct FOnAttributeChangeData;
struct FActiveGameplayEffect;
struct FGameplayEffectSpec;
struct FGameplayAbilitySpec;
struct FGamePlatformAbilityAvatarBindingSnapshot;
struct FDivineBeastsAbilityLoadoutState;
struct FDivineBeastsGrantedAbilitySlot;
class UGameInstance;
class IGamePlatformDataService;

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
    /** 在GAS事件快照时计算的本进程Slate实时时钟截止秒；仅用于材质动画，不是网络权威。 */
    UPROPERTY(BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    float CooldownEndsAtAppSeconds = 0.0f;
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

    /** 向当前本地受控Pawn提交一次已授权槽位的按下/释放输入，复用平台ASC与原生GAS远程激活。
     * SlotId必须属于当前Ready快照及已确认Spec；被动、旧Avatar、禁用和待确认返回中文失败结果。
     * 成功仅表示输入请求受理；成本、冷却、投射物与伤害仍等待服务器GAS确认，无客户端授予或扣费。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    FGamePlatformResult RequestActivateSlot(FName SlotId);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FGamePlatformUISlotState> GetSlots() const { return Slots; }

    /** 项目技能名称/等级/冷却文字投影，蓝图按 SlotId 与平台通用槽位匹配。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FDivineBeastsAbilitySlotDetails> GetSlotDetails() const { return SlotDetails; }

    const TArray<FGamePlatformUISlotState>& GetSlotsRef() const { return Slots; }
    FDivineBeastsAbilityBarChangedNative& OnSlotsChanged() { return SlotsChanged; }

    virtual void BeginDestroy() override;

private:
    // 仅自动化夹具可植入旧的只读显示快照，以验证真实资格事件刷新；不开放运行授权写接口。
    friend class FDivineBeastsAbilityActivationEventsTest;
    friend class FDivineBeastsAbilityDetailsProjectionTest;
    friend class FDivineBeastsAbilityGrantIdentityTest;
    /** 显示与命令共享精确槽位的唯一性约束；重复授权一律拒绝，不按快照顺序挑选。 */
    static const FDivineBeastsGrantedAbilitySlot* FindUniqueGrantedSlot(
        const FDivineBeastsAbilityLoadoutState& Snapshot, FName SlotId);
    void HandleLoadoutChanged(const FDivineBeastsAbilityLoadoutState& Snapshot);
    /** Data只在游戏线程延后完成；代次、英雄、原Loadout与完整租约必须仍属当前请求。 */
    void HandleProfileLoaded(uint64 ExpectedLoadGeneration, FName ExpectedHero,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    void HandleIconsLoaded(uint64 ExpectedLoadGeneration, FName ExpectedHero,
        const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result);
    /** 仅解析当前Loadout所属实例的数据门面；不创建资产管理器或全局缓存。 */
    IGamePlatformDataService* ResolveProfileDataService(UGameInstance* Instance) const;
#if WITH_DEV_AUTOMATION_TESTS
    /** 私有测试注入只控制Data延后通知和主资产查询，不替换生产状态机或游戏资格判断。 */
    friend struct FDivineBeastsUIResourceLeaseTestAccess;
    IGamePlatformDataService* DataServiceForTests = nullptr;
    TFunction<FSoftObjectPath(FName)> ProfilePathForTests;
#endif
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
    /** 游戏线程按当前Pawn重绑定两种玩法资格通知，来源变化先撤旧订阅；只读取复制事实，不设置Active。 */
    void RefreshGameplayEligibilityBindings();
    /** 通知仅作为显示失效信号；重新核验当前授权，不把事件参数当服务器授权结果。 */
    void HandleGameplayEligibilityChanged(const FGamePlatformGameplayEligibilitySnapshot& Snapshot);

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
    /** 当前角色作用域的资格事件源及句柄；换Pawn、解绑、销毁均配对撤销，禁止跨角色残留。 */
    TWeakObjectPtr<AGamePlatformPlayerStateBase> LifecycleState;
    TWeakObjectPtr<UGamePlatformGameplayEligibilityComponent> GameplayEligibility;
    FDelegateHandle LifecycleHandle;
    FDelegateHandle GameplayEligibilityHandle;

    /** 强引用只在本次 UI 作用域有效，回调检查代次避免跨角色污染。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsAbilityUIProfile> LoadedProfile = nullptr;

    /** 两阶段普通资源租约：Profile不是平台Definition，图标批次共享当前英雄代次。 */
    FGamePlatformDataLease ProfileLease;
    FGamePlatformDataLease IconLease;
    TWeakObjectPtr<UGameInstance> ProfileLeaseInstance;
    FSoftObjectPath ProfilePath;
    uint64 LoadGeneration = 0;
    FName LoadedHeroId = NAME_None;
    /** 仅成功图标租约允许向槽位发布软图标，失败保持中性缺图标视觉。 */
    bool bIconsReady = false;

    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> Slots;

    /** 与 Slots 按 SlotId 对应，不增加第二套玩法权威状态。 */
    UPROPERTY(Transient)
    TArray<FDivineBeastsAbilitySlotDetails> SlotDetails;

    FDivineBeastsAbilityBarChangedNative SlotsChanged;
};
