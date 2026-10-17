// SSLM 狼群散布无须Fab美术即可执行的服务器几何回归。
// 仅验证10只候选容量、30米严格间距与安全区，不冒充真实NavMesh/地图/联网验收。
#include "Village/DivineBeastsVillageWolfSpawnPolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsVillageWolfSpawnPolicyTest,
    "DivineBeasts.Server.VillageWolf.SpawnPolicyTenAndThirtyMeters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsVillageWolfSpawnPolicyTest::RunTest(const FString&)
{
    using FPolicy = FDivineBeastsVillageWolfSpawnPolicy;
    TestEqual(TEXT("同时存活数量上限为10只"), FPolicy::MaxAliveWolves, 10);
    TestEqual(TEXT("单批最大新增4只，防止单帧同时10次AI/导航启动"), FPolicy::MaxWolvesPerBatch, 4);
    TestTrue(TEXT("跨批次0.2秒间隔应保持正值"),
        FMath::IsNearlyEqual(FPolicy::SpawnBatchIntervalSeconds, 0.20f));
    TestEqual(TEXT("严格最小散布距离为3000厘米"), FPolicy::MinimumSpawnSeparationCm, 3000.0f);

    const FVector Start = FVector::ZeroVector;
    TestFalse(TEXT("正好30米必须拒绝"), FPolicy::HasSpawnSeparation(Start, FVector(3000.0f, 0, 0)));
    TestFalse(TEXT("29.99米必须拒绝"), FPolicy::HasSpawnSeparation(Start, FVector(2999.0f, 0, 0)));
    TestTrue(TEXT("30.01米应满足间距"), FPolicy::HasSpawnSeparation(Start, FVector(3001.0f, 0, 0)));
    TestFalse(TEXT("只有高度差不能绕过水平间距限制"),
        FPolicy::HasSpawnSeparation(Start, FVector(0, 0, 10000.0f)));
    TestFalse(TEXT("22米内禁止在玩家出生区刷怪"),
        FPolicy::IsOutsidePlayerStart(FVector(2199.0f, 0, 0), Start));
    TestTrue(TEXT("22米以外可以继续进行其它安全查询"),
        FPolicy::IsOutsidePlayerStart(FVector(2201.0f, 0, 0), Start));

    // 理想平地容量验证：选择真实排列顺序中的第一个合格候选点。
    // 如果连续10个锚点无法满足严格距离，本用例立即失败；实际地图依然需要NavMesh和地形检查。
    TArray<FVector> SpawnAnchors;
    for (int32 Ordinal = 0; Ordinal < FPolicy::MaxAliveWolves; ++Ordinal)
    {
        bool bAccepted = false;
        for (int32 Ring = 0; Ring < FPolicy::RingCount && !bAccepted; ++Ring)
        {
            for (int32 Direction = 0; Direction < FPolicy::DirectionsPerRing; ++Direction)
            {
                const FVector Proposed = FPolicy::MakeCandidate(Start, Ring, Direction, Ordinal);
                if (!FPolicy::IsOutsidePlayerStart(Proposed, Start))
                    continue;
                bool bSeparated = true;
                for (const FVector& Existing : SpawnAnchors)
                    if (!FPolicy::HasSpawnSeparation(Proposed, Existing))
                    {
                        bSeparated = false;
                        break;
                    }
                if (!bSeparated)
                    continue;
                SpawnAnchors.Add(Proposed);
                bAccepted = true;
                break;
            }
        }
        if (!TestTrue(*FString::Printf(TEXT("理想平地候选第%d只狼必须有合格散布点"), Ordinal + 1),
                bAccepted))
            return false;
    }
    TestEqual(TEXT("多圈候选在理想平地能容纳10只"), SpawnAnchors.Num(), 10);
    for (int32 i = 0; i < SpawnAnchors.Num(); ++i)
        for (int32 j = i + 1; j < SpawnAnchors.Num(); ++j)
            if (!TestTrue(TEXT("任意两个生成锚点间距必须严格超过30米"),
                    FPolicy::HasSpawnSeparation(SpawnAnchors[i], SpawnAnchors[j])))
                return false;
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
