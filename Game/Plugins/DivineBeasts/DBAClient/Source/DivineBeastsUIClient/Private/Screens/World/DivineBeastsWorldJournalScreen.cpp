// SSLM世界手册视图：事件更新有界控件，地图使用既有组件；页面不直接读GAS或决定流程跳转。
#include "Screens/World/DivineBeastsWorldJournalScreen.h"
#include "ViewModels/World/DivineBeastsWorldJournalViewModel.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/DivineBeastsVillageMinimapWidget.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "GameFramework/Pawn.h"

UDivineBeastsWorldJournalViewModel* UDivineBeastsWorldJournalScreen::ResolveJournal() const
{ return Cast<UDivineBeastsWorldJournalViewModel>(GetViewModel()); }

void UDivineBeastsWorldJournalScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (!IsActivated()) return;
    CharacterTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowCharacter);
    MapQuestTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowMapQuest);
    SpellbookTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowSpellbook);
    PartyTab->OnClicked.AddUniqueDynamic(this, &ThisClass::ShowParty);
    TrackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::TrackSelected);
    UntrackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::UntrackSelected);
    CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::CloseJournal);
    EntryList->OnSelectionChanged.AddUniqueDynamic(this, &ThisClass::HandleEntrySelected);
    BoundJournal = ResolveJournal();
    if (BoundJournal) BoundJournal->BindLocalPlayer(GetOwningLocalPlayer());
    RefreshView();
}

void UDivineBeastsWorldJournalScreen::NativeOnDeactivated()
{
    CharacterTab->OnClicked.RemoveDynamic(this, &ThisClass::ShowCharacter);
    MapQuestTab->OnClicked.RemoveDynamic(this, &ThisClass::ShowMapQuest);
    SpellbookTab->OnClicked.RemoveDynamic(this, &ThisClass::ShowSpellbook);
    PartyTab->OnClicked.RemoveDynamic(this, &ThisClass::ShowParty);
    TrackButton->OnClicked.RemoveDynamic(this, &ThisClass::TrackSelected);
    UntrackButton->OnClicked.RemoveDynamic(this, &ThisClass::UntrackSelected);
    CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::CloseJournal);
    EntryList->OnSelectionChanged.RemoveDynamic(this, &ThisClass::HandleEntrySelected);
    if (BoundJournal) BoundJournal->UnbindLocalPlayer();
    BoundJournal = nullptr;
    if (WorldMap) WorldMap->BindToPawn(nullptr);
    DisplayedMapPawn.Reset(); SelectedKey = NAME_None; VisibleKeys.Reset();
    Super::NativeOnDeactivated();
}

void UDivineBeastsWorldJournalScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    if (IsActivated()) RefreshView();
}

void UDivineBeastsWorldJournalScreen::RefreshView()
{
    auto* VM = ResolveJournal();
    if (!VM || !EntryList || !SummaryText) return;
    SummaryText->SetText(GetPageLoadError().IsEmpty() ? VM->GetSummary() : GetPageLoadError());
    const auto Section = VM->GetSection();
    CharacterTab->SetIsEnabled(Section != EDivineBeastsWorldJournalSection::Character);
    MapQuestTab->SetIsEnabled(Section != EDivineBeastsWorldJournalSection::MapQuest);
    SpellbookTab->SetIsEnabled(Section != EDivineBeastsWorldJournalSection::Spellbook);
    PartyTab->SetIsEnabled(Section != EDivineBeastsWorldJournalSection::Party);
    {
        TGuardValue<bool> Guard(bRefreshingOptions, true);
        EntryList->ClearOptions(); VisibleKeys.Reset();
        for (const auto& Entry : VM->GetEntries()) { VisibleKeys.Add(Entry.Key); EntryList->AddOption(Entry.Label.ToString()); }
        const int32 PreviousIndex = VisibleKeys.Find(SelectedKey);
        const int32 Index = PreviousIndex != INDEX_NONE ? PreviousIndex : (VisibleKeys.IsEmpty() ? INDEX_NONE : 0);
        SelectedKey = VisibleKeys.IsValidIndex(Index) ? VisibleKeys[Index] : NAME_None;
        if (Index != INDEX_NONE) EntryList->SetSelectedIndex(Index);
        EntryList->SetIsEnabled(!VisibleKeys.IsEmpty());
    }
    if (WorldMap)
    {
        APawn* Pawn = Section == EDivineBeastsWorldJournalSection::MapQuest ? VM->GetDisplayPawn() : nullptr;
        if (DisplayedMapPawn.Get() != Pawn) { DisplayedMapPawn = Pawn; WorldMap->SetOwningPlayer(GetOwningPlayer()); WorldMap->BindToPawn(Pawn); }
        if (!Pawn) WorldMap->SetVisibility(ESlateVisibility::Collapsed);
    }
    RefreshSelection();
}

