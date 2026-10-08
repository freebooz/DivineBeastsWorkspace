// 客户端显示意图契约回归：条目复用、禁用和取消均不得派发过期角色身份。
// 使用无视觉树的本地控件投影，不模拟后端成功；实际Monolith资产另行编译、回读和运行验证。
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsChoiceIdentityTest,
    "DivineBeasts.UI.Characters.ChoiceIdentityLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsChoiceIdentityTest::RunTest(const FString&)
{
    TStrongObjectPtr<UDivineBeastsCharacterChoiceEntry> Entry(NewObject<UDivineBeastsCharacterChoiceEntry>());
    TArray<FString> Requests;
    Entry->OnChoiceRequested.AddLambda([&Requests](UDivineBeastsCharacterChoiceEntry* Source) { Requests.Add(Source->GetChoiceIdentity()); });
    Entry->RequestChoice();
    TestEqual(TEXT("未配置条目不派发请求"), Requests.Num(), 0);
    Entry->ConfigureChoice(TEXT("character-old"), FText::FromString(TEXT("旧档案")), {}, {});
    Entry->SetChoiceEnabled(true);
    Entry->ConfigureChoice(TEXT("character-current"), FText::FromString(TEXT("当前档案")), {}, {});
    Entry->RequestChoice();
    TestEqual(TEXT("条目复用只派发当前身份"), Requests.Num(), 1);
    if (!Requests.IsEmpty()) { TestEqual(TEXT("不能携带旧身份"), Requests[0], FString(TEXT("character-current"))); }
    Entry->SetChoiceEnabled(false);
    Entry->RequestChoice();
    TestEqual(TEXT("忙碌或禁用时不派发"), Requests.Num(), 1);
    Entry->SetChoiceEnabled(true);
    Entry->ResetChoice();
    Entry->RequestChoice();
    TestEqual(TEXT("页面取消后旧点击无效"), Requests.Num(), 1);
    TestTrue(TEXT("取消清除显示身份"), Entry->GetChoiceIdentity().IsEmpty());
    Entry->OnChoiceRequested.Clear();
    return true;
}
#endif
