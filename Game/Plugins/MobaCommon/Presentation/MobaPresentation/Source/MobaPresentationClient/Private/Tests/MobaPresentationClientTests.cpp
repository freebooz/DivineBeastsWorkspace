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
    FMobaPresentationCriticalAdapterTest,
    "Moba.Presentation.Client.CriticalAdapter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationCriticalAdapterTest::RunTest(const FString&)
{
    FMobaPresentationCriticalFact Fact;
    Fact.Identity.FactId = FGuid::NewGuid();
    Fact.Identity.bConfirmed = true;
    Fact.Magnitude = 88.0f;

    const FMobaPresentationAdaptedFact Adapted =
        FMobaPresentationFactAdapters::FromCriticalFact(Fact);
    TestEqual(TEXT("Critical语义"), Adapted.Semantic, MobaPresentationTags::Combat_Critical.GetTag());
    TestTrue(TEXT("Critical上下文标志"), Adapted.Context.bCritical);
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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FMobaPresentationCombatCriticalEventAdapterTest,
    "Moba.Presentation.Client.CombatCriticalEventAdapter",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FMobaPresentationCombatCriticalEventAdapterTest::RunTest(const FString&)
{
    FGamePlatformCombatEvent Event;
    Event.EventId = FGuid::NewGuid();
    Event.EventType = EGamePlatformCombatEventType::Damage;
    Event.AppliedMagnitude = 200.0f;
    Event.TargetAvatarGeneration = 3;
    Event.ResultTags.AddTag(GamePlatformCombatTags::Result_Critical);

    TArray<FMobaPresentationAdaptedFact> Facts;
    FMobaPresentationFactAdapters::FromCombatEvent(Event, Facts);
    TestEqual(TEXT("暴击伤害事件产生Hit+Critical两个表现事实"), Facts.Num(), 2);
    if (Facts.Num() == 2)
    {
        TestEqual(TEXT("第一事实为普通命中"), Facts[0].Semantic, MobaPresentationTags::Combat_Hit.GetTag());
        TestEqual(TEXT("第二事实为暴击语义"), Facts[1].Semantic, MobaPresentationTags::Combat_Critical.GetTag());
        TestTrue(TEXT("暴击上下文标志"), Facts[1].Context.bCritical);
        TestEqual(TEXT("暴击表现使用实际伤害值"), Facts[1].Context.Magnitude, 200.0f);
    }
    return true;
}

#endif
