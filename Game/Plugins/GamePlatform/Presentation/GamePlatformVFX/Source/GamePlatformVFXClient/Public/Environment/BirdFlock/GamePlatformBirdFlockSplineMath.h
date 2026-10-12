// 平台层鸟群样条飞行与聚群的纯数值计算合同；不认识鸟类物种或项目资源。
// 运行端：客户端GPU模块作者/UE自动化；绝不用于服务器伤害、导航或复制。
#pragma once

#include "CoreMinimal.h"

/** EGamePlatformBirdRouteTraversal（可视样条行进方式）。旧BirdFlock模式0、1、2不在此重复定义。 */
enum class EGamePlatformBirdRouteTraversal : uint8
{
    Once = 0,     // 单程到终点
    Loop = 1,     // 循环起点
    PingPong = 2  // 起终点往返
};

/**
 * FGamePlatformBirdFlockSplineMath（平台中立鸟群路径数学）。
 * 所有距离以cm，时间以s计；路径采样由Niagara Spline DI或烘焙LUT消费；
 * 本类型只计算有界进度与有限群体权重，不在CPU逐只更新粒子。
 */
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformBirdFlockSplineMath final
{
    /** 必须输入有限且正的路线长度/速度；失败返回false、输出清零。 */
    static bool EvaluateProgress(double ElapsedSeconds, double SpeedCmPerSecond,
        double PathLengthCm, double Phase01, EGamePlatformBirdRouteTraversal Mode,
        double& OutProgress01, bool& bReachedEnd);

    /** 聚群参数三权重均有限、非负，总权重最多4。禁止NaN引发GPU轨迹失效。 */
    static bool ValidateFlockingWeights(float Separation, float Alignment, float Cohesion);

    /** 每群有效数量0..512；画质0..1；返回INDEX_NONE代表非法参数。 */
    static int32 ResolveVisibleCount(int32 RequestedCount, float QualityFraction);
};
