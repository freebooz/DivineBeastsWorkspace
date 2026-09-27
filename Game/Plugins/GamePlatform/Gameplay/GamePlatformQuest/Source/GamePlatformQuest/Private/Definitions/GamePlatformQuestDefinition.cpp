#include "Definitions/GamePlatformQuestDefinition.h"

bool UGamePlatformQuestDefinition::ValidateDefinition(FText& OutReason) const
{
    if (QuestId.IsNone() || Version <= 0 || Objectives.IsEmpty())
    {
        OutReason = FText::FromString(TEXT("QuestId/Version/Objectives非法"));
        return false;
    }

    if (RepeatPolicy == EGamePlatformQuestRepeatPolicy::PeriodicUnsupported ||
        TimeWindowPolicy == EGamePlatformQuestTimeWindowPolicy::PeriodicUnsupported)
    {
        OutReason = FText::FromString(TEXT("Periodic任务第一版未实现"));
        return false;
    }

    TSet<FName> ObjectiveIds;
    for (const FGamePlatformQuestObjectiveDefinition& Objective : Objectives)
    {
        if (Objective.ObjectiveId.IsNone() ||
            Objective.EventType.IsNone() ||
            !FMath::IsFinite(Objective.RequiredValue) ||
            Objective.RequiredValue <= 0.0 ||
            ObjectiveIds.Contains(Objective.ObjectiveId))
        {
            OutReason = FText::FromString(TEXT("Objective配置非法或重复"));
            return false;
        }

        if (Objective.ObjectiveType == EGamePlatformQuestObjectiveType::Region &&
            Objective.RequiredRegionId.IsNone())
        {
            OutReason = FText::FromString(TEXT("Region Objective缺少RegionId"));
            return false;
        }

        ObjectiveIds.Add(Objective.ObjectiveId);
    }

    for (const FGamePlatformQuestPrerequisite& Prerequisite : Prerequisites)
    {
        if (Prerequisite.CompletedQuestId.IsNone() ||
            Prerequisite.CompletedQuestId == QuestId)
        {
            OutReason = FText::FromString(TEXT("Prerequisite配置非法"));
            return false;
        }
    }

    OutReason = FText::GetEmpty();
    return true;
}
