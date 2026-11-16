#include "Screens/Characters/DivineBeastsCharacterCreateScreen.h"
#include "Styling/DivineBeastsUIStyleBindings.h"
#include "Routing/DivineBeastsUIRoutingPolicy.h"

#include "ViewModels/Characters/DivineBeastsCharacterCreateViewModel.h"
#include "Components/TextBlock.h"
#include "Async/Async.h"
#include "UObject/StrongObjectPtr.h"

// 默认主题语义只保存在项目层CDO：当项目主题ID为空时平台兼容绑定保持旧Widget样式。
// 不修改已有Widget树、命名控件、焦点、登录输入/命令或加载任何项目纹理。
UDivineBeastsCharacterCreateScreen::UDivineBeastsCharacterCreateScreen()
{
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("CreateButton"), DivineBeasts::UI::Styling::Keys::ButtonPrimary, EGamePlatformUIStyleKind::Button));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("BackToCharacterSelectButton"), DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LogoutButton"), DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("SelectedHeroName"), DivineBeasts::UI::Styling::Keys::TextTitle, EGamePlatformUIStyleKind::Text, true));
    // 旧UMG UButton不管理内部文本样式；命名子TextBlock按相同正文视觉单独可选适配。
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("CreateButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LogoutButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("BackToCharacterSelectButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
}

UDivineBeastsCharacterCreateViewModel*
UDivineBeastsCharacterCreateScreen::GetCharacterCreateViewModel() const
{
    return Cast<UDivineBeastsCharacterCreateViewModel>(GetViewModel());
}

void UDivineBeastsCharacterCreateScreen::BindUIEvents()
{
    Super::BindUIEvents();
    if (!IsActivated()) return;
    ++PresentationRefreshGeneration;
    bPresentationRefreshPending = false;
    bPresentationRefreshQueued = false;
    bHeroOverviewEventsBound = true;
    ReconcileHeroOverviewBinding();
}

void UDivineBeastsCharacterCreateScreen::ReleaseHeroOverviewBinding()
{
    UDivineBeastsCharacterCreateViewModel* Previous = BoundHeroOverviewViewModel;
    const FDelegateHandle PreviousHandle = HeroOverviewHandle;
    BoundHeroOverviewViewModel = nullptr;
    BoundHeroOverviewPageGeneration = 0;
    HeroOverviewHandle.Reset();
    if (Previous && PreviousHandle.IsValid()) Previous->OnHeroOverviewChanged().Remove(PreviousHandle);
}

void UDivineBeastsCharacterCreateScreen::UnbindUIEvents()
{
    // 先使已排队的弱任务失效，避免旧页补刷在失活/重新激活后误操作新页。
    ++PresentationRefreshGeneration;
    bPresentationRefreshPending = false;
    bPresentationRefreshQueued = false;
    bHeroOverviewEventsBound = false;
    ReleaseHeroOverviewBinding();
    ApplyHeroOverviewText({});
    Super::UnbindUIEvents();
}

bool UDivineBeastsCharacterCreateScreen::ReconcileHeroOverviewBinding()
{
    if (!IsActivated() || !bHeroOverviewEventsBound) return false;
    auto* Current = GetCharacterCreateViewModel();
    if (!Current || !Current->IsPageActive())
    {
        ReleaseHeroOverviewBinding();
        ApplyHeroOverviewText({});
        return false;
    }
    if (BoundHeroOverviewViewModel != Current || BoundHeroOverviewPageGeneration != Current->GetPageGeneration())
    {
        ReleaseHeroOverviewBinding();
        BoundHeroOverviewViewModel = Current;
        BoundHeroOverviewPageGeneration = Current->GetPageGeneration();
        HeroOverviewHandle = Current->OnHeroOverviewChanged().AddUObject(this, &ThisClass::HandleHeroOverviewChanged);
    }
    return true;
}

void UDivineBeastsCharacterCreateScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    if (ReconcileHeroOverviewBinding()) RefreshPresentation();
}

void UDivineBeastsCharacterCreateScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    if (ReconcileHeroOverviewBinding()) RefreshPresentation();
}

