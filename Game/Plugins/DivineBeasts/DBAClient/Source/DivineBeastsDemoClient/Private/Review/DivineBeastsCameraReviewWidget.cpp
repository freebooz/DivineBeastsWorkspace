// 只用UE UMG原生控件构建开发面板：鼠标点击和键盘共用项目同一镜头请求入口。
// 资源及界面不进入发布Cook，未创建新的第二套UI流程/配置注册表。
#include "Review/DivineBeastsCameraReviewWidget.h"
#include "Review/DivineBeastsCinematicCameraReviewActor.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "CommonButtonBase.h"
#include "Styling/SlateColor.h"

UDivineBeastsCameraReviewWidget::UDivineBeastsCameraReviewWidget()
{
    // 控件树在RebuildWidget建立后，平台NativeConstruct统一应用主题并订阅变化；
    // NativeDestruct负责解绑。这里只提供项目语义，不加载材质、不复制按钮播放器。
    const FName ButtonNames[] = {
        TEXT("Lens24"), TEXT("Lens35"), TEXT("Lens50"), TEXT("Lens85"),
        TEXT("OpenAperture"), TEXT("CloseAperture"), TEXT("FocusNear"),
        TEXT("FocusFar"), TEXT("ResetCamera")
    };
    for (const FName Name : ButtonNames)
    {
        FGamePlatformUIWidgetStyleBinding Binding;
        Binding.WidgetName = Name;
        Binding.StyleId = TEXT("UI.Style.Button.Secondary");
        Binding.Kind = EGamePlatformUIStyleKind::Button;
        ThemeBindings.Add(Binding);
    }
}

void UDivineBeastsCameraReviewWidget::BindReviewActor(
    ADivineBeastsCinematicCameraReviewActor* Actor)
{
    ReviewActor = Actor;
}

void UDivineBeastsCameraReviewWidget::SetCameraStatus(const FString& Message)
{
    if (StatusText)
    {
        StatusText->SetText(FText::FromString(Message));
    }
}

UHorizontalBox* UDivineBeastsCameraReviewWidget::AddButtonRow(UVerticalBox& Parent)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(
        UHorizontalBox::StaticClass());
    if (UVerticalBoxSlot* RowSlot = Parent.AddChildToVerticalBox(Row))
    {
        RowSlot->SetPadding(FMargin(0.f, 3.f, 0.f, 3.f));
    }
    return Row;
}

UButton* UDivineBeastsCameraReviewWidget::AddButton(
    UHorizontalBox& Row, FName Name, const FString& Label)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>(
        UButton::StaticClass(), Name);
    // 背景颜色由共享主题画刷唯一提供，避免旧蓝色乘色使云纹按钮再次偏色。
    Button->SetBackgroundColor(FLinearColor::White);
    if (InitialButtonStyleClass)
    {
        // 独立开发地图可能没有活动LocalPlayer主题：初始画刷仍读取同一份正式样式CDO。
        // 这里只配置初始值，后续主题变化及生命周期继续由平台基类的唯一事件链处理。
        const UCommonButtonStyle* InitialStyle = InitialButtonStyleClass->GetDefaultObject<UCommonButtonStyle>();
        FButtonStyle ButtonStyle = Button->GetStyle();
        InitialStyle->GetNormalBaseBrush(ButtonStyle.Normal);
        InitialStyle->GetNormalHoveredBrush(ButtonStyle.Hovered);
        InitialStyle->GetNormalPressedBrush(ButtonStyle.Pressed);
        InitialStyle->GetDisabledBrush(ButtonStyle.Disabled);
        Button->SetStyle(ButtonStyle);
    }
    UTextBlock* Caption = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass());
    Caption->SetText(FText::FromString(Label));
    // 多平台演示控件统一使用小字号；防止默认UMG字体放大后横排按钮溢出面板。
    FSlateFontInfo ButtonFont = Caption->GetFont();
    ButtonFont.Size = 10;
    Caption->SetFont(ButtonFont);
    Caption->SetColorAndOpacity(FSlateColor(FLinearColor::White));
    Button->SetContent(Caption);
    if (UHorizontalBoxSlot* ButtonSlot = Row.AddChildToHorizontalBox(Button))
    {
        ButtonSlot->SetPadding(FMargin(3.f));
    }
    return Button;
}

