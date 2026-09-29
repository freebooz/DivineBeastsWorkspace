#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformPCGEnvironmentDefinitions.h"

/** 线性系统可独立测试的纯规则；不读取World、不扫描Actor。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGLinearRules
{
    /** 根据跨度选择最窄可覆盖MeshSet规则；相同宽度时按MeshSetId字典序稳定决胜。 */
    static bool SelectSpanMeshByLength(
        float SpanLengthCm,
        TConstArrayView<FGamePlatformPCGSpanMeshRule> Rules,
        FName& OutMeshSetId);

    /** 按累计距离判断当前候选是否达到下一个柱距。 */
    static bool ShouldKeepPost(float DistanceFromLastKeptCm, float PostSpacingCm);
};
