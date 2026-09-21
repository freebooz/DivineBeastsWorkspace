#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Catalogs/GamePlatformVFXCatalog.h"
#include "Resolution/GamePlatformVFXCatalogRegistry.h"
#include "Resolution/GamePlatformVFXResolver.h"
#include "GameplayTagsManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXResolverExactTest,
    "GamePlatform.VFX.Resolver.ExactMatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXResolverExactTest::RunTest(const FString&)
{
    UGamePlatformVFXCatalog* Catalog = NewObject<UGamePlatformVFXCatalog>();
    Catalog->StableId = TEXT("TestCatalog");

    FGamePlatformVFXCatalogEntry Entry;
    Entry.SemanticTag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Presentation.Test.Hit"), false);
    if (!Entry.SemanticTag.IsValid())
    {
        AddWarning(TEXT("测试标签 Presentation.Test.Hit 未注册，跳过精确匹配断言。请在测试工程注册该标签。"));
        return true;
    }

    Entry.DefinitionId = FPrimaryAssetId(FPrimaryAssetType(TEXT("GamePlatformVFXDefinition")), TEXT("Test.Hit"));
    Catalog->Entries.Add(Entry);

    FGamePlatformVFXCatalogRegistry Registry;
    Registry.Register(Catalog);

    FGamePlatformVFXRequest Request;
    Request.SemanticTag = Entry.SemanticTag;
    const FGamePlatformVFXResolveResult Result = FGamePlatformVFXResolver::Resolve(Request, Registry);

    TestTrue(TEXT("Resolver 应成功"), Result.bSuccess);
    TestEqual(TEXT("DefinitionId 应一致"), Result.DefinitionId, Entry.DefinitionId);
    return true;
}

#endif
