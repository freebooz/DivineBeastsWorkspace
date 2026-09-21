#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Manifests/PCGSourceFingerprint.h"

// 顺序不参与来源身份；依赖内容和引擎版本必须参与，不能只哈希路径或随机种子。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGSourceFingerprintTest, "GamePlatform.PCG.Editor.SourceFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPCGSourceFingerprintTest::RunTest(const FString& Parameters)
{
    const FString Baseline = GamePlatformPCGEditor::HashRecords({TEXT("mesh|bytes-a"), TEXT("engine|5.8.0")});
    TestEqual(TEXT("依赖顺序不改变指纹"), Baseline,
        GamePlatformPCGEditor::HashRecords({TEXT("engine|5.8.0"), TEXT("mesh|bytes-a")}));
    TestNotEqual(TEXT("真实依赖字节改变使清单失效"), Baseline,
        GamePlatformPCGEditor::HashRecords({TEXT("engine|5.8.0"), TEXT("mesh|bytes-b")}));
    TestNotEqual(TEXT("引擎版本参与指纹"), Baseline,
        GamePlatformPCGEditor::HashRecords({TEXT("engine|5.8.1"), TEXT("mesh|bytes-a")}));
    return true;
}
#endif
