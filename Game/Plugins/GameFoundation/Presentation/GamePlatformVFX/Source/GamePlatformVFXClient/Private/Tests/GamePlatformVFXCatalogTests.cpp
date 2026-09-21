#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXCatalogRegistrationTest, "GamePlatform.VFX.Catalog.RegisterUnregister", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCatalogRegistrationTest::RunTest(const FString&)
{
    UGamePlatformVFXCatalog* Catalog = NewObject<UGamePlatformVFXCatalog>();
    FGamePlatformVFXCatalogRegistry Registry;
    const FGamePlatformVFXRegistrationHandle Handle = Registry.Register(Catalog);
    TestTrue(TEXT("注册句柄应有效"), Handle.IsValid());
    TestTrue(TEXT("注销应成功"), Registry.Unregister(Handle));
    return true;
}
#endif
