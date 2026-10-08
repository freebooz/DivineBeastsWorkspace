#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"

#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"

UDivineBeastsCharacterCreateViewModel*
UDivineBeastsCharacterCreateScreen::GetCharacterCreateViewModel() const
{
    return Cast<UDivineBeastsCharacterCreateViewModel>(GetViewModel());
}

// 项目客户端角色创建适配：只保留未提交输入，资格和创建结果归应用流程/后端。
// 激活订阅、失活解绑；无业务Tick，不直接访问HTTP，不持有权威档案。
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Localization/DivineBeastsUILocalization.h"

void UDivineBeastsCharacterCreateScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    CreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreate);
    LogoutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogout);
    RotateLeftButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateLeft);
    RotateRightButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRotateRight);
    HeroOptions->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleHeroChanged);
    CharacterNameInput->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleNameChanged);
    if (auto* VM = GetCharacterCreateViewModel())
    {
        VM->OnViewStateChanged.AddUniqueDynamic(this, &ThisClass::HandleStateChanged);
        VM->OnCommandCompleted.AddUniqueDynamic(this, &ThisClass::HandleCommandCompleted);
    }
    RefreshPresentation();
}

void UDivineBeastsCharacterCreateScreen::NativeOnDeactivated()
{
    CreateButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCreate);
    LogoutButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLogout);
    RotateLeftButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRotateLeft);
    RotateRightButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRotateRight);
    HeroOptions->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleHeroChanged);
    CharacterNameInput->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleNameChanged);
    if (auto* VM = GetCharacterCreateViewModel())
    {
        VM->OnViewStateChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
        VM->OnCommandCompleted.RemoveDynamic(this, &ThisClass::HandleCommandCompleted);
    }
    Super::NativeOnDeactivated();
}

void UDivineBeastsCharacterCreateScreen::RefreshPresentation()
{
    auto* VM = GetCharacterCreateViewModel();
    if (!VM || bRefreshing) { return; }
    TGuardValue<bool> Guard(bRefreshing, true);
    const auto& State = VM->GetStateRef();
    TArray<FName> HeroIds;
    for (const auto& Item : State.CreateHeroOptions) { HeroIds.Add(Item.HeroDefinitionId); }
    // 只有资格列表变化才重建下拉框，避免状态刷新覆盖用户未提交的选择。
    if (HeroIds != PresentedHeroIds)
    {
        const int32 OldIndex = HeroOptions->GetSelectedIndex();
        const FName OldHero = PresentedHeroIds.IsValidIndex(OldIndex) ? PresentedHeroIds[OldIndex] : NAME_None;
        PresentedHeroIds = MoveTemp(HeroIds);
        HeroOptions->ClearOptions();
        for (FName Id : PresentedHeroIds) { HeroOptions->AddOption(FDivineBeastsUILocalization::HeroNameToText(Id).ToString()); }
        const int32 Index = PresentedHeroIds.Contains(OldHero) ? PresentedHeroIds.IndexOfByKey(OldHero) : 0;
        if (PresentedHeroIds.IsValidIndex(Index))
        {
            HeroOptions->SetSelectedIndex(Index);
            VM->PreviewCharacterHero(PresentedHeroIds[Index]);
        }
    }
    const bool bValidInput = PresentedHeroIds.IsValidIndex(HeroOptions->GetSelectedIndex())
        && !CharacterNameInput->GetText().ToString().TrimStartAndEnd().IsEmpty();
    CreateButton->SetIsEnabled(VM->CanSubmitCreation() && bValidInput);
    HeroOptions->SetIsEnabled(!State.bBusy);
    CharacterNameInput->SetIsEnabled(!State.bBusy);
    StatusText->SetText(FText::FromString(State.bBusy ? TEXT("正在创建角色，请稍候…")
        : (PresentedHeroIds.IsEmpty() ? TEXT("暂无可创建的英雄，请稍后重试。") : TEXT("选择英雄并输入名称，再点击创建角色。"))));
    FText Error = GetPageLoadError();
    if (Error.IsEmpty()) { Error = State.ErrorText; }
    if (Error.IsEmpty()) { Error = FDivineBeastsUILocalization::ErrorCodeToText(VM->GetLastCommandErrorCode()); }
    ErrorText->SetText(Error);
    ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void UDivineBeastsCharacterCreateScreen::HandleCreate()
{
    auto* VM = GetCharacterCreateViewModel();
    const int32 Index = HeroOptions->GetSelectedIndex();
    if (!VM || !VM->CanSubmitCreation() || !PresentedHeroIds.IsValidIndex(Index)) { return; }
    const FString Name = CharacterNameInput->GetText().ToString().TrimStartAndEnd();
    if (Name.IsEmpty()) { return; }
    // 提交前重新核对快照资格，名称与外观合法性仍由后端验证。
    const FName Id = PresentedHeroIds[Index];
    if (!VM->GetStateRef().CreateHeroOptions.ContainsByPredicate([Id](const auto& Item) { return Item.HeroDefinitionId == Id; })) { return; }
    VM->CreateCharacter(Id, Name, {});
    RefreshPresentation();
}
void UDivineBeastsCharacterCreateScreen::HandleHeroChanged(FString, ESelectInfo::Type)
{
    if (bRefreshing) { return; }
    const int32 Index = HeroOptions->GetSelectedIndex();
    if (auto* VM = GetCharacterCreateViewModel(); VM && PresentedHeroIds.IsValidIndex(Index))
    {
        // 预览只修改客户端表现，不能替代创建命令或推进业务流程。
        VM->PreviewCharacterHero(PresentedHeroIds[Index]);
    }
    RefreshPresentation();
}
void UDivineBeastsCharacterCreateScreen::HandleNameChanged(const FText&) { RefreshPresentation(); }
void UDivineBeastsCharacterCreateScreen::HandleStateChanged(int32, int32) { RefreshPresentation(); }
void UDivineBeastsCharacterCreateScreen::HandleCommandCompleted(FGuid, FName) { RefreshPresentation(); }
// 转动复用纯表现端口；注销复用统一命令，页面不访问网络。
void UDivineBeastsCharacterCreateScreen::HandleRotateLeft() { if (auto* VM = GetCharacterCreateViewModel()) { VM->RotateCharacterPreview(-30.0f); } }
void UDivineBeastsCharacterCreateScreen::HandleRotateRight() { if (auto* VM = GetCharacterCreateViewModel()) { VM->RotateCharacterPreview(30.0f); } }
void UDivineBeastsCharacterCreateScreen::HandleLogout() { if (auto* VM = GetCharacterCreateViewModel()) { VM->Logout(); } }
