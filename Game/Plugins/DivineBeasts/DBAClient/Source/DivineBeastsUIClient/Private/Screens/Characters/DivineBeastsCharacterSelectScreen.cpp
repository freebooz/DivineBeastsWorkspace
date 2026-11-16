#include "Screens/Characters/DivineBeastsCharacterSelectScreen.h"
#include "Styling/DivineBeastsUIStyleBindings.h"

#include "ViewModels/Characters/DivineBeastsCharacterSelectViewModel.h"

// 默认主题语义只保存在项目层CDO：当项目主题ID为空时平台兼容绑定保持旧Widget样式。
// 不修改已有Widget树、命名控件、焦点、登录输入/命令或加载任何项目纹理。
UDivineBeastsCharacterSelectScreen::UDivineBeastsCharacterSelectScreen()
{
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("SelectButton"), DivineBeasts::UI::Styling::Keys::ButtonPrimary, EGamePlatformUIStyleKind::Button));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("OpenCharacterCreateButton"), DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LogoutButton"), DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("SelectedHeroName"), DivineBeasts::UI::Styling::Keys::TextTitle, EGamePlatformUIStyleKind::Text, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("SelectedCharacterName"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
    // 旧UMG UButton不管理内部文本样式；命名子TextBlock按相同正文视觉单独可选适配。
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("SelectButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LogoutButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("OpenCharacterCreateButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
}

UDivineBeastsCharacterSelectViewModel*
UDivineBeastsCharacterSelectScreen::GetCharacterSelectViewModel() const
{
    return Cast<UDivineBeastsCharacterSelectViewModel>(GetViewModel());
}

// 项目客户端选择适配：只呈现档案快照，本地浏览ID不代表后端正式选择。
// 激活订阅、失活解绑；角色归属和下一步流程仍由统一命令端口决定。
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"

void UDivineBeastsCharacterSelectScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (!IsActivated()) { return; }
    SelectButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelect);
    LogoutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogout);
    if (OpenCharacterCreateButton) { OpenCharacterCreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenCharacterCreate); }
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
    if (OpenCharacterCreateButton) { OpenCharacterCreateButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleOpenCharacterCreate); }
    if (auto* VM = GetCharacterSelectViewModel())
    {
        VM->OnViewStateChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
        VM->OnCommandCompleted.RemoveDynamic(this, &ThisClass::HandleCommandCompleted);
    }
    ClearCharacterChoices();
    PresentedCharacterIds.Reset();
    PreviewedCharacterId.Reset();
    Super::NativeOnDeactivated();
}
void UDivineBeastsCharacterSelectScreen::RefreshPresentation()
{
    auto* VM = GetCharacterSelectViewModel();
    if (!VM || bRefreshing) { return; }
    TGuardValue<bool> Guard(bRefreshing, true);
    // 独立值快照避免预览完成/页面切换的同步通知使数组引用失效；本地ID只投影当前快照。
    const auto State = VM->GetStateRef();
    const FString OldId = PreviewedCharacterId;
    const TArray<FString> PreviousIds = PresentedCharacterIds;
    PresentedCharacterIds.Reset();
    for (const auto& Item : State.Characters)
    {
        PresentedCharacterIds.Add(Item.CharacterId);
    }
    if (!PresentedCharacterIds.Contains(PreviewedCharacterId))
    {
        const auto* Default = State.Characters.FindByPredicate([](const auto& Item) { return Item.bSelected && Item.bEnabled; });
        if (!Default) { Default = State.Characters.FindByPredicate([](const auto& Item) { return Item.bEnabled; }); }
        if (!Default && !State.Characters.IsEmpty()) { Default = &State.Characters[0]; }
        PreviewedCharacterId = Default ? Default->CharacterId : FString();
    }
    if (PreviousIds != PresentedCharacterIds || ChoiceEntries.Num() != PresentedCharacterIds.Num()) { RebuildCharacterChoices(); }
    const auto* Selected = State.Characters.FindByPredicate([this](const auto& Item) { return Item.CharacterId == PreviewedCharacterId; });
    const bool bEnabled = Selected && Selected->bEnabled;
    SelectButton->SetIsEnabled(State.Characters.IsEmpty() ? VM->CanOpenCharacterCreation() : VM->CanSubmitSelection() && bEnabled);
    if (SelectButtonLabel) { SelectButtonLabel->SetText(FText::FromString(State.Characters.IsEmpty() ? TEXT("创建角色") : TEXT("进入游戏"))); }
    if (OpenCharacterCreateButton) { OpenCharacterCreateButton->SetIsEnabled(VM->CanOpenCharacterCreation()); }
    // 主按钮空态明确引导创建；忙碌提示和已有档案禁用原因保持原有优先级。
    StatusText->SetText(Selected && !bEnabled ? Selected->DisabledReason
        : FText::FromString(State.bBusy ? TEXT("正在确认角色，请稍候…")
            : (State.Characters.IsEmpty() ? TEXT("尚未创建角色，请点击创建角色开始。") : TEXT("选择角色后点击确认。"))));
    FText Error = GetPageLoadError();
    if (Error.IsEmpty()) { Error = State.ErrorText; }
    if (Error.IsEmpty()) { Error = FDivineBeastsUILocalization::ErrorCodeToText(VM->GetLastCommandErrorCode()); }
    ErrorText->SetText(Error);
    ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    RefreshCharacterChoices();
    if (OldId != PreviewedCharacterId)
    {
        if (Selected) { VM->PreviewCharacterHero(Selected->HeroDefinitionId); }
        else { VM->ClearCharacterPreview(); }
    }
}
void UDivineBeastsCharacterSelectScreen::ClearCharacterChoices()
{
    for (UDivineBeastsCharacterChoiceEntry* Entry : ChoiceEntries) { if (Entry) { Entry->ResetChoice(); Entry->OnChoiceRequested.RemoveAll(this); } }
    ChoiceEntries.Reset();
    if (CharacterChoices) { CharacterChoices->ClearChildren(); }
}
void UDivineBeastsCharacterSelectScreen::RebuildCharacterChoices()
{
    ClearCharacterChoices();
    if (!CharacterChoices || !ChoiceEntryClass) { return; }
    for (const FString& Id : PresentedCharacterIds)
    {
        auto* Entry = CreateWidget<UDivineBeastsCharacterChoiceEntry>(GetOwningPlayer(), ChoiceEntryClass);
        if (!Entry) { continue; }
        Entry->ConfigureChoice(Id, {}, {}, {});
        Entry->OnChoiceRequested.AddUObject(this, &ThisClass::HandleChoiceRequested);
        auto* ChoiceSlot = CharacterChoices->AddChildToVerticalBox(Entry);
        ChoiceSlot->SetPadding(FMargin(0, 0, 0, 8));
        ChoiceEntries.Add(Entry);
    }
}
void UDivineBeastsCharacterSelectScreen::RefreshCharacterChoices()
{
    auto* VM = GetCharacterSelectViewModel();
    if (!VM) { return; }
    const auto& State = VM->GetStateRef();
    const FString SelectedId = PreviewedCharacterId;
    for (UDivineBeastsCharacterChoiceEntry* Entry : ChoiceEntries)
    {
        const FString& Id = Entry->GetChoiceIdentity();
        const auto* Item = State.Characters.FindByPredicate([&Id](const auto& Candidate) { return Candidate.CharacterId == Id; });
        if (!Item) { Entry->SetChoiceEnabled(false); continue; }
        const FText Hero = FDivineBeastsUILocalization::HeroNameToText(Item->HeroDefinitionId);
        FString Family, Epithet;
        Hero.ToString().Split(TEXT(" · "), &Family, &Epithet);
        Entry->ConfigureChoice(Id, Item->DisplayName, Hero, FText::FromString(Family.Right(1)));
        Entry->SetChoiceEnabled(Item->bEnabled && VM->CanSubmitSelection());
        Entry->SetChoiceSelected(Id == SelectedId);
        Entry->SetToolTipText(Item->bEnabled ? FText::GetEmpty() : Item->DisabledReason);
    }
    const auto* Item = State.Characters.FindByPredicate([&SelectedId](const auto& Candidate) { return Candidate.CharacterId == SelectedId; });
    if (SelectedHeroName) { SelectedHeroName->SetText(Item ? FDivineBeastsUILocalization::HeroNameToText(Item->HeroDefinitionId) : FText::GetEmpty()); }
    if (SelectedCharacterName) { SelectedCharacterName->SetText(Item ? Item->DisplayName : FText::GetEmpty()); }
}
void UDivineBeastsCharacterSelectScreen::HandleChoiceRequested(UDivineBeastsCharacterChoiceEntry* Entry)
{
    auto* VM = GetCharacterSelectViewModel();
    if (!Entry || !VM || !VM->CanSubmitSelection() || !ChoiceEntries.Contains(Entry)) { return; }
    const FString Id = Entry->GetChoiceIdentity();
    const auto* Item = VM->GetStateRef().Characters.FindByPredicate([&Id](const auto& Candidate) { return Candidate.CharacterId == Id; });
    if (!Item || !Item->bEnabled) { return; }
    PreviewedCharacterId = Id;
    VM->PreviewCharacterHero(Item->HeroDefinitionId);
    if (IsActivated()) { RefreshPresentation(); }
}
UWidget* UDivineBeastsCharacterSelectScreen::NativeGetDesiredFocusTarget() const
{
    // 空列表没有可选档案；创建入口可见、启用且允许键盘焦点时，优先返回它。
    if (const auto* VM = GetCharacterSelectViewModel(); VM && VM->GetStateRef().Characters.IsEmpty() &&
        OpenCharacterCreateButton && OpenCharacterCreateButton->GetIsEnabled() &&
        OpenCharacterCreateButton->IsVisible() && OpenCharacterCreateButton->GetIsFocusable())
    {
        return OpenCharacterCreateButton;
    }
    for (const UDivineBeastsCharacterChoiceEntry* Entry : ChoiceEntries)
    {
        if (Entry && Entry->GetChoiceIdentity() == PreviewedCharacterId) { return Entry->GetChoiceFocusTarget(); }
    }
    return Super::NativeGetDesiredFocusTarget();
}
void UDivineBeastsCharacterSelectScreen::HandleSelect()
{
    auto* VM = GetCharacterSelectViewModel();
    if (!VM) { return; }
    if (VM->GetStateRef().Characters.IsEmpty())
    {
        // 显式取消创建后仍可从空列表重新创建；不提交虚构的角色ID，不调用世界传输。
        if (VM->CanOpenCharacterCreation()) { VM->ShowCharacterEntryScreen(TEXT("UI.Screen.CharacterCreate")); }
        return;
    }
    if (!VM->CanSubmitSelection() || !PresentedCharacterIds.Contains(PreviewedCharacterId)) { return; }
    const FString Id = PreviewedCharacterId;
    const auto* Item = VM->GetStateRef().Characters.FindByPredicate([&Id](const auto& Candidate) { return Candidate.CharacterId == Id; });
    // 提交前重查资格，拒绝已被禁用或从当前快照移除的档案。
    if (Item && Item->bEnabled) { VM->SelectPersistentCharacter(Id); }
    if (IsActivated()) { RefreshPresentation(); }
}
void UDivineBeastsCharacterSelectScreen::HandleStateChanged(int32, int32) { RefreshPresentation(); }
void UDivineBeastsCharacterSelectScreen::HandleCommandCompleted(FGuid, FName) { RefreshPresentation(); }
// 转动只修改预览舞台；注销结束统一会话，不在页面中直接改变业务状态。
void UDivineBeastsCharacterSelectScreen::HandleOpenCharacterCreate() { if (auto* VM = GetCharacterSelectViewModel(); VM && VM->CanOpenCharacterCreation()) { VM->ShowCharacterEntryScreen(TEXT("UI.Screen.CharacterCreate")); } }
void UDivineBeastsCharacterSelectScreen::HandleLogout() { if (auto* VM = GetCharacterSelectViewModel()) { VM->Logout(); } }

// 鼠标拖动重用现有本地预览命令，忙碌时不接受交互。
void UDivineBeastsCharacterSelectScreen::RotatePreviewFromDrag(float DeltaYawDegrees)
{
    if (auto* VM = GetCharacterSelectViewModel(); VM && !VM->GetStateRef().bBusy) { VM->RotateCharacterPreview(DeltaYawDegrees); }
}

// 滚轮镜头调整只通过现有ViewModel（视图模型）命令链提交，忙碌页面不接受额外交互。
void UDivineBeastsCharacterSelectScreen::AdjustPreviewCameraDistanceFromWheel(
    float DeltaCentimeters)
{
    if (auto* VM = GetCharacterSelectViewModel(); VM && !VM->GetStateRef().bBusy)
    {
        VM->AdjustCharacterPreviewCameraDistance(DeltaCentimeters);
    }
}

void UDivineBeastsCharacterSelectScreen::AdjustPreviewCameraFocalLengthFromWheel(
    float DeltaMillimeters)
{
    if (auto* VM = GetCharacterSelectViewModel(); VM && !VM->GetStateRef().bBusy)
    {
        VM->AdjustCharacterPreviewCameraFocalLength(DeltaMillimeters);
    }
}
