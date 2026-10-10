// 平台层主题回归：只创建瞬态定义/规则，无磁盘资产、无真实网络或业务命令。
// 先约束确定性语义解析与异步资格；真实加载、Widget与Cook须另外验收。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformUIThemeDefinition.h"
#include "Styling/GamePlatformUIThemeRequestState.h"

namespace
{
/** 测试中使用未加载的软类身份，只测试规则排序，不把测试路径作为生产资产。 */
FGamePlatformUIButtonThemeEntry MakeButton(
    const TCHAR* Key, EGamePlatformUIStyleScope Scope,
    const TCHAR* Platform = TEXT(""), int32 Priority = 0)
{
    FGamePlatformUIButtonThemeEntry Entry;
    Entry.Rule.StyleId = FName(Key);
    Entry.Rule.Scope = Scope;
    Entry.Rule.PlatformId = FName(Platform);
    Entry.Rule.Priority = Priority;
    Entry.StyleClass = TSoftClassPtr<UCommonButtonStyle>(
        FSoftObjectPath(TEXT("/Game/Tests/UI/BP_TestStyle.BP_TestStyle_C")));
    return Entry;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIThemeResolutionTest,
    "GamePlatform.UI.Theme.DeterministicResolution",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIThemeResolutionTest::RunTest(const FString&)
{
    UGamePlatformUIThemeDefinition* Theme = NewObject<UGamePlatformUIThemeDefinition>();
    FGamePlatformUIThemeContext Context;
    Context.PlatformId = TEXT("Windows");
    FText Error;
    Theme->Buttons.Add(MakeButton(TEXT("UI.Style.Button.Primary"), EGamePlatformUIStyleScope::Platform));
    Theme->Buttons.Add(MakeButton(TEXT("UI.Style.Button.Primary"), EGamePlatformUIStyleScope::Project));
    TestEqual(TEXT("项目精确样式覆盖平台精确样式"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Primary"), Context, false, Error), 1);
    Theme->Buttons.Add(MakeButton(TEXT("UI.Style.Button.Primary"), EGamePlatformUIStyleScope::ContentPack, TEXT("Android")));
    TestEqual(TEXT("不满足平台上下文的高优先级条目必须排除"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Primary"), Context, false, Error), 1);
    Theme->Buttons.Add(MakeButton(TEXT("UI.Style.Button"), EGamePlatformUIStyleScope::ContentPack));
    TestEqual(TEXT("项目精确语义胜过内容包父语义"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Primary"), Context, true, Error), 1);
    TestEqual(TEXT("默认禁止父语义回退"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Missing"), Context, false, Error), INDEX_NONE);
    TestEqual(TEXT("显式允许父语义回退"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Missing"), Context, true, Error), 3);
    Theme->Buttons.Add(MakeButton(TEXT("UI.Style.Button.Primary"), EGamePlatformUIStyleScope::Project, TEXT("Windows")));
    TestEqual(TEXT("同作用域更具体的上下文优先"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Primary"), Context, false, Error), 4);
    Theme->Buttons.Add(Theme->Buttons[4]);
    TestEqual(TEXT("完全同级歧义必须拒绝，不能靠数组顺序决胜"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Primary"), Context, true, Error), INDEX_NONE);
    TestFalse(TEXT("歧义给出可审阅诊断"), Error.IsEmpty());
    TestEqual(TEXT("空语义拒绝"), Theme->ResolveButtonStyle(NAME_None, Context, true, Error), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIThemeDefinitionValidationTest,
    "GamePlatform.UI.Theme.DefinitionValidation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIThemeDefinitionValidationTest::RunTest(const FString&)
{
    UGamePlatformUIThemeDefinition* Theme = NewObject<UGamePlatformUIThemeDefinition>();
    TestFalse(TEXT("没有平台逻辑身份的主题拒绝"), Theme->ValidateDefinition().IsSuccess());
    FGamePlatformId::TryParse(TEXT("platform.ui.theme.test@1"), Theme->LogicalId);
    Theme->DataVersion.SchemaVersion = 1;
    Theme->DataVersion.ContentRevision = 1;
    Theme->Buttons.Add(MakeButton(TEXT("UI.Style.Button.Primary"), EGamePlatformUIStyleScope::Platform));
    TestTrue(TEXT("单条合法只读规则通过静态校验"), Theme->ValidateDefinition().IsSuccess());
    Theme->Buttons.Add(Theme->Buttons[0]);
    TestFalse(TEXT("同一条件重复规则在注册前拒绝"), Theme->ValidateDefinition().IsSuccess());
    Theme->Buttons.Pop();
    Theme->Buttons[0].Rule.Priority = 1001;
    TestFalse(TEXT("异常优先级拒绝"), Theme->ValidateDefinition().IsSuccess());
    Theme->Buttons[0].Rule.Priority = 0;
    FGamePlatformUIThemeContext Context;
    FText Error;
    TestFalse(TEXT("仅有测试软路径不能冒充真实已加载样式"), Theme->ValidateLoadedStyles(Context, Error));
    Context.QualityTier = 100;
    TestEqual(TEXT("非法上下文不能解析样式"),
        Theme->ResolveButtonStyle(TEXT("UI.Style.Button.Primary"), Context, false, Error), INDEX_NONE);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIThemeRequestStateTest,
    "GamePlatform.UI.Theme.RequestCancellation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FGamePlatformUIThemeRequestStateTest::RunTest(const FString&)
{
    FGamePlatformUIThemeRequestState State;
    const FGuid First = State.Begin();
    TestTrue(TEXT("新请求可以完成"), State.CanComplete(First));
    const FGuid Second = State.Begin();
    TestFalse(TEXT("第二请求使旧请求失去发布资格"), State.CanComplete(First));
    TestFalse(TEXT("旧请求不能取消新请求"), State.Cancel(First));
    TestTrue(TEXT("当前请求可以取消"), State.Cancel(Second));
    TestFalse(TEXT("已取消的迟到回调不能提交"), State.CanComplete(Second));
    const FGuid Third = State.Begin();
    State.Close();
    TestFalse(TEXT("作用域关闭拒绝当前结果"), State.CanComplete(Third));
    TestFalse(TEXT("关闭后不再接受新请求"), State.Begin().IsValid());
    return true;
}
#endif
