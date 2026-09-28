#include "Input/DivineBeastsInputSemantics.h"

#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(DBInputAttackPrimary, "DivineBeasts.Input.Combat.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBInputAbility1, "DivineBeasts.Input.Ability.Slot1");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBInputAbility2, "DivineBeasts.Input.Ability.Slot2");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBInputAbility3, "DivineBeasts.Input.Ability.Slot3");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBInputAbility4, "DivineBeasts.Input.Ability.Slot4");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBInputTargetLock, "DivineBeasts.Input.Target.Lock");

// AbilitySystem要求InputTag位于Platform.Ability.Input根下；项目只拥有自己的DivineBeasts子命名空间。
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBAbilityInputPrimary, "Platform.Ability.Input.DivineBeasts.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBAbilityInput1, "Platform.Ability.Input.DivineBeasts.Slot1");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBAbilityInput2, "Platform.Ability.Input.DivineBeasts.Slot2");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBAbilityInput3, "Platform.Ability.Input.DivineBeasts.Slot3");
UE_DEFINE_GAMEPLAY_TAG_STATIC(DBAbilityInput4, "Platform.Ability.Input.DivineBeasts.Slot4");

FGameplayTag DivineBeastsInputSemantics::AttackPrimary() { return DBInputAttackPrimary; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot1() { return DBInputAbility1; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot2() { return DBInputAbility2; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot3() { return DBInputAbility3; }
FGameplayTag DivineBeastsInputSemantics::AbilitySlot4() { return DBInputAbility4; }
FGameplayTag DivineBeastsInputSemantics::TargetLock() { return DBInputTargetLock; }

FGameplayTag DivineBeastsInputSemantics::ToAbilityInputTag(FGameplayTag SemanticTag)
{
    if (SemanticTag == DBInputAttackPrimary) { return DBAbilityInputPrimary; }
    if (SemanticTag == DBInputAbility1) { return DBAbilityInput1; }
    if (SemanticTag == DBInputAbility2) { return DBAbilityInput2; }
    if (SemanticTag == DBInputAbility3) { return DBAbilityInput3; }
    if (SemanticTag == DBInputAbility4) { return DBAbilityInput4; }
    // TargetLock属于目标/角色控制语义，不伪装成GAS技能输入。
    return FGameplayTag();
}

FGamePlatformInputSemanticDescriptor DivineBeastsInputSemantics::MakeGameplayActionDescriptor(
    FGameplayTag SemanticTag)
{
    FGamePlatformInputSemanticDescriptor Result;
    Result.SemanticId.Tag = SemanticTag;
    Result.Unit = EGamePlatformInputUnit::Boolean;
    Result.ValueType = EInputActionValueType::Boolean;
    Result.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Actions);
    Result.ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
    return Result;
}

bool DivineBeastsInputSemantics::IsCoreGameplaySemantic(FGameplayTag SemanticTag)
{
    return SemanticTag == DBInputAttackPrimary ||
        SemanticTag == DBInputAbility1 ||
        SemanticTag == DBInputAbility2 ||
        SemanticTag == DBInputAbility3 ||
        SemanticTag == DBInputAbility4 ||
        SemanticTag == DBInputTargetLock;
}
