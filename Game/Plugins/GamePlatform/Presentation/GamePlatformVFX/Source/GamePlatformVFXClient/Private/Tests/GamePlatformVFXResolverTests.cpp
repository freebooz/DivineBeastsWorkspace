#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Resolution/GamePlatformVFXResolver.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXResolverExactTest,
    "GamePlatform.VFX.Resolver.Exact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXResolverExactTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXCatalog* Catalog = NewObject<UGamePlatformVFXCatalog>();
    UGamePlatformVFXInstantDefinition* Definition = NewObject<UGamePlatformVFXInstantDefinition>();

    FGamePlatformVFXCatalogEntry& Entry = Catalog->Entries.AddDefaulted_GetRef();
    Entry.DefinitionId = TEXT("VFX.Test.Exact");
    Entry.Definition = Definition;
    Entry.Priority = 10;

    FGamePlatformVFXCatalogRegistry Registry;
    const uint64 RevisionBefore = Registry.GetRevision();
    const FGamePlatformVFXRegistrationHandle Handle = Registry.Register(Catalog);
    TestTrue(TEXT("Catalog 注册成功"), Handle.IsValid());
    TestTrue(TEXT("Catalog 注册使Revision前进"), Registry.GetRevision() > RevisionBefore);

    FGamePlatformVFXRequest Request;
    Request.DefinitionId = Entry.DefinitionId;
    const FGamePlatformVFXResolvedDefinition Resolved = FGamePlatformVFXResolver(Registry).Resolve(Request);
    TestTrue(TEXT("Resolver按DefinitionId命中Definition"), Resolved.Definition.Get() == Definition);
    TestFalse(TEXT("单一最佳结果不歧义"), Resolved.bAmbiguous);

    UGamePlatformVFXCatalog* AmbiguousCatalog = NewObject<UGamePlatformVFXCatalog>();
    UGamePlatformVFXInstantDefinition* AmbiguousDefinition = NewObject<UGamePlatformVFXInstantDefinition>();
    FGamePlatformVFXCatalogEntry& AmbiguousEntry = AmbiguousCatalog->Entries.AddDefaulted_GetRef();
    AmbiguousEntry.DefinitionId = Entry.DefinitionId;
    AmbiguousEntry.Definition = AmbiguousDefinition;
    AmbiguousEntry.Priority = Entry.Priority;
    TestTrue(TEXT("第二Catalog注册成功"), Registry.Register(AmbiguousCatalog).IsValid());

    const FGamePlatformVFXResolvedDefinition Ambiguous = FGamePlatformVFXResolver(Registry).Resolve(Request);
    TestTrue(TEXT("同级歧义必须Fail Closed"), Ambiguous.bAmbiguous);
    TestFalse(TEXT("歧义时不得任意选择Definition"), Ambiguous.IsValid());
    return true;
}

#endif
