// 项目层开发授权策略回归：纯值输入覆盖正常、未启用、发行构建和跨英雄映射；不伪造真实资产授予。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
#include "Abilities/DivineBeastsDevelopmentGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Effects/GamePlatformCombatGameplayEffects.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentAbilityPolicyTest,
    "DivineBeasts.Abilities.DevelopmentOverlayPolicy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsDevelopmentAbilityPolicyTest::RunTest(const FString&)
{
    using namespace DivineBeasts::DevelopmentAbilities;
    const FName Formal(TEXT("dba.abilityset.rat_formal@1"));
    for (int32 Mask = 0; Mask < 8; ++Mask)
    {
        TestEqual(TEXT("编译资格、配置与命令行必须同时满足"),
            IsEnabled((Mask & 1) != 0, (Mask & 2) != 0, (Mask & 4) != 0), Mask == 7);
    }
    TestEqual(TEXT("关闭开发覆盖保留正式授权"),
        ResolveSetId(TEXT("Hero.Zodiac.Rat"), Formal, false, TEXT("dba.abilityset.rat_dev@1")), Formal);
    TestEqual(TEXT("跨英雄映射不得替换正式授权"),
        ResolveSetId(TEXT("Hero.Zodiac.Rat"), Formal, true, TEXT("dba.abilityset.ox_dev@1")), Formal);
    TestEqual(TEXT("非法英雄和任意集合不得提升权限"),
        ResolveSetId(TEXT("Hero.Zodiac.Invalid"), NAME_None, true, TEXT("dba.abilityset.invalid_dev@1")), NAME_None);
    TestEqual(TEXT("空映射不虚构技能集合"),
        ResolveSetId(TEXT("Hero.Zodiac.Rat"), NAME_None, true, TEXT("")), NAME_None);
    for (FName HeroId : FDivineBeastsHeroCatalog::GetCoreHeroIds())
    {
        const FString Suffix = HeroId.ToString().RightChop(12).ToLower();
        const FString DevelopmentId = FString(TEXT("dba.abilityset.")) + Suffix + TEXT("_dev@1");
        TestEqual(TEXT("十二生肖各自开发映射可替代空正式集合"),
            ResolveSetId(HeroId, NAME_None, true, DevelopmentId), FName(*DevelopmentId));
    }
    return true;
}
// 使用真实保存的开发能力CDO与平台眩晕/沉默GE检查GAS阻断；不伪造命中或消耗。
// 沉默必须保留普攻，眩晕阻断普攻和施法；移除真实效果后释放阻断计数。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentControlEffectsTest,
    "DivineBeasts.Abilities.DevelopmentControlEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentControlEffectsTest::RunTest(const FString&)
{
    auto* BasicClass = LoadClass<UDivineBeastsDevelopmentGameplayAbility>(nullptr,
        TEXT("/Game/Development/DivineBeasts/Abilities/GA_DBA_Horse_DevBasicAttack.GA_DBA_Horse_DevBasicAttack_C"));
    auto* SpellClass = LoadClass<UDivineBeastsDevelopmentGameplayAbility>(nullptr,
        TEXT("/Game/Development/DivineBeasts/Abilities/GA_DBA_Horse_DevActive01.GA_DBA_Horse_DevActive01_C"));
    if (!TestNotNull(TEXT("真实普攻类"), BasicClass) || !TestNotNull(TEXT("真实主动类"), SpellClass)) return false;
    const auto* Basic = BasicClass->GetDefaultObject<UDivineBeastsDevelopmentGameplayAbility>();
    const auto* Spell = SpellClass->GetDefaultObject<UDivineBeastsDevelopmentGameplayAbility>();
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("隔离GAS世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* Actor = World->SpawnActor<AActor>();
    auto* ASC = NewObject<UAbilitySystemComponent>(Actor);
    ASC->RegisterComponent();
    ASC->InitAbilityActorInfo(Actor, Actor);
    const FActiveGameplayEffectHandle Stun = ASC->ApplyGameplayEffectToSelf(
        GetDefault<UGamePlatformStunGameplayEffect>(), 1.0f, ASC->MakeEffectContext());
    TestTrue(TEXT("真实眩晕效果已应用"), Stun.IsValid());
    TestTrue(TEXT("眩晕由GAS阻断普攻"), ASC->AreAbilityTagsBlocked(Basic->GetAssetTags()));
    TestTrue(TEXT("眩晕由GAS阻断主动技能"), ASC->AreAbilityTagsBlocked(Spell->GetAssetTags()));
    ASC->RemoveActiveGameplayEffect(Stun);
    const FActiveGameplayEffectHandle Silence = ASC->ApplyGameplayEffectToSelf(
        GetDefault<UGamePlatformSilenceGameplayEffect>(), 1.0f, ASC->MakeEffectContext());
    TestTrue(TEXT("真实沉默效果已应用"), Silence.IsValid());
    TestFalse(TEXT("沉默允许普通攻击"), ASC->AreAbilityTagsBlocked(Basic->GetAssetTags()));
    TestTrue(TEXT("沉默由GAS阻断非普攻技能"), ASC->AreAbilityTagsBlocked(Spell->GetAssetTags()));
    ASC->RemoveActiveGameplayEffect(Silence);
    TestFalse(TEXT("效果移除后普攻阻断解除"), ASC->AreAbilityTagsBlocked(Basic->GetAssetTags()));
    TestFalse(TEXT("效果移除后主动阻断解除"), ASC->AreAbilityTagsBlocked(Spell->GetAssetTags()));
    return true;
}
// 真实保存GE必须解析到CharactersRuntime拥有的气势字段，并实际扣除属性；字符串包含Momentum不足以通过。
// 独立测试ASC每次以100气势开始，不执行玩家技能、不接后端，不把测试初始化作为生产授予。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentCostEffectsTest,
    "DivineBeasts.Abilities.DevelopmentCostEffects",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDevelopmentCostEffectsTest::RunTest(const FString&)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!TestNotNull(TEXT("独立属性结算世界"), World)) return false;
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* Actor = World->SpawnActor<AActor>();
    auto* ASC = NewObject<UAbilitySystemComponent>(Actor);
    ASC->RegisterComponent();
    ASC->InitAbilityActorInfo(Actor, Actor);
    auto* Attributes = NewObject<UDivineBeastsMomentumAttributeSet>(Actor);
    ASC->AddAttributeSetSubobject(Attributes);
    const FGameplayAttribute Momentum = UDivineBeastsMomentumAttributeSet::GetMomentumAttribute();
    const FGameplayAttribute Maximum = UDivineBeastsMomentumAttributeSet::GetMaxMomentumAttribute();
    for (const TCHAR* Hero : {TEXT("Rat"),TEXT("Ox"),TEXT("Tiger"),TEXT("Rabbit"),TEXT("Dragon"),TEXT("Snake"),TEXT("Horse"),TEXT("Goat"),TEXT("Monkey"),TEXT("Rooster"),TEXT("Dog"),TEXT("Boar")})
    {
        for (const TCHAR* Slot : {TEXT("Active01"),TEXT("Active02"),TEXT("Ultimate")})
        {
            const FString Path = FString::Printf(TEXT("/Game/Development/DivineBeasts/Abilities/GE_DBA_%s_Dev%sCost.GE_DBA_%s_Dev%sCost_C"), Hero, Slot, Hero, Slot);
            UClass* Class = LoadClass<UGameplayEffect>(nullptr, *Path);
            if (!TestNotNull(*Path, Class)) continue;
            const UGameplayEffect* Effect = Class->GetDefaultObject<UGameplayEffect>();
            TestEqual(TEXT("真实扣费必须为瞬时效果"), Effect->DurationPolicy, EGameplayEffectDurationType::Instant);
            if (!TestEqual(TEXT("真实成本只有一项修正器"), Effect->Modifiers.Num(), 1)) continue;
            const FGameplayModifierInfo& Modifier = Effect->Modifiers[0];
            TestEqual(TEXT("气势成本采用加法"), Modifier.ModifierOp, EGameplayModOp::Additive);
            if (!TestTrue(*(Path + TEXT(":属性字段可解析")), Modifier.Attribute.IsValid())) continue;
            if (!TestTrue(*(Path + TEXT(":绑定实际气势字段")), Modifier.Attribute == Momentum)) continue;
            float Cost = 0.0f;
            if (!TestTrue(TEXT("成本幅值可真实静态求值"), Modifier.ModifierMagnitude.GetStaticMagnitudeIfPossible(1.0f, Cost))) continue;
            if (!TestTrue(TEXT("成本是有限负值"), FMath::IsFinite(Cost) && Cost < 0.0f)) continue;
            ASC->SetNumericAttributeBase(Maximum, 100.0f);
            ASC->SetNumericAttributeBase(Momentum, 100.0f);
            ASC->ApplyGameplayEffectToSelf(Effect, 1.0f, ASC->MakeEffectContext());
            TestTrue(*(Path + TEXT(":真实GAS气势扣费")), FMath::IsNearlyEqual(ASC->GetNumericAttribute(Momentum), 100.0f + Cost));
        }
    }
    return true;
}
#endif
