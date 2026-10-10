// 兼容原生UMG与CommonUI：复制原生样式值，不修改共享CDO，不改变按钮命令、焦点或IsEnabled。
#include "Styling/GamePlatformUIThemeBinding.h"
#include "Manager/GamePlatformUIManagerSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "CommonBorder.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
struct FPreparedThemeBinding
{
    UWidget* Widget = nullptr;
    UClass* StyleClass = nullptr;
    FGamePlatformUIWidgetStyleBinding Binding;
};
bool IsWidgetCompatible(UWidget* Widget, EGamePlatformUIStyleKind Kind)
{
    switch (Kind)
    {
    case EGamePlatformUIStyleKind::Button: return Widget->IsA<UButton>() || Widget->IsA<UCommonButtonBase>();
    case EGamePlatformUIStyleKind::Text: return Widget->IsA<UTextBlock>();
    case EGamePlatformUIStyleKind::Border: return Widget->IsA<UBorder>();
    default: return false;
    }
}
void ApplyStyle(const FPreparedThemeBinding& Item)
{
    if (Item.Binding.Kind == EGamePlatformUIStyleKind::Button)
    {
        if (auto* Common = Cast<UCommonButtonBase>(Item.Widget))
        {
            Common->SetStyle(TSubclassOf<UCommonButtonStyle>(Item.StyleClass));
        }
        else if (auto* Button = Cast<UButton>(Item.Widget))
        {
            const auto* Style = Item.StyleClass->GetDefaultObject<UCommonButtonStyle>();
            // 保留当前内边距/音效等布局和交互属性，仅替换四种原生视觉状态画刷。
            FButtonStyle Copy = Button->GetStyle();
            Style->GetNormalBaseBrush(Copy.Normal);
            Style->GetNormalHoveredBrush(Copy.Hovered);
            Style->GetNormalPressedBrush(Copy.Pressed);
            Style->GetDisabledBrush(Copy.Disabled);
            Button->SetStyle(Copy);
            // 该按钮已显式选择主题绑定，主题Brush是背景颜色唯一真源。
            // 清除旧蓝图BackgroundColor乘色，避免原来的蓝色按钮把青铜/玉色新皮肤再次染色。
            // 不改变内容ColorAndOpacity、IsEnabled、Padding或点击/焦点委托。
            Button->SetBackgroundColor(FLinearColor::White);
        }
    }
    else if (Item.Binding.Kind == EGamePlatformUIStyleKind::Text)
    {
        auto* Text = CastChecked<UTextBlock>(Item.Widget);
        const auto* Style = Item.StyleClass->GetDefaultObject<UCommonTextStyle>();
        const FSlateFontInfo PreviousFont = Text->GetFont();
        FSlateFontInfo Font;
        FLinearColor Color;
        Style->GetFont(Font);
        Style->GetColor(Color);
        if (Item.Binding.bPreserveFontSize) Font.Size = PreviousFont.Size;
        // CommonTextBlock每次SynchronizeProperties都会重新读取它自己的Style。
        // 先取消当前实例的旧Style选择，再复制主题只读值，才能同时保留字号与页面边距。
        // SetStyle(nullptr)不会改共享CDO；已应用样式类由本绑定对象保持强引用。
        if (auto* CommonText = Cast<UCommonTextBlock>(Text)) CommonText->SetStyle(nullptr);
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
        // 阴影与删除线同样归共享文字样式，而非沿用上一主题/历史页面的局部视觉状态。
        // 保留页面自身字号、边距和内容；只在实例上复制原生样式值，不修改样式CDO。
        FVector2D ShadowOffset = FVector2D::ZeroVector;
        FLinearColor ShadowColor = FLinearColor::Transparent;
        if (Style->bUsesDropShadow)
        {
            Style->GetShadowOffset(ShadowOffset);
            Style->GetShadowColor(ShadowColor);
        }
        Text->SetShadowOffset(ShadowOffset);
        Text->SetShadowColorAndOpacity(ShadowColor);
        FSlateBrush StrikeBrush;
        Style->GetStrikeBrush(StrikeBrush);
        Text->SetStrikeBrush(StrikeBrush);
    }
    else if (Item.Binding.Kind == EGamePlatformUIStyleKind::Border)
    {
        const auto* Style = Item.StyleClass->GetDefaultObject<UCommonBorderStyle>();
        FSlateBrush Brush;
        Style->GetBackgroundBrush(Brush);
        auto* Border = CastChecked<UBorder>(Item.Widget);
        if (auto* CommonBorder = Cast<UCommonBorder>(Border))
        {
            // 后续原生属性同步必须继续使用新主题，不能恢复资产原来的Style。
            CommonBorder->SetStyle(TSubclassOf<UCommonBorderStyle>(Item.StyleClass));
        }
        Border->SetBrush(Brush);
        // 颜色唯一来源是主题Brush，避免历史Border颜色与新主题重复相乘形成黑底。
        Border->SetBrushColor(FLinearColor::White);
    }
}
}

