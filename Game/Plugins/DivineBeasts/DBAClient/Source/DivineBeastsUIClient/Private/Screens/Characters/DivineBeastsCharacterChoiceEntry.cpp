// 项目客户端卡片适配：所有调用在游戏线程，视觉层消费页面投影，身份有效性由页面再次核对。
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"
#include "Styling/DivineBeastsUIStyleBindings.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

namespace
{
    /** 图集行序是美术合同，不使用后端资格数组顺序或英雄ID字典序推算。
     * 只有十二个稳定身份可以使用图像；未知身份保留可读文字回退，不显示上一条目图像。
     */
    int32 ZodiacAtlasIndex(const FString& Identity)
    {
        static const TCHAR* Ids[] = {
            TEXT("Hero.Zodiac.Rat"), TEXT("Hero.Zodiac.Ox"), TEXT("Hero.Zodiac.Tiger"), TEXT("Hero.Zodiac.Rabbit"),
            TEXT("Hero.Zodiac.Dragon"), TEXT("Hero.Zodiac.Snake"), TEXT("Hero.Zodiac.Horse"), TEXT("Hero.Zodiac.Goat"),
            TEXT("Hero.Zodiac.Monkey"), TEXT("Hero.Zodiac.Rooster"), TEXT("Hero.Zodiac.Dog"), TEXT("Hero.Zodiac.Boar")};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ids); ++Index)
        {
            if (Identity == Ids[Index]) { return Index; }
        }
        return INDEX_NONE;
    }
}

// 默认主题语义只保存在项目层CDO：当项目主题ID为空时平台兼容绑定保持旧Widget样式。
// 不修改已有Widget树、命名控件、焦点、登录输入/命令或加载任何项目纹理。
UDivineBeastsCharacterChoiceEntry::UDivineBeastsCharacterChoiceEntry()
{
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("ChoiceButton"), DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("ChoiceTitle"), DivineBeasts::UI::Styling::Keys::TextTitle, EGamePlatformUIStyleKind::Text));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("ChoiceSubtitle"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("ChoiceEmblem"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text));
}

void UDivineBeastsCharacterChoiceEntry::ConfigureChoice(const FString& Identity,
    const FText& Title, const FText& Subtitle, const FText& Emblem)
{
    ChoiceIdentity = Identity;
    if (ChoiceTitle) { ChoiceTitle->SetText(Title); }
    if (ChoiceSubtitle)
    {
        ChoiceSubtitle->SetText(Subtitle);
        ChoiceSubtitle->SetVisibility(Subtitle.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
    if (ChoiceEmblem) { ChoiceEmblem->SetText(Emblem); }
    if (ChoicePortrait)
    {
        FSlateBrush Brush = ChoicePortrait->GetBrush();
        const int32 Index = ZodiacAtlasIndex(Identity);
        const bool bHasImage = Index != INDEX_NONE && IsValid(Cast<UTexture2D>(Brush.GetResourceObject()));
        if (bHasImage)
        {
            // 单张4列×3行原稿无损复用；只改变采样区域，不调色、不加载第二套头像服务。
            const FVector2f Minimum(float(Index % 4) / 4.0f, float(Index / 4) / 3.0f);
            Brush.SetUVRegion(FBox2f(Minimum, Minimum + FVector2f(0.25f, 1.0f / 3.0f)));
        }
        else { Brush.SetUVRegion(FBox2f()); }
        ChoicePortrait->SetBrush(Brush);
        ChoicePortrait->SetColorAndOpacity(FLinearColor::White);
        ChoicePortrait->SetVisibility(bHasImage ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (ChoiceTitle) { ChoiceTitle->SetVisibility(bHasImage ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
        if (ChoiceEmblem) { ChoiceEmblem->SetVisibility(bHasImage ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    }
    // 图像选项仍保留中文名称提示；按钮只提交本地意图，不直接改变创建资格。
    if (ChoiceButton) { ChoiceButton->SetToolTipText(Title); }
    // 未创建视觉树时也允许保存投影，便于初始化与独立契约测试；空身份始终禁用。
    SetChoiceEnabled(bChoiceEnabled);
}
void UDivineBeastsCharacterChoiceEntry::SetChoiceSelected(bool bSelected)
{
    if (SelectedFrame) { SelectedFrame->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}
void UDivineBeastsCharacterChoiceEntry::SetChoiceEnabled(bool bEnabled)
{
    bChoiceEnabled = bEnabled && !ChoiceIdentity.IsEmpty();
    if (ChoiceButton) { ChoiceButton->SetIsEnabled(bChoiceEnabled); }
}
void UDivineBeastsCharacterChoiceEntry::ResetChoice()
{
    ChoiceIdentity.Reset();
    SetChoiceEnabled(false);
    SetChoiceSelected(false);
    if (ChoicePortrait)
    {
        // 保留内容资产自己的纹理引用，只撤销本条目UV与显示，避免复用时泄漏旧生肖。
        FSlateBrush Brush = ChoicePortrait->GetBrush();
        Brush.SetUVRegion(FBox2f());
        ChoicePortrait->SetBrush(Brush);
        ChoicePortrait->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ChoiceButton) { ChoiceButton->SetToolTipText(FText::GetEmpty()); }
}
UWidget* UDivineBeastsCharacterChoiceEntry::GetChoiceFocusTarget() const { return ChoiceButton; }
void UDivineBeastsCharacterChoiceEntry::RequestChoice()
{
    // 复用后读取当前身份，不捕获旧索引；页面仍必须核对最新快照，拒绝已撤销角色。
    if (bChoiceEnabled && !ChoiceIdentity.IsEmpty()) { OnChoiceRequested.Broadcast(this); }
}
void UDivineBeastsCharacterChoiceEntry::BindUIEvents()
{
    if (ChoiceButton) { ChoiceButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleButtonClicked); }
}
void UDivineBeastsCharacterChoiceEntry::UnbindUIEvents()
{
    if (ChoiceButton) { ChoiceButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleButtonClicked); }
    ResetChoice();
}
void UDivineBeastsCharacterChoiceEntry::HandleButtonClicked() { RequestChoice(); }
