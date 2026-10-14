// SSLM待审证据拒绝回归；不创建假运行、人工结果或生产文件，不构建时仅交付测试源码。
#if WITH_DEV_AUTOMATION_TESTS
#include "Evidence/DivineBeastsDemoEvidenceWriter.h"
#include "Evidence/DivineBeastsDemoRuntimeOrigin.h"
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
    // 只测试受控来源参数的解析合同，不读取假DLL，不把内存参数视作实际加载证明。
    FGuid LaunchId; FString ContentHash;
    const FString ValidArgs = TEXT("-DBAPluginDemoLaunchId=20261013-1234-4000-8000-000000000001 -DBAPluginDemoContentFingerprint=") + FString::ChrN(64, TEXT('a')) + TEXT(" -LiveCoding=false -NoLiveCoding -NoHotReload");
    TestTrue(TEXT("受控来源完整参数可解析"), DivineBeasts::Demo::ParseRuntimeOriginArguments(*ValidArgs, LaunchId, ContentHash).IsSuccess());
    TestTrue(TEXT("来源身份非零"), LaunchId.IsValid());
    TestEqual(TEXT("内容摘要不被替换"), ContentHash, FString::ChrN(64, TEXT('a')));
    TestFalse(TEXT("普通手工启动不具机器验证来源"), DivineBeasts::Demo::ParseRuntimeOriginArguments(TEXT(""), LaunchId, ContentHash).IsSuccess());
    TestFalse(TEXT("缺热重载隔离拒绝"), DivineBeasts::Demo::ParseRuntimeOriginArguments(*ValidArgs.Replace(TEXT("-NoHotReload"), TEXT("")), LaunchId, ContentHash).IsSuccess());
    TestFalse(TEXT("只有构建NoLiveCoding参数不等于运行禁用"), DivineBeasts::Demo::ParseRuntimeOriginArguments(*ValidArgs.Replace(TEXT("-LiveCoding=false"), TEXT("")), LaunchId, ContentHash).IsSuccess());
    return true;
}
#endif