bool UGamePlatformUIThemeBinding::Initialize(UUserWidget* Widget,
    const TArray<FGamePlatformUIWidgetStyleBinding>& InBindings)
{
    check(IsInGameThread());
    Unbind();
    if (!IsValid(Widget) || InBindings.Num() > 128) return false;
    ULocalPlayer* Player = Widget->GetOwningLocalPlayer();
    auto* UI = Player ? Player->GetSubsystem<UGamePlatformUIManagerSubsystem>() : nullptr;
    if (!UI) return false;
    OwnerWidget = Widget;
    Manager = UI;
    Bindings = InBindings;
    UI->OnThemeChanged.AddUniqueDynamic(this, &UGamePlatformUIThemeBinding::HandleThemeChanged);
    // 未就绪主题不改变旧蓝图，资源加载完成事件会再次刷新。
    Refresh();
    return true;
}

void UGamePlatformUIThemeBinding::Unbind()
{
    if (auto* UI = Manager.Get()) UI->OnThemeChanged.RemoveDynamic(this, &UGamePlatformUIThemeBinding::HandleThemeChanged);
    Manager.Reset();
    OwnerWidget.Reset();
    Bindings.Reset();
}

void UGamePlatformUIThemeBinding::BeginDestroy()
{
    Unbind();
    AppliedStyleClasses.Reset();
    Super::BeginDestroy();
}

void UGamePlatformUIThemeBinding::HandleThemeChanged(int64 Revision)
{
    if (Manager.IsValid() && Manager->GetThemeRevision() == Revision) Refresh();
}

bool UGamePlatformUIThemeBinding::Refresh()
{
    check(IsInGameThread());
    if (bApplying) return false;
    UUserWidget* Root = OwnerWidget.Get();
    auto* UI = Manager.Get();
    if (!Root || !Root->WidgetTree || !UI || !UI->GetCurrentThemeId().IsValid()) return false;
    const TStrongObjectPtr<UGamePlatformUIThemeBinding> KeepBinding(this);
    const TStrongObjectPtr<UUserWidget> KeepRoot(Root);
    const TStrongObjectPtr<UGamePlatformUIManagerSubsystem> KeepUI(UI);
    TArray<FPreparedThemeBinding> Prepared;
    // 原生CommonUI样式回调可同步移除子控件并触发GC；为已预检对象保留临时强引用。
    TArray<TStrongObjectPtr<UWidget>> PinnedWidgets;
    TArray<TObjectPtr<UClass>> IncomingClasses;
    TSet<FName> Seen;
    for (const auto& Binding : Bindings)
    {
        UWidget* Widget = Root->WidgetTree->FindWidget(Binding.WidgetName);
        FText Error;
        UClass* StyleClass = UI->ResolveThemeStyle(Binding.Kind, Binding.StyleId, Binding.bAllowParentFallback, Error);
        if (Binding.WidgetName.IsNone() || Seen.Contains(Binding.WidgetName))
        {
            LastError = FText::FromString(TEXT("主题绑定命名控件为空或重复，保留页面旧样式。"));
            return false;
        }
        Seen.Add(Binding.WidgetName);
        if (!Widget || !StyleClass || !IsWidgetCompatible(Widget, Binding.Kind))
        {
            if (Binding.bOptional) continue;
            LastError = Error.IsEmpty() ? FText::FromString(FString::Printf(
                TEXT("控件不存在或与主题样式类型不匹配：%s"), *Binding.WidgetName.ToString())) : Error;
            return false;
        }
        Prepared.Add({Widget, StyleClass, Binding});
        PinnedWidgets.Emplace(Widget);
        IncomingClasses.AddUnique(StyleClass);
    }
    // 所有必需项预检完成才开始写原生控件。更新期间同时保留旧/新样式类，
    // 防止第一个CommonUI样式回调关闭页面并触发GC时，尚未更新的控件失去旧画刷资源。
    // 全部应用成功再缩减到新集合；中途退出保留并集，直到下次成功刷新或控件销毁。
    TGuardValue<bool> ApplyingGuard(bApplying, true);
    for (UClass* IncomingClass : IncomingClasses) AppliedStyleClasses.AddUnique(IncomingClass);
    for (const auto& Item : Prepared)
    {
        if (OwnerWidget.Get() != Root || Manager.Get() != UI ||
            !Root->WidgetTree || Root->WidgetTree->FindWidget(Item.Binding.WidgetName) != Item.Widget)
        {
            LastError = FText::FromString(TEXT("主题应用期间界面已关闭或控件被替换，停止旧实例后续写入。"));
            return false;
        }
        ApplyStyle(Item);
    }
    AppliedStyleClasses = MoveTemp(IncomingClasses);
    LastError = FText::GetEmpty();
    return true;
}
