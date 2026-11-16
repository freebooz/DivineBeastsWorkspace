#if WITH_DEV_AUTOMATION_TESTS
#include "UI/DivineBeastsDemoHubScreen.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoWorldObservationTest,
    "DivineBeasts.PluginDemos.WorldObservationReentry", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoWorldObservationTest::RunTest(const FString&)
{
    // 原生结构夹具仅验证布局变更事务，不作为Monolith资产或实际画面证明。
    TStrongObjectPtr<UDivineBeastsDemoHubScreen> Page(NewObject<UDivineBeastsDemoHubScreen>());
    Page->WidgetTree=NewObject<UWidgetTree>(Page.Get());
    auto* Tree=Page->WidgetTree.Get();
    auto* Root=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("Root"));
    Tree->RootWidget=Root;
    auto* Background=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("DemoBackground"));
    const FLinearColor OriginalColor(0.17f,0.22f,0.31f,0.86f);
    Background->SetBrushColor(OriginalColor); Root->AddChild(Background);
    auto* Body=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("BodyScroll"));
    auto* BodySlot=Root->AddChildToVerticalBox(Body);BodySlot->SetHorizontalAlignment(HAlign_Right);
    auto* Label=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("WorldObservationButtonLabel"));
    Root->AddChild(Label);
    TArray<UWidget*> Columns;
    const TArray<ESlateVisibility> OriginalVisibility={ESlateVisibility::Visible,ESlateVisibility::Hidden,
        ESlateVisibility::SelfHitTestInvisible,ESlateVisibility::HitTestInvisible};
    int32 Index=0;
    for(const TCHAR* Name:{TEXT("PluginsBounds"),TEXT("FeaturesBounds"),TEXT("SearchBar"),TEXT("CoverageText")})
    {
        auto* Widget=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);
        Root->AddChild(Widget); Widget->SetVisibility(OriginalVisibility[Index++]);Columns.Add(Widget);
    }
    auto CheckRestored=[&]()
    {
        TestFalse(TEXT("模式关闭"),Page->bWorldObservation);
        TestFalse(TEXT("事务已退出"),Page->bApplyingObservationLayout);
        TestTrue(TEXT("背景恢复原值"),Background->GetBrushColor().Equals(OriginalColor));
        TestEqual(TEXT("Body原对齐恢复"),BodySlot->GetHorizontalAlignment(),HAlign_Right);
        for(int32 I=0;I<Columns.Num();++I) TestEqual(TEXT("各列原可见性恢复"),Columns[I]->GetVisibility(),OriginalVisibility[I]);
        TestEqual(TEXT("标签匹配关闭状态"),Label->GetText().ToString(),FString(TEXT("观察实际世界")));
    };
    // 任一首个隐藏通知同步撤销绑定；完整快照必须已在通知之前保存。
    bool bClosed=false;TArray<FDelegateHandle> Handles;
    for(UWidget* Column:Columns)
        Handles.Add(Column->AddFieldValueChangedDelegate(UWidget::FFieldNotificationClassDescriptor::Visibility,
            INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda([&](UObject*,UE::FieldNotification::FFieldId)
            {
                if(!bClosed) {bClosed=true;Page->DetachViewModel();Page->ToggleWorldObservation();}
            })));
    Page->SetWorldObservation(true);TestTrue(TEXT("真实FieldNotify触发关闭"),bClosed);CheckRestored();
    for(int32 I=0;I<Columns.Num();++I)Columns[I]->RemoveFieldValueChangedDelegate(UWidget::FFieldNotificationClassDescriptor::Visibility,Handles[I]);
    Page->SetWorldObservation(true);TestTrue(TEXT("新显式进入仍可成功"),Page->bWorldObservation);
    // 恢复通知内尝试再次进入不得抢占本次清理快照。
    bool bTriedReenter=false;
    const auto Reenter=Columns[0]->AddFieldValueChangedDelegate(UWidget::FFieldNotificationClassDescriptor::Visibility,
        INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda([&](UObject*,UE::FieldNotification::FFieldId)
        {bTriedReenter=true;Page->SetWorldObservation(true);}));
    Page->SetWorldObservation(false);TestTrue(TEXT("恢复通知尝试重入"),bTriedReenter);CheckRestored();
    Columns[0]->RemoveFieldValueChangedDelegate(UWidget::FFieldNotificationClassDescriptor::Visibility,Reenter);
    Page->SetWorldObservation(true);Page->DetachViewModel();CheckRestored();
    Page->SetWorldObservation(true);Page->ToggleWorldObservation();CheckRestored();
    TestTrue(TEXT("退出视觉模式不重绑无效VM"),Page->DemoModel==nullptr);
    // 完整Deactivate→原生Unbind在恢复通知中换VM并重新激活，旧清理不得接管新绑定。
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> OldModel(NewObject<UDivineBeastsDemoHubViewModel>());
    TStrongObjectPtr<UDivineBeastsDemoHubViewModel> NewModel(NewObject<UDivineBeastsDemoHubViewModel>());
    Page->InitializeActivatableViewModel(OldModel.Get());Page->ActivateWidget();
    TestTrue(TEXT("实际激活并绑定旧模型"),Page->IsActivated() && Page->DemoModel==OldModel.Get() && OldModel->IsPageActive());
    Page->SetWorldObservation(true);
    bool bReopened=false;
    const auto Reopen=Columns[0]->AddFieldValueChangedDelegate(UWidget::FFieldNotificationClassDescriptor::Visibility,
        INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateLambda([&](UObject*,UE::FieldNotification::FFieldId)
        {
            if(bReopened)return;bReopened=true;
            Page->InitializeActivatableViewModel(NewModel.Get());Page->ActivateWidget();
        }));
    Page->DeactivateWidget();
    TestTrue(TEXT("恢复通知实际重新激活"),bReopened && Page->IsActivated());
    TestTrue(TEXT("平台与项目桥均保留新VM"),Page->GetViewModel()==NewModel.Get() && Page->DemoModel==NewModel.Get());
    TestTrue(TEXT("新VM页面仍活动"),NewModel->IsPageActive());
    TestFalse(TEXT("旧VM已正常结束"),OldModel->IsPageActive());
    CheckRestored();
    Columns[0]->RemoveFieldValueChangedDelegate(UWidget::FFieldNotificationClassDescriptor::Visibility,Reopen);
    Page->DeactivateWidget();
    return true;
}
#endif
