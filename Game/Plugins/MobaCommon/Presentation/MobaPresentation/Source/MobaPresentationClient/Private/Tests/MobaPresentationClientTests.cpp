#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Adapters/MobaPresentationFactAdapters.h"
#include "Tags/MobaPresentationTags.h"
#include "Tags/GamePlatformCombatTags.h"
#include "Types/GamePlatformCombatEvent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationAbilityAdapterTest,
    "Moba.Presentation.Client.AbilityAdapter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationAbilityAdapterTest::RunTest(const FString&)
{
    FMobaPresentationAbilityFact Fact;
    Fact.Identity.FactId = FGuid::NewGuid();
    Fact.Identity.bConfirmed = true;
    Fact.Type = EMobaPresentationAbilityFactType::CastStart;
    Fact.AbilityId = TEXT("Ability.Test");

    const FMobaPresentationAdaptedFact Adapted =
        FMobaPresentationFactAdapters::FromAbilityFact(Fact);
    TestEqual(TEXT("CastStart语义"), Adapted.Semantic, MobaPresentationTags::Ability_Cast_Start.GetTag());
    TestEqual(TEXT("AbilityId保持"), Adapted.Context.AbilityId, FString(TEXT("Ability.Test")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationPersistentStatusTest,
    "Moba.Presentation.Client.PersistentStatus",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationPersistentStatusTest::RunTest(const FString&)
{
    FMobaPresentationStatusFact Fact;
    Fact.Identity.FactId = FGuid::NewGuid();
    Fact.Identity.bConfirmed = true;
    Fact.Type = EMobaPresentationStatusFactType::Apply;
    Fact.StatusId = TEXT("Status.Test");

    const FMobaPresentationAdaptedFact Adapted =
        FMobaPresentationFactAdapters::FromStatusFact(Fact);
    TestFalse(TEXT("Status Apply应为持续事实"), Adapted.bTransient);
    TestEqual(TEXT("Status Apply使用Persistent生命周期"), Adapted.Lifetime, EGamePlatformPresentationLifetime::Persistent);
    return true;
}


/**
 * 历史暴击标签仅为资产兼容保留，MOBA不能据此产生额外表现事实。
 * 已确认伤害只有一个普通Hit（命中）事实，不能双重播放。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationLegacyCriticalIgnoredTest,
    "Moba.Presentation.Client.LegacyCriticalIgnored",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationLegacyCriticalIgnoredTest::RunTest(const FString&)
{
    FGamePlatformCombatEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.EventType = EGamePlatformCombatEventType::Damage;
    Event.AppliedMagnitude = 200.0f;
    Event.TargetAvatarGeneration = 3;
    Event.ResultTags.AddTag(GamePlatformCombatTags::Result_Critical);

    TArray<FMobaPresentationAdaptedFact> Facts;
    FMobaPresentationFactAdapters::FromCombatEvent(Event, Facts);
    TestEqual(TEXT("历史暴击标签不得产生第二个表现事实"), Facts.Num(), 1);
    if (Facts.Num() == 1)
    {
        TestEqual(TEXT("只保留正常命中语义"), Facts[0].Semantic, MobaPresentationTags::Combat_Hit.GetTag());
        TestEqual(TEXT("正常命中使用实际伤害值"), Facts[0].Context.Magnitude, 200.0f);
    }
    return true;
}

#endif
