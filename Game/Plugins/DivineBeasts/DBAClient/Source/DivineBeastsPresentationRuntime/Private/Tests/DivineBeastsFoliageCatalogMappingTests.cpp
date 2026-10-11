#include "Misc/AutomationTest.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "Tags/DivineBeastsPresentationTags.h"

/** 项目四种自然飘落预设与唯一平台VFX解析语义的自动化测试。 */
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFoliageCatalogMappingTest,
    "DivineBeasts.Presentation.FallingFoliage.CatalogMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFoliageCatalogMappingTest::RunTest(const FString&)
{
    struct FSpecies { const TCHAR* Style; const TCHAR* LogicalId; };
    const FSpecies Species[] = {
        { TEXT("Peach"), TEXT("presentation.dba.environment.fallingfoliage.peach@1") },
        { TEXT("Maple"), TEXT("presentation.dba.environment.fallingfoliage.maple@1") },
        { TEXT("Bamboo"), TEXT("presentation.dba.environment.fallingfoliage.bamboo@1") },
        { TEXT("Ginkgo"), TEXT("presentation.dba.environment.fallingfoliage.ginkgo@1") },
    };
    for (const FSpecies& Entry : Species)
    {
        const auto Fragment = FDivineBeastsPresentationProjectCatalog::BuildFrontEndFoliageFragment(Entry.Style);
        TestEqual(TEXT("一个预设只生成一个目录条目"), Fragment.Entries.Num(), 1);
        if (Fragment.Entries.Num() != 1)
            continue;
        const auto& Mapping = Fragment.Entries[0];
        TestEqual(TEXT("稳定逻辑身份"), Mapping.DefinitionId, FName(Entry.LogicalId));
        TestTrue(TEXT("统一环境表现语义"),
            Mapping.SemanticTag == DivineBeastsPresentationTags::FrontEnd_FallingFoliage);
        TestEqual(TEXT("平台低层VFX提供者"), Mapping.ProviderChannel, FName(TEXT("VFX")));
    }
    const auto Invalid = FDivineBeastsPresentationProjectCatalog::BuildFrontEndFoliageFragment(TEXT("Unknown"));
    TestEqual(TEXT("非法风格不发布目录"), Invalid.Entries.Num(), 0);
    return !HasAnyErrors();
}
#endif
