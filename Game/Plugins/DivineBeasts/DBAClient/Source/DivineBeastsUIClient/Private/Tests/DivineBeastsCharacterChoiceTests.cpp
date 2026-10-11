// 客户端显示意图契约回归：条目复用、禁用和取消均不得派发过期角色身份。
// 使用无视觉树的本地控件投影，不模拟后端成功；实际Monolith资产另行编译、回读和运行验证。
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Blueprint/UserWidget.h"

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

// 验证真实Monolith卡片的可见投影，不只检查图集身份或文件名。
// 误用后端列表顺序、遗漏UV更新、复用遗留图像和未知身份无回退都会使本测试失败。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsZodiacChoiceAtlasTest,
    "DivineBeasts.UI.Characters.ZodiacImageChoice", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsZodiacChoiceAtlasTest::RunTest(const FString&)
{
    UClass* ChoiceClass = LoadClass<UDivineBeastsCharacterChoiceEntry>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_HeroChoice.WBP_DBA_HeroChoice_C"));
    if (!TestNotNull(TEXT("真实生肖卡片类"), ChoiceClass)) { return false; }
    TStrongObjectPtr<UDivineBeastsCharacterChoiceEntry> Entry(NewObject<UDivineBeastsCharacterChoiceEntry>(GetTransientPackage(), ChoiceClass));
    Entry->Initialize();
    auto* Image = Cast<UImage>(Entry->GetWidgetFromName(TEXT("ChoicePortrait")));
    auto* Title = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("ChoiceTitle")));
    auto* Emblem = Cast<UTextBlock>(Entry->GetWidgetFromName(TEXT("ChoiceEmblem")));
    auto* Button = Cast<UButton>(Entry->GetWidgetFromName(TEXT("ChoiceButton")));
    if (!TestNotNull(TEXT("真实图像"), Image) || !TestNotNull(TEXT("名称回退"), Title) ||
        !TestNotNull(TEXT("旧文字图腾"), Emblem) || !TestNotNull(TEXT("真实按钮"), Button)) { return false; }
    UObject* Atlas = Image->GetBrush().GetResourceObject();
    if (!TestNotNull(TEXT("真实图集资源"), Atlas)) { return false; }
    // 表格期望独立于生产映射，倒序配置模拟后端资格数组重排及同一条目复用。
    struct FExpectedCell { const TCHAR* Id; float X; float Y; };
    const FExpectedCell Cells[] = {
        {TEXT("Boar"), .75f, 2.f/3.f}, {TEXT("Dog"), .5f, 2.f/3.f},
        {TEXT("Rooster"), .25f, 2.f/3.f}, {TEXT("Monkey"), 0.f, 2.f/3.f},
        {TEXT("Goat"), .75f, 1.f/3.f}, {TEXT("Horse"), .5f, 1.f/3.f},
        {TEXT("Snake"), .25f, 1.f/3.f}, {TEXT("Dragon"), 0.f, 1.f/3.f},
        {TEXT("Rabbit"), .75f, 0.f}, {TEXT("Tiger"), .5f, 0.f},
        {TEXT("Ox"), .25f, 0.f}, {TEXT("Rat"), 0.f, 0.f}};
    for (const auto& Cell : Cells)
    {
        const FText Name = FText::FromString(FString(Cell.Id));
        Entry->ConfigureChoice(FString(TEXT("Hero.Zodiac.")) + Cell.Id, Name, {}, {});
        const FBox2f UV = Image->GetBrush().GetUVRegion();
        TestTrue(TEXT("生肖格子正确，不能显示上一生肖"), UV.bIsValid &&
            UV.Min.Equals(FVector2f(Cell.X, Cell.Y)) && UV.Max.Equals(FVector2f(Cell.X + .25f, Cell.Y + 1.f/3.f)));
        TestEqual(TEXT("图像可见但不阻断按钮"), Image->GetVisibility(), ESlateVisibility::HitTestInvisible);
        TestEqual(TEXT("生肖选项不再显示文字图腾"), Emblem->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("生肖选项不再显示文字名称"), Title->GetVisibility(), ESlateVisibility::Collapsed);
        TestTrue(TEXT("鼠标悬停保留名称提示"), Button->GetToolTipText().EqualTo(Name));
        TestTrue(TEXT("不对源图额外乘色"), Image->GetColorAndOpacity().Equals(FLinearColor::White));
    }
    Entry->ConfigureChoice(TEXT("Hero.Zodiac.Unknown"), FText::FromString(TEXT("未知生肖")), {}, {});
    TestEqual(TEXT("未知身份不能显示旧图"), Image->GetVisibility(), ESlateVisibility::Collapsed);
    TestEqual(TEXT("未知身份仍有可读文字"), Title->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Entry->ResetChoice();
    TestEqual(TEXT("取消清除图像"), Image->GetVisibility(), ESlateVisibility::Collapsed);
    const FBox2f CancelledUV = Image->GetBrush().GetUVRegion();
    TestFalse(TEXT("取消清除旧UV"), CancelledUV.bIsValid);
    TestTrue(TEXT("取消保留内容包的纹理引用"), Image->GetBrush().GetResourceObject() == Atlas);
    return true;
}

// 真实命名绑定是鼠标旋转的前提；纯手势状态测试无法发现可选控件被重建为匿名节点。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPreviewSurfaceBindingTest,
    "DivineBeasts.UI.Characters.PreviewSurfaceBinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPreviewSurfaceBindingTest::RunTest(const FString&)
{
    for (const TCHAR* Page : {TEXT("CharacterCreate"), TEXT("CharacterSelect")})
    {
        const FString ClassPath = FString::Printf(TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_%s.WBP_DBA_UI_%s_C"), Page, Page);
        UClass* Class = LoadClass<UUserWidget>(nullptr, *ClassPath);
        if (!TestNotNull(TEXT("真实角色页类"), Class)) { return false; }
        TStrongObjectPtr<UUserWidget> Screen(NewObject<UUserWidget>(GetTransientPackage(), Class));
        Screen->Initialize();
        const auto* Property = FindFProperty<FObjectPropertyBase>(Class, TEXT("PreviewDragSurface"));
        if (!TestNotNull(TEXT("原生透明区域绑定字段"), Property)) { return false; }
        UObject* Surface = Property->GetObjectPropertyValue_InContainer(Screen.Get());
        TestNotNull(TEXT("原生鼠标入口实际获得透明区域"), Surface);
        TestTrue(TEXT("绑定正是当前页面区域"), Surface == Screen->GetWidgetFromName(TEXT("PreviewDragSurface")));
    }
    return true;
}
#endif
