#pragma once

#include "CoreMinimal.h"
#include "Services/GamePlatformPCGSpatialRules.h"
#include "GamePlatformPCGAdvancedSpatialRules.generated.h"

/**
 * P7桥候选，仅供编辑器审核。计算道路和水系的投影交点，不放置桥、不修改地形或导航。
 * TerrainWrite和CutFillRequest独立专项审查通过前不得自动执行。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGBridgeCandidate
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Bridge")
    FVector2D IntersectionXY = FVector2D::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Bridge")
    float RequiredSpanCm = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Bridge")
    int32 RoadSegmentIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Bridge")
    int32 WaterSegmentIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Bridge")
    bool bRequiresHumanApproval = true;
};

/** P7 3D体腔排除合同：只定义禁止散布的空间，不执行Landscape不可逆写入。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGCavityExclusion
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Cavity")
    FGamePlatformPCGSpatialMask Footprint;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Cavity")
    float MinZCm = -100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Cavity")
    float MaxZCm = 100.0f;
};

/** P8空间图的节点：用于连通性与Nav候选审查，而非生成任意房屋模型。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGSpaceNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Space")
    FName NodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Space")
    FVector CenterCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Space")
    float ClearanceCm = 200.0f;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGSpaceEdge
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Space")
    FName FromNodeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|PCG|Space")
    FName ToNodeId = NAME_None;
};

/** P7/P8无状态空间规则，输入有预算上限、无副作用且失败关闭。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGAdvancedSpatialRules
{
    /** 水域交叉只输出候选，自动连接前仍需人工与Nav/碰撞审批。 */
    static bool FindBridgeCandidates(const FGamePlatformPCGSpatialMask& Road,
        const FGamePlatformPCGSpatialMask& Water, float AdditionalMarginCm,
        TArray<FGamePlatformPCGBridgeCandidate>& OutCandidates);

    static bool IsInsideCavity(const FGamePlatformPCGCavityExclusion& Cavity,
        const FVector& Position, bool& bOutInside);

    /** 不允许运行时生成不可达的封闭空间；最多64节点、128条边。 */
    static bool ValidateSpatialGraph(TConstArrayView<FGamePlatformPCGSpaceNode> Nodes,
        TConstArrayView<FGamePlatformPCGSpaceEdge> Edges, FName EntryId, FName ExitId);
};
