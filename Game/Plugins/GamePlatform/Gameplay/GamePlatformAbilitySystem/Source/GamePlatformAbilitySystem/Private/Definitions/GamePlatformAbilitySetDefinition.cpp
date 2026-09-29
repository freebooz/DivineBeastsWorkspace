#include "Definitions/GamePlatformAbilitySetDefinition.h"

/**
 * 纯字段校验：不加载软类、不访问世界、不产生GAS副作用。
 * 加载后的真实类型、网络策略、效果可回滚性继续由ASC授权阶段校验。
 */
FGamePlatformResult UGamePlatformAbilitySetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult Base = Super::ValidateDefinition();
    if (!Base.IsSuccess())
    {
        return Base;
    }

    constexpr int32 MaxEntriesPerKind = 64;
    if (Abilities.Num() > MaxEntriesPerKind ||
        Effects.Num() > MaxEntriesPerKind ||
        Attributes.Num() > MaxEntriesPerKind ||
        (Abilities.IsEmpty() && Effects.IsEmpty() && Attributes.IsEmpty()))
    {
        return FGamePlatformResult::Failure(
            TEXT("InvalidAbilitySetSize"),
            TEXT("AbilitySet至少包含一项能力/效果/属性，且每类条目最多64项。"));
    }

    TSet<FString> AbilityIds;
    TSet<FSoftObjectPath> AbilityClasses;
    TSet<FGameplayTag> InputTags;
    for (const FGamePlatformAbilityGrant& Grant : Abilities)
    {
        const FString AbilityId = Grant.AbilityId.ToString();
        if (!Grant.AbilityId.IsValid() || Grant.AbilityClass.IsNull() ||
            Grant.AbilityLevel < 1 || Grant.AbilityLevel > 100 ||
            AbilityIds.Contains(AbilityId) ||
            AbilityClasses.Contains(Grant.AbilityClass.ToSoftObjectPath()))
        {
            return FGamePlatformResult::Failure(
                TEXT("InvalidAbilityGrant"),
                TEXT("能力授权必须具有唯一合法逻辑ID、唯一非空能力类和1..100级别。"));
        }

        if (Grant.InputTag.IsValid())
        {
            const FString InputTagText = Grant.InputTag.ToString();
            if (!InputTagText.StartsWith(TEXT("Platform.Ability.Input.")) ||
                InputTags.Contains(Grant.InputTag))
            {
                return FGamePlatformResult::Failure(
                    TEXT("InvalidAbilityInputTag"),
                    TEXT("非空能力输入标签必须位于Platform.Ability.Input子命名空间且在集合内唯一。"));
            }
            InputTags.Add(Grant.InputTag);
        }

        AbilityIds.Add(AbilityId);
        AbilityClasses.Add(Grant.AbilityClass.ToSoftObjectPath());
    }

    TSet<FString> EffectIds;
    TSet<FSoftObjectPath> EffectClasses;
    for (const FGamePlatformEffectGrant& Grant : Effects)
    {
        const FString EffectId = Grant.EffectId.ToString();
        if (!Grant.EffectId.IsValid() || Grant.EffectClass.IsNull() ||
            !FMath::IsFinite(Grant.Level) || Grant.Level < 1.0f || Grant.Level > 100.0f ||
            EffectIds.Contains(EffectId) ||
            EffectClasses.Contains(Grant.EffectClass.ToSoftObjectPath()))
        {
            return FGamePlatformResult::Failure(
                TEXT("InvalidEffectGrant"),
                TEXT("效果授权必须具有唯一合法逻辑ID、唯一非空效果类和有限的1..100级别。"));
        }
        EffectIds.Add(EffectId);
        EffectClasses.Add(Grant.EffectClass.ToSoftObjectPath());
    }

    TSet<FSoftObjectPath> AttributeClasses;
    for (const FGamePlatformAttributeGrant& Grant : Attributes)
    {
        if (Grant.AttributeSetClass.IsNull() ||
            AttributeClasses.Contains(Grant.AttributeSetClass.ToSoftObjectPath()))
        {
            return FGamePlatformResult::Failure(
                TEXT("InvalidAttributeGrant"),
                TEXT("属性授权必须提供非空且在集合内唯一的属性集类。"));
        }
        AttributeClasses.Add(Grant.AttributeSetClass.ToSoftObjectPath());
    }

    return FGamePlatformResult::Success();
}
