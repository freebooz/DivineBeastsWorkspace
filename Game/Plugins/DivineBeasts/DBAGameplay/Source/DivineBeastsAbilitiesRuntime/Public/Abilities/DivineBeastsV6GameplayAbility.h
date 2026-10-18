// SSLM V6项目层玩法能力：在已有GAS及Combat平台接口上实现安全的基础技能原语。
#pragma once

#include "CoreMinimal.h"
#include "Abilities/DivineBeastsConfiguredGameplayAbility.h"
#include "Types/GamePlatformCombatProjectileMotion.h"
#include "DivineBeastsV6GameplayAbility.generated.h"

class UDivineBeastsAbilityDefinition;
struct FDivineBeastsEightSlotDraftAction;
class UGamePlatformCombatComponent;

/**
 * EV6NativeExecutionMode（V6原生可执行技能模式）。
 * 只能代表已有平台提供确定性结算的原语，未实现复杂技能一律Unsupported。
 * 单次作用和一次GAS Commit保持一一对应，不用蓝图外观绕过伤害/治疗授权。
 */
UENUM(BlueprintType)
enum class EDivineBeastsV6NativeMode : uint8
{
    Unsupported,
    ForwardDamage,
    SingleProjectile,
    RadialDamage,
    SingleHealing,
    SingleShield,
    SingleStun,
    SingleSilence,
    SafeDash,
    /** 新模式只追加在已发布的8种原语之后，不改变旧枚举序列化值。 */
    DamageAndStun,
    DamageAndSilence,
    RadialDamageAndStun
};

/**
 * UDivineBeastsV6GameplayAbility（SSLM V6原生数据驱动技能执行基类）。
 * 由每个英雄的独立GA蓝图子类绑定唯一AbilityDefinitionId，AbilitySet只授予真实已验收类；
 * 不创建第二个ASC、伤害执行器、客户端RPC或Niagara播放器。
 *
 * 当前支持：服务器单体、投射物、范围伤害、友方治疗、限时护盾、眩晕、沉默以及
 * 单体伤害加眩晕／沉默、范围伤害加眩晕三种单段复合技能（原生动作需后续验收）。
 * 普攻命中才可发放气势。普通短位移由导航与胶囊扫掠验证；减速、破甲、毒路、
 * 假身、实体门盾、揭隐、免死、多段及特殊位移仍需单独权威执行审核。
 */
UCLASS(Blueprintable)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsV6GameplayAbility
    : public UDivineBeastsConfiguredGameplayAbility
{
    GENERATED_BODY()
public:
    UDivineBeastsV6GameplayAbility();
    /** 纯数据合同，便于UE原生自动化正反例；不会授予技能或修改世界。 */
    static bool IsValidDamageControlAction(
        const FDivineBeastsEightSlotDraftAction& ControlAction, bool bSilence);

    /** 编辑器显式设置的V6执行模式；不依据技能名称或可选视觉效果猜测行为。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|V6")
    EDivineBeastsV6NativeMode ExecutionMode = EDivineBeastsV6NativeMode::Unsupported;

    /** 只有SingleProjectile模式使用：真实平台权威投射物运动；客户端不据此直接扣血。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|V6")
    FGamePlatformCombatProjectileMotion ProjectileMotion;

    /** 原生GAS冷却Tag来源；持续时长仍由已授权的GameplayEffect处理。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|V6")
    FGameplayTagContainer ApprovedCooldownTags;

    /** 授予阶段预校验：任何额外行为、不支持目标模式、段数与数值不一致均拒绝。 */
    bool ValidateNativeContract(const UDivineBeastsAbilityDefinition& Definition,
        const FDivineBeastsAbilityBalanceRow& Balance, FString& OutError) const;

    virtual const FGameplayTagContainer* GetCooldownTags() const override;

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        const FGameplayEventData* TriggerEventData) override;

    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
        const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,
        bool bReplicateEndAbility, bool bWasCancelled) override;

private:
    /** 激活唯一身份；提交GE/战斗效果同步触发死亡/换Pawn时必须立即失效。 */
    FGuid ActivationToken;

    bool IsCurrentActivation(const FGuid& ExpectedToken, const AActor* ExpectedAvatar,
        const UGamePlatformCombatComponent* ExpectedCombat, int32 ExpectedGeneration) const;
};
