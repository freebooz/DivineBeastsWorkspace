// 原生足底边界回归：跨越接地、重复姿态、静止/腾空和非法数据。与GPU/人工验收分别记录。
#include "Misc/AutomationTest.h"
#include "Types/GamePlatformSnowContactTypes.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSnowContactGateTest,"GP.Surface.Snow.ContactGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformSnowContactGateTest::RunTest(const FString& Parameters)
{
    FGamePlatformSnowFootContactGate Left,Right;
    TestFalse(TEXT("空中不留印"),Left.Update(FVector::ZeroVector,0,100,false,0));
    TestFalse(TEXT("静止不连续写印"),Left.Update(FVector::ZeroVector,0,0,true,1));
    TestTrue(TEXT("首次真实触地"),Left.Update(FVector::ZeroVector,6,100,true,2));
    TestFalse(TEXT("重复最终姿态去重"),Left.Update(FVector(20,0,0),6,100,true,2.1));
    TestFalse(TEXT("抬脚仅重新布防"),Left.Update(FVector(30,0,0),25,100,true,2.2));
    TestTrue(TEXT("再次落地形成下一步"),Left.Update(FVector(40,0,0),6,100,true,2.4));
    TestTrue(TEXT("右脚独立门闩"),Right.Update(FVector(20,10,0),6,100,true,2.4));
    Left.Update(FVector(40,0,0),25,100,true,2.5);
    TestFalse(TEXT("原地抖动不反复叠印"),Left.Update(FVector(41,0,0),6,100,true,2.6));
    FGamePlatformSnowContact Contact; Contact.EventId=FGuid::NewGuid();
    TestTrue(TEXT("鞋底默认合同合法"),Contact.IsValid());
    Contact.Pressure=1.1f; TestFalse(TEXT("压力不能越界"),Contact.IsValid());
    Contact.Pressure=.95f; Contact.Forward=FVector::ZeroVector;
    TestFalse(TEXT("零朝向拒绝"),Contact.IsValid());
    return true;
}
#endif
