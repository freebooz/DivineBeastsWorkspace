#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformId.h"
#include "GamePlatformPCGAnchorContracts.generated.h"

/** P5静态候选：提供来源/位置和稳定身份，但不意味着该地点已获Gameplay/Navigation服务器批准。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGAnchorCandidate
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Anchor")
    FGuid StableId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Anchor")
    FGuid SourceId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Anchor")
    FName DomainId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Anchor")
    FTransform WorldTransform = FTransform::Identity;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|Anchor")
    int32 ContentRevision = 0;
};

/**
 * P9权威状态快照仅由外部服务器Gameplay/Interaction/Save提供。
 * PCG自己不修改此结构、不发资源、不决定开门/砍伐/采集成功。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPCG_API FGamePlatformPCGObjectStateSnapshot
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|State")
    FGuid StableId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|State")
    int32 ContentRevision = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|State")
    int64 ServerSequence = 0;

    /** 业务状态的枚举数值由权威插件解读；PCG只检查传输身份、代次和时序，不判定合法交互。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GamePlatform|PCG|State")
    int32 StateCode = 0;
};

/** P5/P6无状态候选身份与权威快照校验规则，可用于其他游戏和服务器恢复。 */
struct GAMEPLATFORMPCG_API FGamePlatformPCGAnchorRules
{
    /** 内容修订/来源/插槽/领域决定确定性ID；升级版本时须迁移存档映射。 */
    static bool MakeStableId(const FGamePlatformId& WorldId, const FGamePlatformId& RegionId,
        FGuid SourceId, FName DomainId, int32 ContentRevision, int32 SlotIndex,
        FGuid& OutStableId);

    /** 检查来自服务器的候选状态能否映射本次静态烘焙，不修改游戏或存档状态。 */
    static bool ValidateAuthoritativeStates(TConstArrayView<FGamePlatformPCGAnchorCandidate> Anchors,
        TConstArrayView<FGamePlatformPCGObjectStateSnapshot> Snapshots);
};
