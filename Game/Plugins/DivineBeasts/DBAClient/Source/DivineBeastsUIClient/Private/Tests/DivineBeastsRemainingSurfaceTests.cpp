// SSLM公共剩余页面资产回归；仅Editor自动化加载真实Monolith资产，不构造生产视觉树或伪造业务服务。
// 验证目标：目录路径不是空合同，命名交互控件存在，通知请求真正更新文本；失败表示资产或原生接线未交付。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Notifications/DivineBeastsNotificationWidget.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsRemainingSurfaceTest,
    "DivineBeasts.UI.RemainingSurfaces.NativeAssets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsRemainingSurfaceTest::RunTest(const FString&)
{
    struct FSurface { const TCHAR* Path; const TCHAR* Required; };
    const FSurface Surfaces[] = {
        {TEXT("Screens/WBP_DBA_UI_Boot"), TEXT("StageText")},
        {TEXT("Screens/WBP_DBA_UI_Quest"), TEXT("QuestList")},
        {TEXT("Screens/WBP_DBA_UI_Social"), TEXT("PartyButton")},
        {TEXT("Screens/WBP_DBA_UI_LiveOps"), TEXT("NoticesList")},
        {TEXT("HUD/WBP_DBA_UI_OpenWorldHUD"), TEXT("WorldStatusText")},
        {TEXT("HUD/WBP_DBA_UI_VillageMainHUD"), TEXT("JournalButton")},
        {TEXT("HUD/WBP_DBA_UI_TutorialGuidance"), TEXT("GuidanceText")},
        {TEXT("HUD/WBP_DBA_UI_TrainingControls"), TEXT("TrainingResetButton")},
        {TEXT("Notifications/WBP_DBA_UI_Toast"), TEXT("MessageText")}
    };
    for (const auto& Surface : Surfaces)
    {
        const FString Package = FString(TEXT("/DBAUIPack_Core/UI/")) + Surface.Path;
        UClass* Class = LoadClass<UUserWidget>(nullptr, *(Package + TEXT(".") + FPaths::GetCleanFilename(Package) + TEXT("_C")));
        if (!TestNotNull(Package, Class)) continue;
        const TStrongObjectPtr<UUserWidget> Widget(NewObject<UUserWidget>(GetTransientPackage(), Class));
        TestTrue(TEXT("真实蓝图初始化成功"), Widget->Initialize());
        TestNotNull(FString::Printf(TEXT("%s 命名合同"), Surface.Required), Widget->GetWidgetFromName(Surface.Required));
        if (auto* Notification = Cast<UDivineBeastsNotificationWidget>(Widget.Get()))
        {
            FGamePlatformUINotificationRequest Request;
            Request.Channel = TEXT("Toast"); Request.Message = FText::FromString(TEXT("真实通知更新"));
            Notification->ApplyNotificationRequest(Request);
            auto* Message = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("MessageText")));
            if (TestNotNull(TEXT("通知正文控件"), Message)) TestEqual(TEXT("原生通知不能停留在设计占位文案"), Message->GetText().ToString(), FString(TEXT("真实通知更新")));
        }
    }
    return true;
}
#endif
