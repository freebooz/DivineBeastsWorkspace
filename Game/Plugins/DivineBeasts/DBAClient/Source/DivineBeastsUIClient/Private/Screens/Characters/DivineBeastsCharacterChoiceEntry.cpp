// 项目客户端卡片适配：所有调用在游戏线程，视觉层消费页面投影，身份有效性由页面再次核对。
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

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
