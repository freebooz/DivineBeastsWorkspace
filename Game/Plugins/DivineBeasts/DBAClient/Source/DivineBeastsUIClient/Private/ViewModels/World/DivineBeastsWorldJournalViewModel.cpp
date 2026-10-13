// SSLM公共世界手册：读取已授权角色/技能/任务，命令仅修改本地追踪偏好；不派生MOBA依赖。
#include "ViewModels/World/DivineBeastsWorldJournalViewModel.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "ViewModels/Combat/DivineBeastsPlayerStatusViewModel.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Services/GamePlatformQuestClientSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/LocalPlayer.h"

#define LOCTEXT_NAMESPACE "DivineBeastsWorldJournal"

bool UDivineBeastsWorldJournalViewModel::BindLocalPlayer(ULocalPlayer* Player)
{
    UnbindLocalPlayer();
    APlayerController* Controller = Player ? Player->GetPlayerController(Player->GetWorld()) : nullptr;
    if (!IsValid(Controller) || Controller->GetLocalPlayer() != Player || !Controller->IsLocalController()) return false;
    BoundController = Controller;
    bSourceBound = true;
    Abilities = NewObject<UDivineBeastsAbilityBarViewModel>(this);
    PlayerStatus = NewObject<UDivineBeastsPlayerStatusViewModel>(this);
    Abilities->OnSlotsChanged().AddUObject(this, &ThisClass::HandleSlotsChanged);
    PlayerStatus->OnStatusChanged().AddUObject(this, &ThisClass::HandleStatusChanged);
    QuestSource = Player->GetSubsystem<UGamePlatformQuestClientSubsystem>();
    if (QuestSource.IsValid()) QuestHandle = QuestSource->OnChanged.AddUObject(this, &ThisClass::RefreshEntries);
    Controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &ThisClass::HandlePawnChanged);
    HandlePawnChanged(nullptr, Controller->GetPawn());
    return true;
}

void UDivineBeastsWorldJournalViewModel::UnbindLocalPlayer()
{
    bSourceBound = false;
    if (BoundController.IsValid()) BoundController->OnPossessedPawnChanged.RemoveDynamic(this, &ThisClass::HandlePawnChanged);
    if (QuestSource.IsValid()) QuestSource->OnChanged.Remove(QuestHandle);
    if (Abilities) { Abilities->OnSlotsChanged().RemoveAll(this); Abilities->UnbindFromLoadout(); }
    if (PlayerStatus) { PlayerStatus->OnStatusChanged().RemoveAll(this); PlayerStatus->UnbindFromAbilitySystem(); }
    BoundController.Reset(); QuestSource.Reset(); QuestHandle.Reset();
    Abilities = nullptr; PlayerStatus = nullptr; Entries.Reset(); bStatusAvailable = false;
}

void UDivineBeastsWorldJournalViewModel::BeginDestroy()
{
    UnbindLocalPlayer();
    Super::BeginDestroy();
}

APawn* UDivineBeastsWorldJournalViewModel::GetDisplayPawn() const
{
    return bSourceBound && BoundController.IsValid() ? BoundController->GetPawn() : nullptr;
}

void UDivineBeastsWorldJournalViewModel::HandlePawnChanged(APawn* /*Previous*/, APawn* Current)
{
    if (!bSourceBound || !BoundController.IsValid() || Current != BoundController->GetPawn()) return;
    bStatusAvailable = PlayerStatus && PlayerStatus->BindToAbilitySystem(
        Current ? Current->FindComponentByClass<UGamePlatformAbilitySystemComponent>() : nullptr);
    if (Abilities) Abilities->BindToLoadout(Current ? Current->FindComponentByClass<UDivineBeastsAbilityLoadoutComponent>() : nullptr);
    RefreshEntries();
}

void UDivineBeastsWorldJournalViewModel::HandleSlotsChanged(const TArray<FGamePlatformUISlotState>& /*Slots*/) { RefreshEntries(); }
void UDivineBeastsWorldJournalViewModel::HandleStatusChanged(const FDivineBeastsPlayerStatusViewData& /*Status*/)
{
    if (bSourceBound) MarkStateChanged();
}

bool UDivineBeastsWorldJournalViewModel::SelectSection(EDivineBeastsWorldJournalSection InSection)
{
    if (static_cast<uint8>(InSection) > static_cast<uint8>(EDivineBeastsWorldJournalSection::Party)) return false;
    Section = InSection;
    RefreshEntries();
    return true;
}

