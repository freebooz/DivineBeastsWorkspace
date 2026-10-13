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
    return true;
}
#endif
