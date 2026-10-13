// SSLM待审证据拒绝回归；不创建假运行、人工结果或生产文件，不构建时仅交付测试源码。
#if WITH_DEV_AUTOMATION_TESTS
#include "Evidence/DivineBeastsDemoEvidenceWriter.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoEvidenceTest, "DivineBeasts.PluginDemos.Evidence",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoEvidenceTest::RunTest(const FString&)
{
    FDivineBeastsDemoRunEvidence Empty; FString Path, Error;
    TestFalse(TEXT("无实际运行不写证据"), DivineBeasts::Demo::WriteRunEvidence(Empty, Path, Error));
    TestTrue(TEXT("拒绝含具名诊断"), !Error.IsEmpty()); TestTrue(TEXT("失败不返回伪路径"), Path.IsEmpty());
    // 仅用内存合同验证写前拒绝；不生成冒充实际运行的文件或人工签收记录。
    Empty.Run.Token.RunId = FGuid::NewGuid(); Empty.Run.Token.WorldId = FGuid::NewGuid(); Empty.Run.Token.Generation = 1;
    Empty.Run.ScenarioId = TEXT("Sample.Normal"); Empty.Scenario.ScenarioId = Empty.Run.ScenarioId;
    Empty.ReviewCases.AddDefaulted();
    Empty.SourceFingerprint = FString::ChrN(64, TEXT('z'));
    TestFalse(TEXT("64字符非SHA256拒绝，不能仅检查长度"), DivineBeasts::Demo::WriteRunEvidence(Empty, Path, Error));
    Empty.SourceFingerprint = FString::ChrN(64, TEXT('a')); Empty.Run.CompletedOperations = 9007199254740992LL;
    TestFalse(TEXT("JSON精度外操作计数写前拒绝"), DivineBeasts::Demo::WriteRunEvidence(Empty, Path, Error));
    return true;
}
#endif
