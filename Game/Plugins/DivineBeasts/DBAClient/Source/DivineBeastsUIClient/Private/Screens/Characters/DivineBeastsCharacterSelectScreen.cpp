#include "Screens/Characters/DivineBeastsCharacterSelectScreen.h"

#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"

UDivineBeastsCharacterSelectViewModel*
UDivineBeastsCharacterSelectScreen::GetCharacterSelectViewModel() const
{
    return Cast<UDivineBeastsCharacterSelectViewModel>(GetViewModel());
}

// 项目客户端选择适配：只呈现档案快照，输入索引不代表后端正式选择。
// 激活订阅、失活解绑；角色归属和下一步流程仍由统一命令端口决定。
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Localization/DivineBeastsUILocalization.h"

void UDivineBeastsCharacterSelectScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    SelectButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelect);
    LogoutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogout);
    RotateLeftButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateLeft);
    RotateRightButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateRight);
    CharacterList->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleCharacterChanged);
    if (auto* VM = GetCharacterSelectViewModel())
    {
        VM->OnViewStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
        VM->OnCommandCompleted.AddUniqueDynamic(this, &ThisClass::HandleCommandCompleted);
    }
    RefreshPresentation();
}
void UDivineBeastsCharacterSelectScreen::NativeOnDeactivated()
{
    SelectButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSelect);
    LogoutButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLogout);
    RotateLeftButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRotateLeft);
    RotateRightButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRotateRight);
    CharacterList->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleCharacterChanged);
    if (auto* VM = GetCharacterSelectViewModel())
    {
        VM->OnViewStateChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
        VM->OnCommandCompleted.RemoveDynamic(this, &ThisClass::HandleCommandCompleted);
    }
    Super::NativeOnDeactivated();
}
void UDivineBeastsCharacterSelectScreen::RefreshPresentation()
{
    auto* VM = GetCharacterSelectViewModel();
    if (!VM || bRefreshing) { return; }
    TGuardValue<bool> Guard(bRefreshing, true);
    const auto& State = VM->GetStateRef();
    const int32 OldIndex = CharacterList->GetSelectedIndex();
    const FString OldId = PresentedCharacterIds.IsValidIndex(OldIndex) ? PresentedCharacterIds[OldIndex] : FString();
    PresentedCharacterIds.Reset();
    CharacterList->ClearOptions();
    int32 SelectedIndex = 0;
    for (const auto& Item : State.Characters)
    {
        const int32 Index = PresentedCharacterIds.Add(Item.CharacterId);
        // 序号区分重名档案，用户文案不暴露内部CharacterId。
        CharacterList->AddOption(FString::Printf(TEXT("%d. %s · %s"), Index + 1,
            *Item.DisplayName.ToString(), *FDivineBeastsUILocalization::HeroNameToText(Item.HeroDefinitionId).ToString()));
        if (Item.CharacterId == OldId || (OldId.IsEmpty() && Item.bSelected)) { SelectedIndex = Index; }
    }
    if (PresentedCharacterIds.IsValidIndex(SelectedIndex)) { CharacterList->SetSelectedIndex(SelectedIndex); }
    const int32 Index = CharacterList->GetSelectedIndex();
    const bool bEnabled = State.Characters.IsValidIndex(Index) && State.Characters[Index].bEnabled;
    SelectButton->SetIsEnabled(VM->CanSubmitSelection() && bEnabled);
    CharacterList->SetIsEnabled(!State.bBusy);
    StatusText->SetText(State.Characters.IsValidIndex(Index) && !bEnabled ? State.Characters[Index].DisabledReason
        : FText::FromString(State.bBusy ? TEXT("正在确认角色，请稍候…") : TEXT("选择角色后点击确认。")));
    FText Error = GetPageLoadError();
    if (Error.IsEmpty()) { Error = State.ErrorText; }
    if (Error.IsEmpty()) { Error = FDivineBeastsUILocalization::ErrorCodeToText(VM->GetLastCommandErrorCode()); }
    ErrorText->SetText(Error);
    ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}
void UDivineBeastsCharacterSelectScreen::HandleCharacterChanged(FString, ESelectInfo::Type)
{
    if (bRefreshing) { return; }
    const int32 Index = CharacterList->GetSelectedIndex();
    if (auto* VM = GetCharacterSelectViewModel(); VM && PresentedCharacterIds.IsValidIndex(Index))
    {
        const FString Id = PresentedCharacterIds[Index];
        if (const auto* Item = VM->GetStateRef().Characters.FindByPredicate([&Id](const auto& Candidate) { return Candidate.CharacterId == Id; }))
        {
            VM->PreviewCharacterHero(Item->HeroDefinitionId);
        }
    }
    RefreshPresentation();
}
void UDivineBeastsCharacterSelectScreen::HandleSelect()
{
    auto* VM = GetCharacterSelectViewModel();
    const int32 Index = CharacterList->GetSelectedIndex();
    if (!VM || !VM->CanSubmitSelection() || !PresentedCharacterIds.IsValidIndex(Index)) { return; }
    const FString Id = PresentedCharacterIds[Index];
    const auto* Item = VM->GetStateRef().Characters.FindByPredicate([&Id](const auto& Candidate) { return Candidate.CharacterId == Id; });
    // 提交前重查资格，拒绝已被禁用或从当前快照移除的档案。
    if (Item && Item->bEnabled) { VM->SelectPersistentCharacter(Id); }
    RefreshPresentation();
}
void UDivineBeastsCharacterSelectScreen::HandleStateChanged(int32, int32) { RefreshPresentation(); }
void UDivineBeastsCharacterSelectScreen::HandleCommandCompleted(FGuid, FName) { RefreshPresentation(); }
// 转动只修改预览舞台；注销结束统一会话，不在页面中直接改变业务状态。
void UDivineBeastsCharacterSelectScreen::HandleRotateLeft() { if (auto* VM = GetCharacterSelectViewModel()) { VM->RotateCharacterPreview(-30.0f); } }
void UDivineBeastsCharacterSelectScreen::HandleRotateRight() { if (auto* VM = GetCharacterSelectViewModel()) { VM->RotateCharacterPreview(30.0f); } }
void UDivineBeastsCharacterSelectScreen::HandleLogout() { if (auto* VM = GetCharacterSelectViewModel()) { VM->Logout(); } }