TSharedRef<SWidget> UDivineBeastsCameraReviewWidget::RebuildWidget()
{
    // UE在NativeConstruct之前建立Slate树，必须先在RebuildWidget配置真实控件。
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, UWidgetTree::StaticClass(),
            TEXT("RuntimeCameraReviewTree"), RF_Transient);
    }
    if (!WidgetTree->RootWidget)
    {

    // 仅此开发控件首次建立时加载一次；样式身份与正式页面一致，无新调色表或全局缓存。
    InitialButtonStyleClass = LoadClass<UCommonButtonStyle>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Styles/BP_DBA_ButtonStyle_Secondary.BP_DBA_ButtonStyle_Secondary_C"));
    ensureMsgf(InitialButtonStyleClass, TEXT("镜头开发按钮缺少项目共享云纹样式"));

    UBorder* Background = WidgetTree->ConstructWidget<UBorder>(
        UBorder::StaticClass(), TEXT("CinematicReviewBackground"));
    Background->SetBrushColor(FLinearColor(0.015f, 0.02f, 0.04f, 0.90f));
    Background->SetPadding(FMargin(14.f, 10.f));
    WidgetTree->RootWidget = Background;

    UVerticalBox* Layout = WidgetTree->ConstructWidget<UVerticalBox>(
        UVerticalBox::StaticClass(), TEXT("CinematicReviewControls"));
    Background->SetContent(Layout);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("SSLM 电影镜头演示")));
    FSlateFontInfo TitleFont = Title->GetFont();
    TitleFont.Size = 10;
    Title->SetFont(TitleFont);
    Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.85f, 1.0f, 1.f)));
    Layout->AddChildToVerticalBox(Title);

    UHorizontalBox* LensRow = AddButtonRow(*Layout);
    AddButton(*LensRow, TEXT("Lens24"), TEXT("1 · 24mm"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::Choose24);
    AddButton(*LensRow, TEXT("Lens35"), TEXT("2 · 35mm"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::Choose35);
    AddButton(*LensRow, TEXT("Lens50"), TEXT("3 · 50mm"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::Choose50);
    AddButton(*LensRow, TEXT("Lens85"), TEXT("4 · 85mm"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::Choose85);

    UHorizontalBox* OpticsRow = AddButtonRow(*Layout);
    AddButton(*OpticsRow, TEXT("OpenAperture"), TEXT("Z · 开大光圈"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::ApertureOpen);
    AddButton(*OpticsRow, TEXT("CloseAperture"), TEXT("X · 缩小光圈"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::ApertureClose);

    UHorizontalBox* FocusRow = AddButtonRow(*Layout);
    AddButton(*FocusRow, TEXT("FocusNear"), TEXT("C · 焦点更近"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::FocusNear);
    AddButton(*FocusRow, TEXT("FocusFar"), TEXT("V · 焦点更远"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::FocusFar);
    AddButton(*FocusRow, TEXT("ResetCamera"), TEXT("R · 重置参数"))->OnClicked.AddDynamic(
        this, &UDivineBeastsCameraReviewWidget::ResetAll);

    StatusText = WidgetTree->ConstructWidget<UTextBlock>(
        UTextBlock::StaticClass(), TEXT("CinematicReviewStatus"));
    StatusText->SetText(FText::FromString(TEXT("选择镜头，调整光圈与焦点")));
    FSlateFontInfo StatusFont = StatusText->GetFont();
    StatusFont.Size = 10;
    StatusText->SetFont(StatusFont);
    StatusText->SetAutoWrapText(true);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.90f, 0.95f, 1.f)));
    Layout->AddChildToVerticalBox(StatusText);
    }
    return Super::RebuildWidget();
}

void UDivineBeastsCameraReviewWidget::Choose24()
{ if (auto* Actor = ReviewActor.Get()) Actor->SelectLens(0); }
void UDivineBeastsCameraReviewWidget::Choose35()
{ if (auto* Actor = ReviewActor.Get()) Actor->SelectLens(1); }
void UDivineBeastsCameraReviewWidget::Choose50()
{ if (auto* Actor = ReviewActor.Get()) Actor->SelectLens(2); }
void UDivineBeastsCameraReviewWidget::Choose85()
{ if (auto* Actor = ReviewActor.Get()) Actor->SelectLens(3); }
void UDivineBeastsCameraReviewWidget::ApertureOpen()
{ if (auto* Actor = ReviewActor.Get()) Actor->AdjustAperture(true); }
void UDivineBeastsCameraReviewWidget::ApertureClose()
{ if (auto* Actor = ReviewActor.Get()) Actor->AdjustAperture(false); }
void UDivineBeastsCameraReviewWidget::FocusNear()
{ if (auto* Actor = ReviewActor.Get()) Actor->AdjustFocus(false); }
void UDivineBeastsCameraReviewWidget::FocusFar()
{ if (auto* Actor = ReviewActor.Get()) Actor->AdjustFocus(true); }
void UDivineBeastsCameraReviewWidget::ResetAll()
{ if (auto* Actor = ReviewActor.Get()) Actor->ResetLens(); }