void UDivineBeastsCharacterCreateScreen::HandleHeroOverviewChanged(const FDivineBeastsHeroOverview& Overview)
{
    const auto* VM = BoundHeroOverviewViewModel.Get();
    if (!IsActivated() || !bHeroOverviewEventsBound || !VM || VM != GetCharacterCreateViewModel() || !VM->IsPageActive() ||
        BoundHeroOverviewPageGeneration != VM->GetPageGeneration() || Overview.PageGeneration != VM->GetPageGeneration()) return;
    // 更早的事件观察者可同步切英雄；该旧事件即使仍在当前广播栈，也不得覆盖后继文本。
    const auto Current = VM->GetHeroOverview();
    if (Overview.RequestGeneration != Current.RequestGeneration || Overview.HeroDefinitionId != Current.HeroDefinitionId) return;
    ApplyHeroOverviewText(Overview);
}

void UDivineBeastsCharacterCreateScreen::ApplyHeroOverviewText(const FDivineBeastsHeroOverview& Overview)
{
    // 所有正文/草案状态来自VM值快照；控件固定字体、滚动布局和标题由Monolith资产管理。
    if (HeroOverviewStatusText) HeroOverviewStatusText->SetText(Overview.StatusText);
    if (HeroIntroductionText) HeroIntroductionText->SetText(Overview.Introduction);
    if (SkillIntroductionText) SkillIntroductionText->SetText(Overview.SkillIntroductionText);
}

// 项目客户端角色创建适配：只保留未提交输入，资格和创建结果归应用流程/后端。
// 激活订阅、失活解绑；无业务Tick，不直接访问HTTP，不持有权威档案。
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "Components/WrapBox.h"
#include "Screens/Characters/DivineBeastsCharacterChoiceEntry.h"

void UDivineBeastsCharacterCreateScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (!IsActivated()) return;
    CreateButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCreate);
    LogoutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogout);
    if (BackToCharacterSelectButton) { BackToCharacterSelectButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBackToCharacterSelect); }
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
    if (BackToCharacterSelectButton) { BackToCharacterSelectButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBackToCharacterSelect); }
    HeroOptions->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleHeroChanged);
    CharacterNameInput->OnTextChanged.RemoveDynamic(this, &ThisClass::HandleNameChanged);
    if (auto* VM = GetCharacterCreateViewModel())
    {
        VM->OnViewStateChanged.RemoveDynamic(this, &ThisClass::HandleStateChanged);
        VM->OnCommandCompleted.RemoveDynamic(this, &ThisClass::HandleCommandCompleted);
    }
    ClearHeroChoices();
    PresentedHeroIds.Reset();
    Super::NativeOnDeactivated();
}

void UDivineBeastsCharacterCreateScreen::RefreshPresentation()
{
    if (!IsActivated() || !bHeroOverviewEventsBound) return;
    if (bRefreshing)
    {
        // Loading/BeginPage的同步观察者可能换VM；保存意图而非吞掉新VM的初刷。
        bPresentationRefreshPending = true;
        return;
    }
    const TStrongObjectPtr<UDivineBeastsCharacterCreateScreen> KeepScreen(this);
    {
        TGuardValue<bool> Guard(bRefreshing, true);
        // 最多两轮保证普通同步换VM立即可见；更多外部重入必须让出调用栈，不形成无限递归。
        for (int32 Pass = 0; Pass < 2 && IsActivated() && bHeroOverviewEventsBound; ++Pass)
        {
            bPresentationRefreshPending = false;
            const TStrongObjectPtr<UDivineBeastsCharacterCreateViewModel> Current(GetCharacterCreateViewModel());
            if (!Current.IsValid() || !Current->IsPageActive()) break;
            const int32 ExpectedPageGeneration = Current->GetPageGeneration();
            const uint64 ExpectedRefreshGeneration = PresentationRefreshGeneration;
            RefreshPresentationPass(Current.Get(), ExpectedPageGeneration);
            if (IsActivated() && bHeroOverviewEventsBound &&
                (GetCharacterCreateViewModel() != Current.Get() || !Current->IsPageActive() ||
                    Current->GetPageGeneration() != ExpectedPageGeneration || PresentationRefreshGeneration != ExpectedRefreshGeneration))
                bPresentationRefreshPending = true;
            if (!bPresentationRefreshPending) break;
        }
    }
    // 退出旧guard后再排队；仅存在真实重入意图才调度，不轮询业务状态。
    QueuePendingPresentationRefresh();
}

