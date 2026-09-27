#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Definitions/GamePlatformVFXAttachedDefinition.h"
#include "Definitions/GamePlatformVFXProjectileDefinition.h"
#include "Definitions/GamePlatformVFXBeamDefinition.h"
#include "Definitions/GamePlatformVFXAreaDefinition.h"
#include "Definitions/GamePlatformVFXShieldDefinition.h"
#include "Definitions/GamePlatformVFXPortalDefinition.h"
#include "Definitions/GamePlatformVFXTrailDefinition.h"
#include "Definitions/GamePlatformVFXWorldDefinition.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXAllBehaviorsTest,
    "GamePlatform.VFX.Behavior.AllTenDefinitions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXAllBehaviorsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Instant"), NewObject<UGamePlatformVFXInstantDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Instant);
    TestEqual(TEXT("Attached"), NewObject<UGamePlatformVFXAttachedDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Attached);
    TestEqual(TEXT("Projectile"), NewObject<UGamePlatformVFXProjectileDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Projectile);
    TestEqual(TEXT("Beam"), NewObject<UGamePlatformVFXBeamDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Beam);
    TestEqual(TEXT("Area"), NewObject<UGamePlatformVFXAreaDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Area);
    TestEqual(TEXT("Shield"), NewObject<UGamePlatformVFXShieldDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Shield);
    TestEqual(TEXT("Portal"), NewObject<UGamePlatformVFXPortalDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Portal);
    TestEqual(TEXT("Trail"), NewObject<UGamePlatformVFXTrailDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Trail);
    TestEqual(TEXT("World"), NewObject<UGamePlatformVFXWorldDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::World);
    TestEqual(TEXT("Composite"), NewObject<UGamePlatformVFXCompositeDefinition>()->GetBehavior(), EGamePlatformVFXBehavior::Composite);
    return true;
}

#endif
