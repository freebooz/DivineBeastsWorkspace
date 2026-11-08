// 真实基类 + UMG/Slate 离屏输入回归；不应用捕获 Reply 到桌面，不连接后端。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Tests/GamePlatformUIWindowDragFixture.h"
#include "Misc/AutomationTest.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "RenderingThread.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "CommonInputSettings.h"
#include "ICommonInputModule.h"

namespace
{
FPointerEvent Pointer(const FVector2D Position, const FKey Button, const bool bDown, uint32 PointerId=0)
{
    return FPointerEvent(0, PointerId, Position, Position,
        bDown ? TSet<FKey>{EKeys::LeftMouseButton} : TSet<FKey>{}, Button, 0.f, FModifierKeysState());
}

bool ExerciseWindow(UClass* Class, FAutomationTestBase& Test, const float Scale, const bool bOversized=false)
{
    TStrongObjectPtr<UUserWidget> Widget(NewObject<UUserWidget>(GetTransientPackage(), Class));
    if (!Widget->Initialize()) return false;
    // 用反射设置合同，允许同一测试在没有拖拽实现的基线上实际失败而非编译错误。
    FStructProperty* Config = FindFProperty<FStructProperty>(Class, TEXT("WindowDrag"));
    if (!Test.TestNotNull(TEXT("现有基类必须暴露统一窗口拖拽配置"), Config)) return false;
    Test.TestFalse(TEXT("未声明窗口的现有页面默认不拖动"),Config->ContainerPtrToValuePtr<FGamePlatformUIWindowDragSettings>(Widget.Get())->bEnabled);
    Config->ImportText_Direct(TEXT("(bEnabled=True,BoundsWidgetName=Frame,MoveTargetWidgetNames=(Frame),HeaderHeight=40.0,ViewportInset=8.0)"),
        Config->ContainerPtrToValuePtr<void>(Widget.Get()), Widget.Get(), PPF_None);
    Widget->WidgetTree = NewObject<UWidgetTree>(Widget.Get());
    UCanvasPanel* Root = Widget->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    Widget->WidgetTree->RootWidget = Root;
    UBorder* Frame = Widget->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
    Frame->SetPadding(FMargin(0));
    UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Frame);
    Slot->SetPosition(FVector2D(100,80)); Slot->SetSize(FVector2D(240,160));
    if(bOversized) { Slot->SetPosition(FVector2D(-160,-142.5)); Slot->SetSize(FVector2D(960,765)); }
    UCanvasPanel* Content = Widget->WidgetTree->ConstructWidget<UCanvasPanel>();
    Frame->SetContent(Content);
    UButton* Button = Widget->WidgetTree->ConstructWidget<UButton>();
    UCanvasPanelSlot* ButtonSlot = Content->AddChildToCanvas(Button);
    ButtonSlot->SetPosition(FVector2D(150,4)); ButtonSlot->SetSize(FVector2D(80,32));
    ICommonInputModule::GetSettings().LoadData();
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    auto* Active = Cast<UGamePlatformUIWindowDragActiveFixture>(Widget.Get());
    if (Active) Active->ActivateWidget();
    TStrongObjectPtr<UTextureRenderTarget2D> Target(NewObject<UTextureRenderTarget2D>());
    Target->InitAutoFormat(FMath::RoundToInt(640*Scale),FMath::RoundToInt(480*Scale));
    Target->UpdateResourceImmediate(true);
    auto* Renderer = new FWidgetRenderer(false,true);
    Renderer->DrawWidget(Target.Get(), Slate, Scale, FVector2D(640*Scale,480*Scale), 0.f);
    FlushRenderingCommands();
    if(bOversized)
    {
        Renderer->DrawWidget(Target.Get(),Slate,Scale,FVector2D(640*Scale,480*Scale),0.f); FlushRenderingCommands();
        Test.TestTrue(TEXT("超高窗口首次布局后标题可达"),FMath::IsNearlyEqual(Frame->GetCachedGeometry().GetRenderBoundingRect().Top,8.f*Scale,.1f));
        FGamePlatformUIAdaptiveContext Smaller;
        Smaller.ViewportSize=FVector2D(640,400);
        Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("HandleAdaptiveContextChanged")),&Smaller);
        Slot->SetPosition(FVector2D(-160,-182.5));
        Renderer->DrawWidget(Target.Get(),Slate,Scale,FVector2D(640*Scale,400*Scale),0.f); FlushRenderingCommands();
        Renderer->DrawWidget(Target.Get(),Slate,Scale,FVector2D(640*Scale,400*Scale),0.f); FlushRenderingCommands();
        Test.TestTrue(TEXT("视口缩小后标题仍可达"),FMath::IsNearlyEqual(Frame->GetCachedGeometry().GetRenderBoundingRect().Top,8.f*Scale,.1f));
        Active->DeactivateWidget(); Widget->ReleaseSlateResources(true); BeginCleanup(Renderer);
        return true;
    }
    const FGeometry Geometry = Widget->GetCachedGeometry();
    const auto P = [Scale](double X,double Y){return FVector2D(X*Scale,Y*Scale);};
    const FPointerEvent ButtonPress=Pointer(P(270,95),EKeys::LeftMouseButton,true);
    Test.TestFalse(TEXT("基类预览事件不抢走标题按钮"),Slate->OnPreviewMouseButtonDown(Geometry,ButtonPress).IsEventHandled());
    Test.TestTrue(TEXT("标题按钮先消费正常冒泡输入"),Button->TakeWidget()->OnMouseButtonDown(Button->GetCachedGeometry(),ButtonPress).IsEventHandled());
    Button->TakeWidget()->OnMouseCaptureLost(FCaptureLostEvent(0,0));
    Test.TestTrue(TEXT("按钮输入不改变窗口位置"),Frame->GetRenderTransform().Translation.IsNearlyZero());
    Test.TestFalse(TEXT("内容区左键不启动窗口拖动"), Slate->OnMouseButtonDown(Geometry,Pointer(P(180,160),EKeys::LeftMouseButton,true)).IsEventHandled());
    Test.TestFalse(TEXT("标题右键不启动拖动"), Slate->OnMouseButtonDown(Geometry,Pointer(P(180,95),EKeys::RightMouseButton,false)).IsEventHandled());
    const FReply Down = Slate->OnMouseButtonDown(Geometry,Pointer(P(180,95),EKeys::LeftMouseButton,true));
    Test.TestTrue(TEXT("顶部左键被窗口消费，不能传到游戏镜头"),Down.IsEventHandled());
    Test.TestTrue(TEXT("拖动请求捕获当前窗口"),Down.GetMouseCaptor().IsValid());
    Slate->OnMouseMove(Geometry,Pointer(P(220,125),EKeys::Invalid,true,1));
    Test.TestTrue(TEXT("其他指针不能移动窗口"),Frame->GetRenderTransform().Translation.IsNearlyZero());
    Slate->OnMouseMove(Geometry,Pointer(P(220,125),EKeys::Invalid,true));
    Test.TestTrue(TEXT("DPI换算后实际移动40x30逻辑像素"),Frame->GetRenderTransform().Translation.Equals(FVector2D(40,30),0.1));
    Slate->OnMouseMove(Geometry,Pointer(P(9999,9999),EKeys::Invalid,true));
    Test.TestTrue(TEXT("右下边界保留8逻辑像素"),Frame->GetRenderTransform().Translation.Equals(FVector2D(292,232),0.1));
    Slate->OnMouseMove(Geometry,Pointer(P(-9999,-9999),EKeys::Invalid,true));
    Test.TestTrue(TEXT("左上边界保留8逻辑像素"),Frame->GetRenderTransform().Translation.Equals(FVector2D(-92,-72),0.1));
    Test.TestTrue(TEXT("左键释放结束拖动"),Slate->OnMouseButtonUp(Geometry,Pointer(P(50,50),EKeys::LeftMouseButton,false)).IsEventHandled());
    Slate->OnMouseMove(Geometry,Pointer(P(300,300),EKeys::Invalid,true));
    Test.TestTrue(TEXT("释放后的迟到移动不改变窗口"),Frame->GetRenderTransform().Translation.Equals(FVector2D(-92,-72),0.1));
    // 重新绘制后从实际新位置抓取，再分别验证丢失捕获与页面失活。
    Renderer->DrawWidget(Target.Get(), Slate, Scale, FVector2D(640*Scale,480*Scale), 0.f); FlushRenderingCommands();
    const FVector2D Header = Frame->GetCachedGeometry().LocalToAbsolute(FVector2D(80,15));
    Slate->OnMouseButtonDown(Widget->GetCachedGeometry(),Pointer(Header,EKeys::LeftMouseButton,true));
    Slate->OnMouseCaptureLost(FCaptureLostEvent(0,0));
    const FVector2D Before = Frame->GetRenderTransform().Translation;
    Slate->OnMouseMove(Geometry,Pointer(Header+P(40,30),EKeys::Invalid,true));
    Test.TestTrue(TEXT("丢失捕获取消旧拖动"),Frame->GetRenderTransform().Translation.Equals(Before));
    Slate->OnMouseButtonDown(Widget->GetCachedGeometry(),Pointer(Header,EKeys::LeftMouseButton,true));
    Widget->SetVisibility(ESlateVisibility::Hidden);
    Widget->SetVisibility(ESlateVisibility::Visible);
    Slate->OnMouseMove(Geometry,Pointer(Header+P(40,30),EKeys::Invalid,true));
    Test.TestTrue(TEXT("隐藏再显示不恢复旧拖动"),Frame->GetRenderTransform().Translation.Equals(Before));
    Slate->OnMouseButtonDown(Widget->GetCachedGeometry(),Pointer(Header,EKeys::LeftMouseButton,true));
    Slate->OnFocusLost(FFocusEvent(EFocusCause::Cleared,0));
    Slate->OnMouseMove(Geometry,Pointer(Header+P(40,30),EKeys::Invalid,true));
    Test.TestTrue(TEXT("失焦取消拖动"),Frame->GetRenderTransform().Translation.Equals(Before));
    FGamePlatformUIAdaptiveContext Resized;
    Resized.ViewportSize=FVector2D(800,600);
    Widget->ProcessEvent(Widget->FindFunctionChecked(TEXT("HandleAdaptiveContextChanged")),&Resized);
    Test.TestTrue(TEXT("视口事件恢复设计位置"),Frame->GetRenderTransform().Translation.IsNearlyZero());
    Renderer->DrawWidget(Target.Get(),Slate,Scale,FVector2D(640*Scale,480*Scale),0.f); FlushRenderingCommands();
    Slate->OnMouseButtonDown(Widget->GetCachedGeometry(),Pointer(P(180,95),EKeys::LeftMouseButton,true));
    if(Active) Active->DeactivateWidget();
    else CastChecked<UGamePlatformUIWindowDragPlainFixture>(Widget.Get())->EndForTest();
    const FVector2D ClosedPosition = Frame->GetRenderTransform().Translation;
    Slate->OnMouseMove(Geometry,Pointer(Header+P(120,90),EKeys::Invalid,true));
    Test.TestTrue(TEXT("失活/销毁后的移动不能恢复捕获状态"),Frame->GetRenderTransform().Translation.Equals(ClosedPosition));
    if(!Active) Test.TestTrue(TEXT("销毁恢复默认位置供复用"),ClosedPosition.IsNearlyZero());
    Widget->ReleaseSlateResources(true);
    BeginCleanup(Renderer);
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIWindowDragPlainTest,
    "GamePlatform.UI.WindowDrag.PlainWidget", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIWindowDragPlainTest::RunTest(const FString&)
{return ExerciseWindow(UGamePlatformUIWindowDragPlainFixture::StaticClass(),*this,1.f);}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIWindowDragActiveTest,
    "GamePlatform.UI.WindowDrag.ActivatableDPI", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIWindowDragActiveTest::RunTest(const FString&)
{return ExerciseWindow(UGamePlatformUIWindowDragActiveFixture::StaticClass(),*this,1.5f);}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIWindowDragOversizedTest,
    "GamePlatform.UI.WindowDrag.OversizedViewport", EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIWindowDragOversizedTest::RunTest(const FString&)
{return ExerciseWindow(UGamePlatformUIWindowDragActiveFixture::StaticClass(),*this,1.f,true);}
#endif
