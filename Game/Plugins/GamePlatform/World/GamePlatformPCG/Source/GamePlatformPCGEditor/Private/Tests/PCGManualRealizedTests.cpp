#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Authoring/GamePlatformPCGManualAuthoring.h"

// 任意路径/不合法名称不得到达原作者；固定目录由桥构建，不接受用户目录。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPCGManualRealizedNameTest,"GamePlatform.PCG.Editor.ManualRealizedOutputScope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FPCGManualRealizedNameTest::RunTest(const FString&)
{
 FString P,E;
 TestTrue(TEXT("平台中性资产名"),GamePlatformPCGManualAuthoring::MakeRealizedPackage(TEXT("PCG_ExampleScatter"),P,E));
 TestEqual(TEXT("输出严格固定开发目录"),P,FString(TEXT("/Game/Development/Foundation/PCG/Realized/PCG_ExampleScatter")));
 for(const TCHAR* N:{TEXT(""),TEXT("None"),TEXT("../Foreign"),TEXT("Example.WithDot"),TEXT("/Game/Example")})
  TestFalse(TEXT("非法/越界名称拒绝"),GamePlatformPCGManualAuthoring::MakeRealizedPackage(FName(N),P,E));
 return true;
}
#endif