void UDivineBeastsCharacterCreateScreen::QueuePendingPresentationRefresh()
{
    if (!bPresentationRefreshPending || bPresentationRefreshQueued || !IsActivated() || !bHeroOverviewEventsBound) return;
    bPresentationRefreshQueued = true;
    const uint64 ExpectedRefreshGeneration = PresentationRefreshGeneration;
    const TWeakObjectPtr<UDivineBeastsCharacterCreateScreen> WeakThis(this);
    AsyncTask(ENamedThreads::GameThread, [WeakThis, ExpectedRefreshGeneration]()
    {
        auto* Self = WeakThis.Get();
        if (!Self || Self->PresentationRefreshGeneration != ExpectedRefreshGeneration) return;
        Self->bPresentationRefreshQueued = false;
        if (!Self->IsActivated() || !Self->bHeroOverviewEventsBound)
        {
            Self->bPresentationRefreshPending = false;
            return;
        }
        // 不捕获旧VM或正文；始终重新读取当前选择/页面代次，已被同步补刷消费则不再执行。
        if (Self->bPresentationRefreshPending) Self->RefreshPresentation();
    });
}

void UDivineBeastsCharacterCreateScreen::RefreshPresentationPass(
    UDivineBeastsCharacterCreateViewModel* VM, int32 ExpectedPageGeneration)
{
    const uint64 ExpectedRefreshGeneration = PresentationRefreshGeneration;
    const auto IsCurrent = [this, VM, ExpectedPageGeneration, ExpectedRefreshGeneration]()
    {
        return IsActivated() && bHeroOverviewEventsBound && PresentationRefreshGeneration == ExpectedRefreshGeneration &&
            GetCharacterCreateViewModel() == VM && VM->IsPageActive() && VM->GetPageGeneration() == ExpectedPageGeneration;
    };
    if (!IsCurrent()) return;
    // 当前业务值快照属于本轮，不能持有会被同步新状态原地改写的引用。
    const auto State = VM->GetState();
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
            if (!IsCurrent()) return;
            VM->PreviewCharacterHero(PresentedHeroIds[Index]);
            if (!IsCurrent()) return;
        }
        RebuildHeroChoices();
        if (!IsCurrent()) return;
    }
    const bool bValidInput = PresentedHeroIds.IsValidIndex(HeroOptions->GetSelectedIndex())
        && !CharacterNameInput->GetText().ToString().TrimStartAndEnd().IsEmpty();
    CreateButton->SetIsEnabled(VM->CanSubmitCreation() && bValidInput);
    // 取消草稿仅导航回空/非空角色选择页，不借用业务角色提交资格；同时保留认证/忙碌/错误门禁。
    if (BackToCharacterSelectButton)
    {
        BackToCharacterSelectButton->SetIsEnabled(FDivineBeastsUIRoutingPolicy::CanNavigateCharacterEntry(
            State, TEXT("UI.Screen.CharacterSelect")));
    }
    HeroOptions->SetIsEnabled(!State.bBusy);
    CharacterNameInput->SetIsEnabled(!State.bBusy);
    StatusText->SetText(FText::FromString(State.bBusy ? TEXT("正在创建角色，请稍候…")
        : (PresentedHeroIds.IsEmpty() ? TEXT("暂无可创建的英雄，请稍后重试。") : TEXT("选择英雄并输入名称，再点击创建角色。"))));
    FText Error = GetPageLoadError();
    if (Error.IsEmpty()) { Error = State.ErrorText; }
    if (Error.IsEmpty()) { Error = FDivineBeastsUILocalization::ErrorCodeToText(VM->GetLastCommandErrorCode()); }
    ErrorText->SetText(Error);
    ErrorText->SetVisibility(Error.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    RefreshHeroChoices();
    if (!IsCurrent()) return;
    if (ReconcileHeroOverviewBinding() && IsCurrent())
    {
        const int32 Index = HeroOptions->GetSelectedIndex();
        const FName HeroId = PresentedHeroIds.IsValidIndex(Index) ? PresentedHeroIds[Index] : NAME_None;
        // 本地浏览命令由VM拥有租约；Widget从不读取DataAsset或直接调用Data/HTTP。
        if (!HeroId.IsNone()) VM->SelectHeroOverview(HeroId);
        else if (!VM->GetHeroOverview().HeroDefinitionId.IsNone()) VM->ClearHeroOverview();
        // 活动页换VM且同英雄已Ready时可能无需新事件，补一次当前快照投影。
        if (IsCurrent()) HandleHeroOverviewChanged(VM->GetHeroOverview());
    }
}

