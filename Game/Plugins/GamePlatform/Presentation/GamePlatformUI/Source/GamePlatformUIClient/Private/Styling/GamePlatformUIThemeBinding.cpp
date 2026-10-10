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
        IncomingClasses.AddUnique(StyleClass);
    }
    // 所有必需项预检完成才开始写原生控件；持有类强引用，防止回调GC回收绘制资源。
    TGuardValue<bool> ApplyingGuard(bApplying, true);
    AppliedStyleClasses = MoveTemp(IncomingClasses);
    for (const auto& Item : Prepared)
    {
        if (OwnerWidget.Get() != Root || Manager.Get() != UI) return false;
        ApplyStyle(Item);
    }
    LastError = FText::GetEmpty();
    return true;
}
