// 项目层鸟群目录回归：三种物种拥有不同语义和主资产ID，不重复构建Niagara播放器。
// 仅测试正式映射合同，不把此测试当作真实资源加载或GPU画面通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Catalog/DivineBeastsPresentationProjectCatalog.h"
#include "GamePlatformPresentationTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsBirdFlockCatalogTest,
    "DivineBeasts.Presentation.BirdFlock.Catalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsBirdFlockCatalogTest::RunTest(const FString&)
{
    const auto Fragment=FDivineBeastsPresentationProjectCatalog::BuildBirdFlockFragment();
    TestEqual(TEXT("一份共享目录只含三物种"),Fragment.Entries.Num(),3);
    TSet<FName> Definitions;
    TSet<FGameplayTag> Semantics;
    for(const auto& Entry:Fragment.Entries)
    {
        TestEqual(TEXT("统一VFX提供者，不建立第二套执行器"),Entry.ProviderChannel,FName(TEXT("VFX")));
        TestTrue(TEXT("鸟类语义真实注册"),Entry.SemanticTag.IsValid());
        TestTrue(TEXT("主资产逻辑ID而非硬编码文件路径"),Entry.DefinitionId.ToString().StartsWith(TEXT("presentation.dba.environment.birdflock.")));
        Definitions.Add(Entry.DefinitionId); Semantics.Add(Entry.SemanticTag);
    }
    TestEqual(TEXT("物种定义不重复"),Definitions.Num(),3);
    TestEqual(TEXT("不同物种不会产生解析冲突"),Semantics.Num(),3);
    TestFalse(TEXT("未知鸟种不静默回退"),FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(TEXT("Unknown")).IsValid());
    TestTrue(TEXT("海鸥可选"),FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(TEXT("Seagull")).IsValid());
    FGamePlatformPresentationRequest Request;
    Request.RequestId=FGuid::NewGuid();
    Request.SemanticTag=FDivineBeastsPresentationProjectCatalog::GetBirdFlockTag(TEXT("Seagull"));
    Request.IntegerParameters.Add(TEXT("User.BirdCount"),512);
    TestTrue(TEXT("整数数量可通过中立表现总线"),Request.IsValid());
    Request.FloatParameters.Add(TEXT("User.BirdCount"),512.0f);
    TestFalse(TEXT("同名整数浮点冲突被拒绝"),Request.IsValid());
    Request.FloatParameters.Reset();
    for(int32 Index=0;Index<16;++Index)
        Request.IntegerParameters.Add(FName(*FString::Printf(TEXT("User.Extra%d"),Index)),Index);
    TestFalse(TEXT("所有类型合计超过16个字段被拒绝"),Request.IsValid());
    return true;
}
#endif
