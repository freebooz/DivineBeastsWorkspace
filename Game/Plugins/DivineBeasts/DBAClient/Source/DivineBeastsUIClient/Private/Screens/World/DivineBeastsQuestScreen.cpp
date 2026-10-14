#include "Screens/World/DivineBeastsQuestScreen.h"

#include "Engine/LocalPlayer.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

namespace
{
const TArray<FGamePlatformQuestSnapshot>& EmptyQuestSnapshots()
{
    static const TArray<FGamePlatformQuestSnapshot> Empty;
    return Empty;
}
}

TArray<FGamePlatformQuestSnapshot>
UDivineBeastsQuestScreen::GetQuestSnapshots() const
{
    return GetQuestSnapshotsView();
}

const TArray<FGamePlatformQuestSnapshot>&
UDivineBeastsQuestScreen::GetQuestSnapshotsView() const
{
    const UGamePlatformQuestClientSubsystem* Subsystem =
        ResolveQuestSubsystem();
    return IsValid(Subsystem)
        ? Subsystem->GetSortedSnapshotsView()
        : EmptyQuestSnapshots();
}

bool UDivineBeastsQuestScreen::IsQuestTracked(FName QuestId) const
{
    const UGamePlatformQuestClientSubsystem* Subsystem =
        ResolveQuestSubsystem();
    return IsValid(Subsystem) &&
        Subsystem->IsTracked(QuestId);
}

bool UDivineBeastsQuestScreen::TrackQuest(FName QuestId)
{
    UGamePlatformQuestClientSubsystem* Subsystem =
        ResolveQuestSubsystem();
    return IsValid(Subsystem) &&
        Subsystem->TrackQuest(QuestId);
}

bool UDivineBeastsQuestScreen::UntrackQuest(FName QuestId)
{
    UGamePlatformQuestClientSubsystem* Subsystem =
        ResolveQuestSubsystem();
    return IsValid(Subsystem) &&
        Subsystem->UntrackQuest(QuestId);
}

void UDivineBeastsQuestScreen::BindUIEvents()
{
    Super::BindUIEvents();
    if (auto* List = Cast<UComboBoxString>(GetWidgetFromName(TEXT("QuestList")))) List->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::QuestSelected);
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("TrackButton")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::TrackSelectedQuest);
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("UntrackButton")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::UntrackSelectedQuest);
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("CloseButton")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseQuestPage);

    QuestSubsystem = ResolveQuestSubsystem();
    if (IsValid(QuestSubsystem) &&
        !QuestChangedHandle.IsValid())
    {
        QuestChangedHandle =
            QuestSubsystem->OnChanged.AddUObject(
                this,
                &UDivineBeastsQuestScreen::HandleQuestChanged);
    }
}

void UDivineBeastsQuestScreen::UnbindUIEvents()
{
    if (auto* List = Cast<UComboBoxString>(GetWidgetFromName(TEXT("QuestList")))) List->OnSelectionChanged.RemoveAll(this);
    for (const TCHAR* Name : {TEXT("TrackButton"), TEXT("UntrackButton"), TEXT("CloseButton")})
        if (auto* Button = Cast<UButton>(GetWidgetFromName(Name))) Button->OnClicked.RemoveAll(this);
    QuestIds.Reset(); SelectedQuestId = NAME_None; QuestCommandFeedback=FText();
    if (IsValid(QuestSubsystem) &&
        QuestChangedHandle.IsValid())
    {
        QuestSubsystem->OnChanged.Remove(QuestChangedHandle);
        QuestChangedHandle.Reset();
    }
    QuestSubsystem = nullptr;

    Super::UnbindUIEvents();
}

void UDivineBeastsQuestScreen::RefreshInitialState()
{
    Super::RefreshInitialState();
    RefreshQuestPresentation();
    BP_OnQuestViewChanged();
}

void UDivineBeastsQuestScreen::HandleQuestChanged()
{
    RefreshQuestPresentation();
    BP_OnQuestViewChanged();
}

