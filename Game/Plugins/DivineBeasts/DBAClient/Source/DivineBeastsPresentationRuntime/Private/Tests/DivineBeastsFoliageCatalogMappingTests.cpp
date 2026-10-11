#include "Misc/AutomationTest.h"
#include "Engine/AssetManager.h" // 校验四种Definition的稳定主资产ID是否被真实AssetManager扫描。
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

/**
 * 四种自然飘落物通过GamePlatformData（平台数据服务）加载；
 * 仅磁盘上存在Definition文件不足以证明客户端能够按逻辑ID找到它们。
 * 本测试检查当前引擎AssetManager真实扫描到的PrimaryAssetPath（主资产路径）。
 * 不同步加载游戏世界、不生成Niagara实例、不触发权威玩法。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsFoliagePrimaryAssetRegistrationTest,
    "DivineBeasts.Presentation.FallingFoliage.PrimaryAssetRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsFoliagePrimaryAssetRegistrationTest::RunTest(const FString&)
{
    struct FSpecies
    {
        const TCHAR* Style;
        const TCHAR* LogicalId;
    };
    const FSpecies Species[] = {
        { TEXT("Peach"), TEXT("presentation.dba.environment.fallingfoliage.peach@1") },
        { TEXT("Maple"), TEXT("presentation.dba.environment.fallingfoliage.maple@1") },
        { TEXT("Bamboo"), TEXT("presentation.dba.environment.fallingfoliage.bamboo@1") },
        { TEXT("Ginkgo"), TEXT("presentation.dba.environment.fallingfoliage.ginkgo@1") },
    };

    UAssetManager& Manager = UAssetManager::Get();
    for (const FSpecies& Entry : Species)
    {
        const FPrimaryAssetId PrimaryAssetId(
            FPrimaryAssetType(TEXT("GamePlatformDefinition")),
            FName(Entry.LogicalId));
        const FSoftObjectPath ResolvedPath = Manager.GetPrimaryAssetPath(PrimaryAssetId);
        const FString ExpectedPackage = FString::Printf(
            TEXT("/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Definitions/DA_DBA_VFX_FallingFoliage_%s"),
            Entry.Style);
        TestTrue(
            *FString::Printf(TEXT("%s Definition必须由AssetManager按逻辑ID发现"), Entry.Style),
            ResolvedPath.IsValid());
        TestEqual(
            *FString::Printf(TEXT("%s 必须映射到真实项目内容包资产"), Entry.Style),
            ResolvedPath.GetLongPackageName(),
            ExpectedPackage);
    }
    return !HasAnyErrors();
}

#endif
