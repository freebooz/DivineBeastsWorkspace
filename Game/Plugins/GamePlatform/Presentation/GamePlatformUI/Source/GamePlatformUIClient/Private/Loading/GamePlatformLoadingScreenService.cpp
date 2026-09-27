#include "Loading/GamePlatformLoadingScreenService.h"

namespace
{
    float NormalizeProgress(float Progress)
    {
        return Progress < 0.0f
            ? -1.0f
            : FMath::Clamp(Progress, 0.0f, 1.0f);
    }
}

FGamePlatformLoadingToken UGamePlatformLoadingScreenService::AcquireToken(
    FText Stage,
    float Progress)
{
    FGamePlatformLoadingToken Token;
    Token.Id = FGuid::NewGuid();

    FTokenState& State = Tokens.Add(Token.Id);
    State.Stage = MoveTemp(Stage);
    State.Progress = NormalizeProgress(Progress);
    State.Sequence = NextSequence++;
    BroadcastSnapshot();
    return Token;
}

bool UGamePlatformLoadingScreenService::UpdateToken(
    FGamePlatformLoadingToken Token,
    FText Stage,
    float Progress)
{
    FTokenState* State = Tokens.Find(Token.Id);
    if (!Token.IsValid() || !State)
    {
        return false;
    }

    State->Stage = MoveTemp(Stage);
    State->Progress = NormalizeProgress(Progress);
    State->Sequence = NextSequence++;
    BroadcastSnapshot();
    return true;
}

bool UGamePlatformLoadingScreenService::ReleaseToken(FGamePlatformLoadingToken Token)
{
    if (!Token.IsValid() || Tokens.Remove(Token.Id) == 0)
    {
        return false;
    }

    BroadcastSnapshot();
    return true;
}

void UGamePlatformLoadingScreenService::ReleaseAll()
{
    if (Tokens.IsEmpty())
    {
        return;
    }

    Tokens.Reset();
    BroadcastSnapshot();
}

FGamePlatformUILoadingSnapshot UGamePlatformLoadingScreenService::GetSnapshot() const
{
    FGamePlatformUILoadingSnapshot Snapshot;
    Snapshot.ActiveTokenCount = Tokens.Num();
    Snapshot.bIsLoading = Snapshot.ActiveTokenCount > 0;

    uint64 BestSequence = 0;
    for (const TPair<FGuid, FTokenState>& Pair : Tokens)
    {
        if (Pair.Value.Sequence >= BestSequence)
        {
            BestSequence = Pair.Value.Sequence;
            Snapshot.Stage = Pair.Value.Stage;
            Snapshot.Progress = Pair.Value.Progress;
        }
    }

    return Snapshot;
}

void UGamePlatformLoadingScreenService::BroadcastSnapshot()
{
    OnSnapshotChanged.Broadcast(GetSnapshot());
}