void UDivineBeastsQuestScreen::RefreshQuestPresentation()
{
    if (!IsActivated()) return;
    const auto* Source = ResolveQuestSubsystem();
    const auto& Values = GetQuestSnapshotsView();
    TGuardValue<bool> Guard(bRefreshingQuestList, true);
    QuestIds.Reset();
    auto* List = Cast<UComboBoxString>(GetWidgetFromName(TEXT("QuestList")));
    if (List) List->ClearOptions();
    for (const auto& Quest : Values)
    {
        QuestIds.Add(Quest.QuestId);
        // 当前平台合同没有本地化任务标题；明确显示任务身份，不生成假的剧情或目标。
        if (List) List->AddOption(Quest.QuestId.ToString());
    }
    if (!QuestIds.Contains(SelectedQuestId)) SelectedQuestId = QuestIds.IsEmpty() ? NAME_None : QuestIds[0];
    const int32 Index = QuestIds.IndexOfByKey(SelectedQuestId);
    if (List) { List->SetIsEnabled(!QuestIds.IsEmpty()); if (Index != INDEX_NONE) List->SetSelectedIndex(Index); }
    const bool bSelected = Index != INDEX_NONE;
    if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("QuestDetailText"))))
    {
        FString Detail=!Source ? TEXT("任务服务不可用；未加载任何任务。") : !bSelected ? TEXT("当前没有可展示的任务。") :
            SelectedQuestId.ToString() + (IsQuestTracked(SelectedQuestId) ? TEXT("\n已加入本地追踪；完成和奖励仍由服务器确认。") : TEXT("\n未追踪；可加入本地任务追踪。"));
        if(!QuestCommandFeedback.IsEmpty())Detail+=TEXT("\n")+QuestCommandFeedback.ToString();
        Text->SetText(FText::FromString(Detail));
    }
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("TrackButton")))) Button->SetIsEnabled(bSelected && !IsQuestTracked(SelectedQuestId));
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("UntrackButton")))) Button->SetIsEnabled(bSelected && IsQuestTracked(SelectedQuestId));
}
void UDivineBeastsQuestScreen::QuestSelected(FString, ESelectInfo::Type)
{
    if (bRefreshingQuestList || !IsActivated()) return;
    const auto* List = Cast<UComboBoxString>(GetWidgetFromName(TEXT("QuestList")));
    const int32 Index = List ? List->GetSelectedIndex() : INDEX_NONE;
    SelectedQuestId = QuestIds.IsValidIndex(Index) ? QuestIds[Index] : NAME_None;
    QuestCommandFeedback=FText();
    RefreshQuestPresentation();
}
void UDivineBeastsQuestScreen::TrackSelectedQuest()
{ if (IsActivated() && !SelectedQuestId.IsNone()) { const bool bAccepted=TrackQuest(SelectedQuestId);QuestCommandFeedback=FText::FromString(bAccepted?TEXT("本地追踪已更新。"):TEXT("无法追踪：任务已失效或超过追踪上限。"));RefreshQuestPresentation(); } }
void UDivineBeastsQuestScreen::UntrackSelectedQuest()
{ if (IsActivated() && !SelectedQuestId.IsNone()) { const bool bAccepted=UntrackQuest(SelectedQuestId);QuestCommandFeedback=FText::FromString(bAccepted?TEXT("本地追踪已取消。"):TEXT("取消未受理：该任务已不在当前追踪集合。"));RefreshQuestPresentation(); } }
void UDivineBeastsQuestScreen::CloseQuestPage() { if (IsActivated()) CloseScreen(); }

UGamePlatformQuestClientSubsystem*
UDivineBeastsQuestScreen::ResolveQuestSubsystem() const
{
    if (IsValid(QuestSubsystem))
    {
        return QuestSubsystem;
    }

    ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    return IsValid(LocalPlayer)
        ? LocalPlayer->GetSubsystem<UGamePlatformQuestClientSubsystem>()
        : nullptr;
}