void UDivineBeastsWorldJournalScreen::RefreshSelection()
{
    const auto* VM = ResolveJournal();
    if (!VM) return;
    const auto* Entry = VM->GetEntries().FindByPredicate([this](const auto& Row) { return Row.Key == SelectedKey; });
    DetailText->SetText(Entry ? Entry->Details : FText::GetEmpty());
    const bool bQuest = VM->GetSection() == EDivineBeastsWorldJournalSection::MapQuest;
    TrackButton->SetVisibility(bQuest ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UntrackButton->SetVisibility(bQuest ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    TrackButton->SetIsEnabled(bQuest && Entry && !Entry->bTracked);
    UntrackButton->SetIsEnabled(bQuest && Entry && Entry->bTracked);
}

void UDivineBeastsWorldJournalScreen::HandleEntrySelected(FString /*Item*/, ESelectInfo::Type /*SelectionType*/)
{
    if (bRefreshingOptions) return;
    const int32 Index = EntryList->GetSelectedIndex();
    SelectedKey = VisibleKeys.IsValidIndex(Index) ? VisibleKeys[Index] : NAME_None;
    RefreshSelection();
}
void UDivineBeastsWorldJournalScreen::ShowCharacter() { if (auto* VM = ResolveJournal()) VM->SelectSection(EDivineBeastsWorldJournalSection::Character); }
void UDivineBeastsWorldJournalScreen::ShowMapQuest() { if (auto* VM = ResolveJournal()) VM->SelectSection(EDivineBeastsWorldJournalSection::MapQuest); }
void UDivineBeastsWorldJournalScreen::ShowSpellbook() { if (auto* VM = ResolveJournal()) VM->SelectSection(EDivineBeastsWorldJournalSection::Spellbook); }
void UDivineBeastsWorldJournalScreen::ShowParty() { if (auto* VM = ResolveJournal()) VM->SelectSection(EDivineBeastsWorldJournalSection::Party); }
void UDivineBeastsWorldJournalScreen::TrackSelected() { if (auto* VM = ResolveJournal()) VM->SetQuestTracked(SelectedKey, true); }
void UDivineBeastsWorldJournalScreen::UntrackSelected() { if (auto* VM = ResolveJournal()) VM->SetQuestTracked(SelectedKey, false); }
void UDivineBeastsWorldJournalScreen::CloseJournal()
{
    if (auto* Player = GetOwningLocalPlayer()) if (auto* UI = Player->GetSubsystem<UDivineBeastsUIClientSubsystem>()) UI->CloseSystemMenu();
}
bool UDivineBeastsWorldJournalScreen::NativeOnHandleBackAction() { CloseJournal(); return true; }
FReply UDivineBeastsWorldJournalScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape) { CloseJournal(); return FReply::Handled(); }
    if (Event.GetKey() == EKeys::C) { ShowCharacter(); return FReply::Handled(); }
    if (Event.GetKey() == EKeys::M || Event.GetKey() == EKeys::L) { ShowMapQuest(); return FReply::Handled(); }
    if (Event.GetKey() == EKeys::K) { ShowSpellbook(); return FReply::Handled(); }
    if (Event.GetKey() == EKeys::P) { ShowParty(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}
