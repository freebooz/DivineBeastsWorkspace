#include "Types/GamePlatformQuestStateMachine.h"

bool FGamePlatformQuestStateMachine::CanTransition(
    EGamePlatformQuestState From,
    EGamePlatformQuestState To)
{
    if (From == To)
    {
        return true;
    }

    switch (From)
    {
    case EGamePlatformQuestState::Locked:
        return To == EGamePlatformQuestState::Available;

    case EGamePlatformQuestState::Available:
        return To == EGamePlatformQuestState::Accepted ||
               To == EGamePlatformQuestState::Active;

    case EGamePlatformQuestState::Accepted:
        return To == EGamePlatformQuestState::Active ||
               To == EGamePlatformQuestState::Abandoned ||
               To == EGamePlatformQuestState::Expired;

    case EGamePlatformQuestState::Active:
        return To == EGamePlatformQuestState::ObjectivesCompleted ||
               To == EGamePlatformQuestState::CompletionPending ||
               To == EGamePlatformQuestState::Abandoned ||
               To == EGamePlatformQuestState::Failed ||
               To == EGamePlatformQuestState::Expired;

    case EGamePlatformQuestState::ObjectivesCompleted:
        return To == EGamePlatformQuestState::CompletionPending ||
               To == EGamePlatformQuestState::Failed;

    case EGamePlatformQuestState::CompletionPending:
        return To == EGamePlatformQuestState::Completed ||
               To == EGamePlatformQuestState::Failed;

    case EGamePlatformQuestState::Completed:
    case EGamePlatformQuestState::Abandoned:
    case EGamePlatformQuestState::Failed:
    case EGamePlatformQuestState::Expired:
    default:
        return false;
    }
}

bool FGamePlatformQuestStateMachine::TryTransition(
    EGamePlatformQuestState& InOutState,
    EGamePlatformQuestState To)
{
    if (!CanTransition(InOutState, To))
    {
        return false;
    }

    InOutState = To;
    return true;
}