void UDivineBeastsWorldJournalViewModel::RefreshEntries()
{
    if (!bSourceBound) return;
    Entries.Reset();
    if (Section == EDivineBeastsWorldJournalSection::Spellbook && Abilities)
    {
        for (const auto& Skill : Abilities->GetSlotDetails())
        {
            FDivineBeastsWorldJournalEntry Entry;
            Entry.Key = Skill.SlotId;
            Entry.Label = Skill.DisplayName.IsEmpty() ? LOCTEXT("SkillTextPending", "技能名称尚未加载") : Skill.DisplayName;
            Entry.Details = FText::Format(LOCTEXT("SkillDetails", "{0}\n等级：{1}\n{2}\n{3}"),
                Entry.Label, FText::AsNumber(Skill.AbilityLevel), Skill.Description, Skill.DisabledReason);
            Entries.Add(MoveTemp(Entry));
        }
    }
    else if (Section == EDivineBeastsWorldJournalSection::MapQuest && QuestSource.IsValid())
    {
        for (const auto& Quest : QuestSource->GetSortedSnapshotsView())
        {
            FDivineBeastsWorldJournalEntry Entry;
            Entry.Key = Quest.QuestId;
            // 当前任务快照不含本地化标题；不将内部任务ID伪装成最终文案，也不编造剧情文本。
            Entry.Label = FText::Format(LOCTEXT("QuestPendingTitle", "任务 {0}（标题未提供）"), FText::AsNumber(Entries.Num() + 1));
            Entry.bTracked = QuestSource->IsTracked(Quest.QuestId);
            FString Details = Entry.Label.ToString();
            for (int32 Index = 0; Index < Quest.Objectives.Num(); ++Index)
            {
                const auto& Objective = Quest.Objectives[Index];
                Details += TEXT("\n") + FText::Format(LOCTEXT("QuestObjective", "目标 {0}：{1} / {2}"),
                    FText::AsNumber(Index + 1), FText::AsNumber(Objective.CurrentValue), FText::AsNumber(Objective.RequiredValue)).ToString();
            }
            Entry.Details = FText::FromString(Details);
            Entries.Add(MoveTemp(Entry));
        }
    }
    MarkStateChanged();
}

FText UDivineBeastsWorldJournalViewModel::GetSummary() const
{
    if (!bSourceBound) return LOCTEXT("NoSource", "当前角色数据不可用。");
    if (Section == EDivineBeastsWorldJournalSection::Party)
        return LOCTEXT("NoPartyService", "公共组队服务尚未接通。此处不会生成模拟队友；邀请、离队和队长操作暂不可用。");
    if (Section == EDivineBeastsWorldJournalSection::Spellbook)
        return Entries.IsEmpty() ? LOCTEXT("NoSkills", "尚未收到服务器授权的技能。") : LOCTEXT("SkillHelp", "选择技能查看说明。此处不授予技能；快捷栏与数字键仍使用原有授权输入。");
    if (Section == EDivineBeastsWorldJournalSection::MapQuest)
        return LOCTEXT("MapHelp", "地图仅显示已有底图的当前区域。选择任务可切换本地追踪；追踪不代表完成或领奖。");
    if (!bStatusAvailable || !PlayerStatus) return LOCTEXT("NoAttributes", "角色属性尚未就绪。");
    const auto& Status = PlayerStatus->GetStatusRef();
    FText Name = LOCTEXT("CurrentCharacter", "当前角色");
    for (const auto& Character : GetStateRef().Characters)
        if (Character.CharacterId == GetStateRef().SelectedCharacterId) { Name = Character.DisplayName; break; }
    return FText::Format(LOCTEXT("CharacterSummary", "{0}\n生命：{1} / {2}\n气势：{3} / {4}\n{5}"), Name,
        FText::AsNumber(Status.Health), FText::AsNumber(Status.MaxHealth),
        FText::AsNumber(Status.Momentum), FText::AsNumber(Status.MaxMomentum),
        Status.bDead ? LOCTEXT("Dead", "状态：死亡") : LOCTEXT("Alive", "状态：存活"));
}

bool UDivineBeastsWorldJournalViewModel::SetQuestTracked(FName QuestId, bool bTracked)
{
    if (!bSourceBound || Section != EDivineBeastsWorldJournalSection::MapQuest || !QuestSource.IsValid() || !QuestSource->FindQuest(QuestId)) return false;
    return bTracked ? QuestSource->TrackQuest(QuestId) : QuestSource->UntrackQuest(QuestId);
}

#undef LOCTEXT_NAMESPACE
