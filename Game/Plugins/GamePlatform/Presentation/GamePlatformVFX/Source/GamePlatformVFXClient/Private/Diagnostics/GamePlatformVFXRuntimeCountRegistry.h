// VFX跨世界诊断账本：只持有计数和注册标识，不延长UWorld/PIE对象生命周期。
#pragma once

#include "CoreMinimal.h"

/** 所有计数单位为实例/世界；Peak为本模块生命周期内同时Tracked总数的历史最大值。 */
struct FGamePlatformVFXRuntimeCountSnapshot
{
    int32 TrackedInstances = 0;
    int32 PendingInstances = 0;
    int32 PeakTrackedInstances = 0;
    int32 RegisteredWorlds = 0;
};

/** 仅游戏线程。显式注册/注销，已注销标识的迟到更新不能复活记录；Reset用于模块结束。 */
class FGamePlatformVFXRuntimeCountRegistry
{
public:
    FGuid RegisterWorld()
    {
        check(IsInGameThread());
        const FGuid Id = FGuid::NewGuid();
        Worlds.Add(Id, {});
        Recalculate();
        return Id;
    }

    bool UpdateWorld(const FGuid& Id, int32 Tracked, int32 Pending)
    {
        check(IsInGameThread());
        FWorldCounts* Counts = Worlds.Find(Id);
        if (!Counts) return false;
        Counts->Tracked = FMath::Max(0, Tracked);
        Counts->Pending = FMath::Max(0, Pending);
        Recalculate();
        return true;
    }

    bool UnregisterWorld(const FGuid& Id)
    {
        check(IsInGameThread());
        if (Worlds.Remove(Id) == 0) return false;
        Recalculate();
        return true;
    }

    FGamePlatformVFXRuntimeCountSnapshot GetSnapshot() const
    {
        check(IsInGameThread());
        return Snapshot;
    }

    void Reset()
    {
        check(IsInGameThread());
        Worlds.Reset();
        Snapshot = {};
    }

private:
    struct FWorldCounts
    {
        int32 Tracked = 0;
        int32 Pending = 0;
    };

    void Recalculate()
    {
        // 世界数很小，按世界重算避免计数饱和后减法丢失其他世界贡献；不会遍历VFX实例。
        int64 Tracked = 0;
        int64 Pending = 0;
        for (const auto& Pair : Worlds)
        {
            Tracked += Pair.Value.Tracked;
            Pending += Pair.Value.Pending;
        }
        Snapshot.TrackedInstances = static_cast<int32>(FMath::Min<int64>(Tracked, MAX_int32));
        Snapshot.PendingInstances = static_cast<int32>(FMath::Min<int64>(Pending, MAX_int32));
        Snapshot.PeakTrackedInstances = FMath::Max(Snapshot.PeakTrackedInstances, Snapshot.TrackedInstances);
        Snapshot.RegisteredWorlds = Worlds.Num();
    }

    TMap<FGuid, FWorldCounts> Worlds;
    FGamePlatformVFXRuntimeCountSnapshot Snapshot;
};
