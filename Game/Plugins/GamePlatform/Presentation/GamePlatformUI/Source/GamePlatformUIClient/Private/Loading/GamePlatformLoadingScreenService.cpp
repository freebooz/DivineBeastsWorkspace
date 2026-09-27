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

    // 只在事务状态变化时重建快照，避免 UI 读取阶段重复遍历 Token。
    RebuildSnapshot();
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
    RebuildSnapshot();
    BroadcastSnapshot();
    return true;
}

bool UGamePlatformLoadingScreenService::ReleaseToken(FGamePlatformLoadingToken Token)
{
    if (!Token.IsValid() || Tokens.Remove(Token.Id) == 0)
    {
        return false;
    }

    RebuildSnapshot();
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
    RebuildSnapshot();
    BroadcastSnapshot();
}

FGamePlatformUILoadingSnapshot UGamePlatformLoadingScreenService::GetSnapshot() const
{
    // 快照在写路径中维护，读取保持 O(1)，适合多个 UI 消费者同时查询。
    return CachedSnapshot;
}

void UGamePlatformLoadingScreenService::RebuildSnapshot()
{
    CachedSnapshot = FGamePlatformUILoadingSnapshot();
    CachedSnapshot.ActiveTokenCount = Tokens.Num();
    CachedSnapshot.bIsLoading = CachedSnapshot.ActiveTokenCount > 0;

    // Token 数量通常极小；仅在 Acquire/Update/Release 时执行一次线性扫描。
    uint64 BestSequence = 0;
    for (const TPair<FGuid, FTokenState>& Pair : Tokens)
    {
        if (Pair.Value.Sequence >= BestSequence)
        {
            BestSequence = Pair.Value.Sequence;
            CachedSnapshot.Stage = Pair.Value.Stage;
            CachedSnapshot.Progress = Pair.Value.Progress;
        }
    }
}

void UGamePlatformLoadingScreenService::BroadcastSnapshot()
{
    OnSnapshotChanged.Broadcast(CachedSnapshot);
}
