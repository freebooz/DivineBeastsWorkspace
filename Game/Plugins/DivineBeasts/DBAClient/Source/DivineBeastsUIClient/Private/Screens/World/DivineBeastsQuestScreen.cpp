#include "Screens/World/DivineBeastsQuestScreen.h"

#include "Engine/LocalPlayer.h"

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
    BP_OnQuestViewChanged();
}

void UDivineBeastsQuestScreen::HandleQuestChanged()
{
    BP_OnQuestViewChanged();
}

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
