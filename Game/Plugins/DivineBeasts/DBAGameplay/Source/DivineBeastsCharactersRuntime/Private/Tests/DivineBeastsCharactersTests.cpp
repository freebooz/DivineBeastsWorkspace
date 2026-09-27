#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Creation/DivineBeastsCharacterCreationProvider.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Definitions/GamePlatformHeroDefinition.h"
#include "Initialization/GamePlatformCharacterInitializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsHeroCatalogTest,
    "DivineBeasts.Characters.HeroCatalog",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHeroCatalogTest::RunTest(const FString&)
{
    const TArray<FName>& Ids = FDivineBeastsHeroCatalog::GetCoreHeroIds();
    TestEqual(TEXT("Exactly 12 zodiac Hero IDs"), Ids.Num(), 12);

    TSet<FName> Unique;
    for (const FName Id : Ids)
    {
        TestTrue(TEXT("Hero ID valid"), !Id.IsNone());
        Unique.Add(Id);

        EDivineBeastsZodiacIdentity Zodiac =
            EDivineBeastsZodiacIdentity::Rat;
        TestTrue(
            TEXT("Hero ID maps to zodiac"),
            FDivineBeastsHeroCatalog::TryGetZodiacIdentity(Id, Zodiac));
        TestTrue(
            TEXT("Zodiac tag valid"),
            FDivineBeastsHeroCatalog::GetZodiacTag(Zodiac).IsValid());
        TestNotEqual(
            TEXT("Display key is not protocol identity"),
            FDivineBeastsHeroCatalog::GetDisplayNameKey(Id),
            Id);
    }
    TestEqual(TEXT("No duplicate Hero IDs"), Unique.Num(), 12);

    TestTrue(TEXT("Rat ID"), Ids.Contains(TEXT("Hero.Zodiac.Rat")));
    TestTrue(TEXT("Boar ID"), Ids.Contains(TEXT("Hero.Zodiac.Boar")));
    TestFalse(
        TEXT("Unknown Hero rejected"),
        FDivineBeastsHeroCatalog::IsCoreHeroId(TEXT("Hero.Zodiac.Unknown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformHeroDefinitionValidationTest,
    "DivineBeasts.Characters.GenericHeroDefinition",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformHeroDefinitionValidationTest::RunTest(const FString&)
{
    UDivineBeastsHeroDefinition* Definition =
        NewObject<UDivineBeastsHeroDefinition>();
    FString Error;
    TestFalse(TEXT("Empty definition invalid"), Definition->IsDefinitionValid(Error));

    Definition->DefinitionId = TEXT("Hero.Zodiac.Rat");
    Definition->ContentRevision = TEXT("1");
    Definition->ZodiacIdentity = EDivineBeastsZodiacIdentity::Rat;
    Definition->ZodiacTag =
        FDivineBeastsHeroCatalog::GetZodiacTag(
            EDivineBeastsZodiacIdentity::Rat);
    Definition->DisplayNameKey = TEXT("Hero.Zodiac.Rat.Name");
    Definition->ContentPackId =
        FDivineBeastsHeroCatalog::GetCoreContentPackId();

    Error.Reset();
    TestTrue(
        TEXT("Project hero definition valid"),
        Definition->IsProjectDefinitionValid(Error));

    Definition->Movement.MaxWalkSpeed = -1.0f;
    Error.Reset();
    TestFalse(
        TEXT("Negative movement rejected"),
        Definition->IsProjectDefinitionValid(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAppearanceSchemaTest,
    "DivineBeasts.Characters.AppearanceSchema",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAppearanceSchemaTest::RunTest(const FString&)
{
    FDivineBeastsAppearanceOptionSchema Schema;
    Schema.BodyVariants = { TEXT("Default"), TEXT("VariantA") };
    Schema.HeadPresets = { TEXT("Head01") };
    Schema.SkinMarkingPresets = { TEXT("None"), TEXT("Mark01") };

    TMap<FString, FString> Valid;
    Valid.Add(TEXT("BodyVariant"), TEXT("Default"));
    Valid.Add(TEXT("HeadPreset"), TEXT("Head01"));

    FString Error;
    TestTrue(TEXT("Valid appearance"), Schema.ValidateSelection(Valid, Error));

    Valid.Add(TEXT("Element"), TEXT("Fire"));
    Error.Reset();
    TestFalse(TEXT("Unknown/legacy appearance field rejected"), Schema.ValidateSelection(Valid, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsInitializationContextTest,
    "DivineBeasts.Characters.InitializationContext",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsInitializationContextTest::RunTest(const FString&)
{
    FGamePlatformCharacterInitializationContext Context;
    FString Error;
    TestFalse(TEXT("Empty context invalid"), Context.IsValid(Error));

    Context.HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    Context.CharacterId = TEXT("character-test");
    Context.SpawnGeneration = 1;
    Context.AvatarGeneration = 1;
    Error.Reset();
    TestTrue(TEXT("Persistent context valid"), Context.IsValid(Error));

    Context.CharacterId.Reset();
    Context.bPersistentCharacterIdRequired = false;
    Error.Reset();
    TestTrue(TEXT("AI/nonpersistent context valid"), Context.IsValid(Error));
    return true;
}

#endif
