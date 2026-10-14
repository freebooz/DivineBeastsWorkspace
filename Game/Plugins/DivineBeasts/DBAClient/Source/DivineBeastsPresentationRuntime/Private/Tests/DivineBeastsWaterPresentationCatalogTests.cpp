// SSLM水体视觉目录契约测试：两个中立GameplayTag只能映射到预审的逻辑Definition，
// 项目层不得直接链接Niagara播放器或越过平台Data/内容包验证门禁。
#include "Misc/AutomationTest.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsWaterVFXCatalogTests,
    "DivineBeasts.Presentation.Water.Catalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWaterVFXCatalogTests::RunTest(const FString& Parameters)
{
    const FGamePlatformPresentationCatalogFragment Fragment =
        FDivineBeastsPresentationProjectCatalog::BuildWaterVFXFragment();
    TestTrue(TEXT("本项目水体目录必须采用世界生命周期"), 
        Fragment.LifecycleScope == EGamePlatformPresentationContextScope::World);
    TestEqual(TEXT("必须同时提交涟漪和水花两个逻辑条目，不能发布半套目录"),
        Fragment.Entries.Num(), 2);

    TSet<FName> Seen;
    for (const FGamePlatformPresentationCatalogEntry& Entry : Fragment.Entries)
    {
        TestTrue(TEXT("不能发布无效水体语义标签"), Entry.SemanticTag.IsValid());
        TestTrue(TEXT("不能发布未经认证的空逻辑资产"), !Entry.DefinitionId.IsNone());
        TestTrue(TEXT("平台水体目录只能声明中立VFX通道"),
            Entry.ProviderChannel == TEXT("VFX"));
        TestFalse(TEXT("水域逻辑不能映射多次相同语义"),
            Seen.Contains(Entry.SemanticTag.GetTagName()));
        Seen.Add(Entry.SemanticTag.GetTagName());
    }
    TestTrue(TEXT("必须包含平台涟漪逻辑定义"),
        Fragment.Entries.ContainsByPredicate([](const FGamePlatformPresentationCatalogEntry& E)
        {
            return E.DefinitionId == TEXT("platform.water.ripple@1");
        }));
    TestTrue(TEXT("必须包含平台飞溅逻辑定义"),
        Fragment.Entries.ContainsByPredicate([](const FGamePlatformPresentationCatalogEntry& E)
        {
            return E.DefinitionId == TEXT("platform.water.splash@1");
        }));
    return true;
}
#endif