void UDivineBeastsCharacterCreateScreen::ClearHeroChoices()
{
    // 先关闭输入并解除意图订阅，再移除视觉树；旧条目不能操作新页面。
    for (UDivineBeastsCharacterChoiceEntry* Entry : ChoiceEntries) { if (Entry) { Entry->ResetChoice(); Entry->OnChoiceRequested.RemoveAll(this); } }
    ChoiceEntries.Reset();
    if (HeroChoices) { HeroChoices->ClearChildren(); }
}
void UDivineBeastsCharacterCreateScreen::RebuildHeroChoices()
{
    ClearHeroChoices();
    if (!HeroChoices || !ChoiceEntryClass) { return; }
    for (const FName Id : PresentedHeroIds)
    {
        auto* Entry = CreateWidget<UDivineBeastsCharacterChoiceEntry>(GetOwningPlayer(), ChoiceEntryClass);
        if (!Entry) { continue; }
        const FText Name = FDivineBeastsUILocalization::HeroNameToText(Id);
        FString Family, Epithet;
        Name.ToString().Split(TEXT(" · "), &Family, &Epithet);
        Entry->ConfigureChoice(Id.ToString(), Name, {}, FText::FromString(Family.Right(1)));
        Entry->OnChoiceRequested.AddUObject(this, &ThisClass::HandleChoiceRequested);
        HeroChoices->AddChildToWrapBox(Entry);
        ChoiceEntries.Add(Entry);
    }
}
void UDivineBeastsCharacterCreateScreen::RefreshHeroChoices()
{
    auto* VM = GetCharacterCreateViewModel();
    if (!VM) { return; }
    const int32 Selected = HeroOptions->GetSelectedIndex();
    const FName Id = PresentedHeroIds.IsValidIndex(Selected) ? PresentedHeroIds[Selected] : NAME_None;
    // 只有真实磁贴全部生成成功才隐藏兼容控件，资源缺失时保留可操作回退。
    const bool bHasChoices = !ChoiceEntries.IsEmpty() && ChoiceEntries.Num() == PresentedHeroIds.Num();
    HeroOptions->SetVisibility(bHasChoices ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    for (UDivineBeastsCharacterChoiceEntry* Entry : ChoiceEntries)
    {
        Entry->SetChoiceEnabled(!VM->GetStateRef().bBusy);
        Entry->SetChoiceSelected(Entry->GetChoiceIdentity() == Id.ToString());
    }
    if (SelectedHeroName) { SelectedHeroName->SetText(Id.IsNone() ? FText::GetEmpty() : FDivineBeastsUILocalization::HeroNameToText(Id)); }
}
void UDivineBeastsCharacterCreateScreen::HandleChoiceRequested(UDivineBeastsCharacterChoiceEntry* Entry)
{
    auto* VM = GetCharacterCreateViewModel();
    if (!Entry || !VM || VM->GetStateRef().bBusy) { return; }
    const FName Id(*Entry->GetChoiceIdentity());
    if (!VM->GetStateRef().CreateHeroOptions.ContainsByPredicate([Id](const auto& Item) { return Item.HeroDefinitionId == Id; })) { return; }
    const int32 Index = PresentedHeroIds.IndexOfByKey(Id);
    if (Index != INDEX_NONE) { HeroOptions->SetSelectedIndex(Index); }
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
void UDivineBeastsCharacterCreateScreen::HandleBackToCharacterSelect() { if (auto* VM = GetCharacterCreateViewModel()) { VM->ShowCharacterEntryScreen(TEXT("UI.Screen.CharacterSelect")); } }
void UDivineBeastsCharacterCreateScreen::HandleLogout() { if (auto* VM = GetCharacterCreateViewModel()) { VM->Logout(); } }

// 鼠标拖动重用现有本地预览命令，忙碌时不接受交互。
void UDivineBeastsCharacterCreateScreen::RotatePreviewFromDrag(float DeltaYawDegrees)
{
    if (auto* VM = GetCharacterCreateViewModel(); VM && !VM->GetStateRef().bBusy) { VM->RotateCharacterPreview(DeltaYawDegrees); }
}

// 滚轮镜头调整只通过现有ViewModel（视图模型）命令链提交，忙碌页面不接受额外交互。
void UDivineBeastsCharacterCreateScreen::AdjustPreviewCameraDistanceFromWheel(
    float DeltaCentimeters)
{
    if (auto* VM = GetCharacterCreateViewModel(); VM && !VM->GetStateRef().bBusy)
    {
        VM->AdjustCharacterPreviewCameraDistance(DeltaCentimeters);
    }
}

void UDivineBeastsCharacterCreateScreen::AdjustPreviewCameraFocalLengthFromWheel(
    float DeltaMillimeters)
{
    if (auto* VM = GetCharacterCreateViewModel(); VM && !VM->GetStateRef().bBusy)
    {
        VM->AdjustCharacterPreviewCameraFocalLength(DeltaMillimeters);
    }
}
