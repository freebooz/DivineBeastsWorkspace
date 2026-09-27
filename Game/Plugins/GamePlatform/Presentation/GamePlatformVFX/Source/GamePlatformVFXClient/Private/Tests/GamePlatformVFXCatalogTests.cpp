#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXCatalogRegistrationTest,
    "GamePlatform.VFX.Catalog.Registration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCatalogRegistrationTest::RunTest(const FString& Parameters)
{
    FGamePlatformVFXCatalogRegistry Registry;
    UGamePlatformVFXCatalog* Catalog = NewObject<UGamePlatformVFXCatalog>();

    const FGamePlatformVFXRegistrationHandle Handle = Registry.Register(Catalog);
    TestTrue(TEXT("注册句柄有效"), Handle.IsValid());
    TestEqual(TEXT("注册表数量"), Registry.Num(), 1);
    TestTrue(TEXT("注销成功"), Registry.Unregister(Handle));
    TestEqual(TEXT("注销后为空"), Registry.Num(), 0);
    return true;
}

#endif
