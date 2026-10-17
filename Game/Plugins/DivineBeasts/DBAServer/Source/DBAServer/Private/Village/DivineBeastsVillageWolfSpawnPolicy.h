// SSLM 新手村狼群散布的项目级几何规则：只由服务器调用，不引入第三方美术或客户端依赖。
// 这里定义“最大同时存活10只”和“生成锚点之间水平距离严格大于30米”。
#pragma once

#include "CoreMinimal.h"

struct FDivineBeastsVillageWolfSpawnPolicy final
{
    static constexpr int32 MaxAliveWolves = 10;
    // 将10只分批生成以降低同帧导航投射和碰撞扫描峰值；批次由本世界Timer驱动。
    static constexpr int32 MaxWolvesPerBatch = 4;
    static constexpr float SpawnBatchIntervalSeconds = 0.20f;
    static constexpr float MinimumSpawnSeparationCm = 3000.0f; // 30米；距离恰好30米不合格。
    static constexpr float PlayerStartSafetyRadiusCm = 2200.0f; // 现有22米出生保护圈保持不变。

    // 初始45米，之后每圈外扩35米，最远290米；每圈24个方向，候选有上界。
    // 范围并不保证必定刷足10只：必须通过真实导航、坡度、地面与占位检查。
    static constexpr int32 RingCount = 8;
    static constexpr int32 DirectionsPerRing = 24;
    static constexpr float FirstRingRadiusCm = 4500.0f;
    static constexpr float RingStepCm = 3500.0f;

    /** 在每个出生点附近生成有界、确定顺序的多圈候选；不同序号旋转方向起点以避免扎堆。 */
    static FVector MakeCandidate(
        const FVector& PlayerStart, int32 Ring, int32 Direction, int32 SpawnOrdinal)
    {
        const int32 DirectionIndex =
            (Direction * 7 + SpawnOrdinal * 5 + Ring * 3) % DirectionsPerRing;
        const float Radians = 2.0f * PI * static_cast<float>(DirectionIndex) /
            static_cast<float>(DirectionsPerRing);
        const float RadiusCm = FirstRingRadiusCm + RingStepCm * Ring;
        return PlayerStart + FVector(FMath::Cos(Radians) * RadiusCm,
            FMath::Sin(Radians) * RadiusCm, 0.0f);
    }

    /** 地形高差不计入散布间距；只校验XY，严格大于30米。 */
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
