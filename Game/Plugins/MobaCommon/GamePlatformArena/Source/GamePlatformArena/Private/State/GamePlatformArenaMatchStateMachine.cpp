#include "State/GamePlatformArenaMatchStateMachine.h"

bool FGamePlatformArenaMatchStateMachine::CanTransition(
    EGamePlatformArenaMatchPhase From,
    EGamePlatformArenaMatchPhase To)
{
    if (From == To) { return false; }
    if (To == EGamePlatformArenaMatchPhase::Failed)
    {
        return From != EGamePlatformArenaMatchPhase::Completed &&
               From != EGamePlatformArenaMatchPhase::Failed;
    }

    switch (From)
    {
    case EGamePlatformArenaMatchPhase::Uninitialized:
        return To == EGamePlatformArenaMatchPhase::WaitingAssignment;
    case EGamePlatformArenaMatchPhase::WaitingAssignment:
        return To == EGamePlatformArenaMatchPhase::Preparing;
    case EGamePlatformArenaMatchPhase::Preparing:
        return To == EGamePlatformArenaMatchPhase::WaitingPlayers;
    case EGamePlatformArenaMatchPhase::WaitingPlayers:
        return To == EGamePlatformArenaMatchPhase::HeroSelection || To == EGamePlatformArenaMatchPhase::Aborted;
    case EGamePlatformArenaMatchPhase::HeroSelection:
        return To == EGamePlatformArenaMatchPhase::ReadyCheck || To == EGamePlatformArenaMatchPhase::Aborted;
    case EGamePlatformArenaMatchPhase::ReadyCheck:
        return To == EGamePlatformArenaMatchPhase::Countdown || To == EGamePlatformArenaMatchPhase::Aborted;
    case EGamePlatformArenaMatchPhase::Countdown:
        return To == EGamePlatformArenaMatchPhase::InProgress || To == EGamePlatformArenaMatchPhase::Aborted;
    case EGamePlatformArenaMatchPhase::InProgress:
        return To == EGamePlatformArenaMatchPhase::Ending || To == EGamePlatformArenaMatchPhase::Aborted;
    case EGamePlatformArenaMatchPhase::Ending:
        return To == EGamePlatformArenaMatchPhase::ResultPending || To == EGamePlatformArenaMatchPhase::Completed;
    case EGamePlatformArenaMatchPhase::ResultPending:
        return To == EGamePlatformArenaMatchPhase::Completed;
    default:
        return false;
    }
}

bool FGamePlatformArenaMatchStateMachine::TryTransition(
    EGamePlatformArenaMatchPhase NewPhase,
    FString& OutError)
{
    if (!CanTransition(Phase, NewPhase))
    {
        OutError = FString::Printf(
            TEXT("非法竞技阶段转换：%d -> %d"),
            static_cast<int32>(Phase),
            static_cast<int32>(NewPhase));
        return false;
    }

    Phase = NewPhase;
    ++Revision;
    OutError.Reset();
    return true;
}
