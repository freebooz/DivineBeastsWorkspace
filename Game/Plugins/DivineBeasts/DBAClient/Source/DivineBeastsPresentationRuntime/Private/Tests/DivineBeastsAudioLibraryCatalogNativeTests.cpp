// SSLM第二批音频目录UE原生自动化：32条合法语义按分类唯一映射，未知类安全空返回。
// 仅验证项目Catalog/GameplayTag合同；UE原生音频试听与5v5性能仍属独立人工验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAudioLibraryCatalogNativeTest,
    "DivineBeasts.Audio.Library.CategoryCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAudioLibraryCatalogNativeTest::RunTest(const FString& Parameters)
{
    struct FExpectedCategory { const TCHAR* Name; int32 Count; };
    const FExpectedCategory Categories[] = {
        { TEXT("UI"), 3 },
        { TEXT("Spells"), 3 },
        { TEXT("Footsteps"), 6 },
        { TEXT("Combat"), 7 },
        { TEXT("Creatures"), 5 },
        { TEXT("World"), 3 },
        { TEXT("Water"), 2 },
        { TEXT("Ambience"), 3 }
    };
    int32 Total = 0;
    TSet<FName> SemanticKeys;
    TSet<FName> DefinitionKeys;
    for (const auto& Expected : Categories)
    {
        const auto Fragment =
            FDivineBeastsPresentationProjectCatalog::BuildAudioLibraryFragment(FName(Expected.Name));
        TestEqual(FString::Printf(TEXT("音频分类 %s 数量"), Expected.Name), Fragment.Entries.Num(), Expected.Count);
        Total += Fragment.Entries.Num();
        for (const auto& Entry : Fragment.Entries)
        {
            TestTrue(TEXT("GameplayTag有效"), Entry.SemanticTag.IsValid());
            TestEqual(TEXT("音效由统一SFX执行器处理"), Entry.ProviderChannel, FName(TEXT("SFX")));
            TestTrue(TEXT("Definition使用规范逻辑身份"),
                Entry.DefinitionId.ToString().StartsWith(TEXT("presentation.dba.audiolibrary.")));
            TestFalse(TEXT("目录不能重复绑定同一语义"), SemanticKeys.Contains(Entry.SemanticTag.GetTagName()));
            TestFalse(TEXT("不同声音不能复用同一Definition身份"), DefinitionKeys.Contains(Entry.DefinitionId));
            SemanticKeys.Add(Entry.SemanticTag.GetTagName());
            DefinitionKeys.Add(Entry.DefinitionId);
        }
    }
    TestEqual(TEXT("全部分类条目总数"), Total, 32);
    TestEqual(TEXT("去重后真实音频语义数量"), SemanticKeys.Num(), 32);
    TestEqual(TEXT("去重后Definition数量"), DefinitionKeys.Num(), 32);
    const auto Unknown = FDivineBeastsPresentationProjectCatalog::BuildAudioLibraryFragment(TEXT("Unknown"));
    TestTrue(TEXT("未知分类必须返回空目录"), Unknown.Entries.IsEmpty());
    return true;
}
#endif
