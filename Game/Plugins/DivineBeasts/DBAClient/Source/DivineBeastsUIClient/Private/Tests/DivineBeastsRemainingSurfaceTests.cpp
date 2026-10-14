// SSLM公共剩余页面资产回归；仅Editor自动化加载真实Monolith资产，不构造生产视觉树或伪造业务服务。
// 验证目标：目录路径不是空合同，命名交互控件存在，通知请求真正更新文本；失败表示资产或原生接线未交付。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Notifications/DivineBeastsNotificationWidget.h"
#include "HUD/World/DivineBeastsWorldHUDBase.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "Screens/DivineBeastsUIScreenCatalog.h"
#include "Screens/GamePlatformUIScreen.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

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
        if (FCString::Strcmp(Surface.Path, TEXT("Screens/WBP_DBA_UI_Quest")) == 0)
        {
            // 无任务服务是正常空态；从实际目录获取焦点，不能指向刷新后禁用的任务列表。
            const auto* Descriptor = FDivineBeastsUIScreenCatalog::Find(TEXT("UI.Screen.Quest"));
            auto* Page = Cast<UGamePlatformUIScreen>(Widget.Get());
            if (TestNotNull(TEXT("任务目录"), Descriptor) && TestNotNull(TEXT("任务平台页面"), Page))
            {
                Page->InitializeScreen(Descriptor->SurfaceId, nullptr, Descriptor->DefaultFocusWidgetName,
                    Descriptor->InputMode, Descriptor->PausePolicy);
                Page->ActivateWidget();
                auto* Focus = Page->GetWidgetFromName(Descriptor->DefaultFocusWidgetName);
                if (TestNotNull(TEXT("任务空态焦点必须存在"), Focus))
                    TestTrue(TEXT("无任务服务时初始焦点仍启用"), Focus->GetIsEnabled());
                Page->DeactivateWidget();
            }
        }
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHUDClearedModelTest,
    "DivineBeasts.UI.RemainingSurfaces.ClearHUDModel",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsHUDClearedModelTest::RunTest(const FString&)
{
    UClass* Class=LoadClass<UDivineBeastsWorldHUDBase>(nullptr,TEXT("/DBAUIPack_Core/UI/HUD/WBP_DBA_UI_OpenWorldHUD.WBP_DBA_UI_OpenWorldHUD_C"));
    if(!TestNotNull(TEXT("真实世界HUD资产"),Class))return false;
    TStrongObjectPtr<UDivineBeastsWorldHUDBase> HUD(NewObject<UDivineBeastsWorldHUDBase>(GetTransientPackage(),Class));
    TStrongObjectPtr<UDivineBeastsUIViewModel> Model(NewObject<UDivineBeastsUIViewModel>());
    HUD->Initialize();Model->BeginPage();HUD->InitializeWidgetViewModel(Model.Get());
    // 只构造真实资产的瞬态测试实例，以进入平台Construct生命周期；不生成或保存新的视觉资产。
    const auto Slate=HUD->TakeWidget();
    auto* Status=Cast<UTextBlock>(HUD->GetWidgetFromName(TEXT("WorldStatusText")));
    if(TestNotNull(TEXT("世界状态文本"),Status))
    {
        Status->SetText(FText::FromString(TEXT("上一世界的过期文本")));
        HUD->InitializeWidgetViewModel(nullptr);
        TestFalse(TEXT("清空模型必须立即撤下旧显示"),Status->GetText().ToString().Contains(TEXT("过期文本")));
    }
    Model->EndPage();return true;
}
#endif
