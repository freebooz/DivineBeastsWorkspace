// SSLM 新手村狼群散布的项目级几何规则：只由服务器调用，不引入第三方美术或客户端依赖。
// 野狼与吸血鬼合计最多200只；锚点严格大于24米，保留22米出生保护。
#pragma once

#include "CoreMinimal.h"

struct FDivineBeastsVillageWolfSpawnPolicy final
{
    static constexpr int32 MaxAliveWolves = 200;
    // 分批生成以降低同帧AI/导航启动峰值；批次由本世界Timer驱动。
    static constexpr int32 MaxWolvesPerBatch = 4;
    static constexpr float SpawnBatchIntervalSeconds = 0.20f;
    static constexpr float MinimumSpawnSeparationCm = 2400.0f; // 恰好24米不合格。
    static constexpr float PlayerStartSafetyRadiusCm = 2200.0f; // 现有22米出生保护圈保持不变。
    static constexpr float CandidateGridStepCm = 1300.0f;
    static constexpr int32 MaxCandidateCells = 4096;
    static constexpr int32 ScatterSeed = 20261024;

    /** 竖直胶囊相对斜面接触点的中心高度；球端沿法线保留4cm净距，不能按平地高度穿坡。 */
    static float SpawnHeightOnSlope(float HalfHeightCm, float RadiusCm, float SurfaceNormalZ)
    {
        return HalfHeightCm - RadiusCm + (RadiusCm + 4.f) / FMath::Clamp(SurfaceNormalZ, .1f, 1.f);
    }

    /** 地形高差不计入散布间距；只校验XY，严格大于24米。 */
    static bool HasSpawnSeparation(const FVector& Candidate, const FVector& ExistingSpawnAnchor)
    {
        return FVector::DistSquared2D(Candidate, ExistingSpawnAnchor) >
            FMath::Square(MinimumSpawnSeparationCm);
    }

    /** 保留玩家初生22米禁刷区；检查所有实际PlayerStart，而不只是当前采样中心。 */
    static bool IsOutsidePlayerStart(const FVector& Candidate, const FVector& Start)
    {
        return FVector::DistSquared2D(Candidate, Start) >=
            FMath::Square(PlayerStartSafetyRadiusCm);
    }
};
