#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPCGSpatialRules.generated.h"

/**
 * 平台通用二维空间掩码快照：仅包含已采样的数值几何与稳定来源，不持有地图Actor或项目资产。
 * 闭合多边形可形成农田/人工排除；开放折线以HalfWidthCm形成道路/围合缓冲区。
 * 出于初版可复现/CPU预算，每个来源最多1024个顶点，强度只允许0..1。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGSpatialMask
{
    GENERATED_BODY()

    /** 来源放置器的稳定GUID；项目不能用数组索引代替已发布来源身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial")
    FGuid SourceId;
    /** 来源的稳定领域语义；由WorldDirector读取DomainCatalog标识，用于编辑器候选过滤。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial")
    FName DomainId = NAME_None;

    /** 自该来源向外扩张的线条半宽（厘米）；闭合地块可设置0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0.0"))
    float HalfWidthCm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Strength = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial")
    bool bClosed = false;
    /** 是否挖除闭合曲线内部：地块为true，闭合围栏/沿线装饰为false（只沿边界挖除），避免把农田作物全部清空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial")
    bool bFillInterior = true;

    /** 采样后的世界XY坐标；高度投影由官方PCG处理，空间互斥不写Landscape。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Spatial")
    TArray<FVector2D> Vertices;
};

/** 一次生成范围内预先审查并计算的空间Bounds缓存，不保存Actor指针、不跨请求复用。 */
struct FGamePlatformPCGPreparedSpatialMask
{
    const FGamePlatformPCGSpatialMask* Mask = nullptr;
    FVector2D Min = FVector2D::ZeroVector;
    FVector2D Max = FVector2D::ZeroVector;
};

/** 不依赖世界、网络或内容包的有界几何与优先级算法。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGSpatialRules
{
    static bool ValidateMask(const FGamePlatformPCGSpatialMask& Mask);
    /** 仅在请求开始时审查、预计算几何包围盒，避免每个点都重复做面积与有效性检查。 */
    static bool PrepareMasks(TConstArrayView<FGamePlatformPCGSpatialMask> Masks,
        TArray<FGamePlatformPCGPreparedSpatialMask>& OutPrepared);

    /** 只消费PrepareMasks生成的瞬时缓存；输入几何在本请求完成前禁止修改。 */
    static bool EvaluatePrepared(const FVector2D& Position, int32 SubjectPriority, float Threshold,
        TConstArrayView<FGamePlatformPCGPreparedSpatialMask> Prepared, float& OutStrength, FGuid& OutSource);

    /** 点落在多边形内部/边界，或折线/多边形边缘的缓冲范围内。 */
    static bool ContainsPoint(const FGamePlatformPCGSpatialMask& Mask, const FVector2D& Position);

    /**
     * 输入按任意登记顺序，输出对相同来源/优先级保持确定性；
     * 高优先级才能挖掉低优先级。非法掩码返回false，调用节点必须安全拒绝本批输出。
     */
    static bool Evaluate(const FVector2D& Position, int32 SubjectPriority,
        float Threshold, TConstArrayView<FGamePlatformPCGSpatialMask> Masks,
        float& OutStrength, FGuid& OutSource);
};
