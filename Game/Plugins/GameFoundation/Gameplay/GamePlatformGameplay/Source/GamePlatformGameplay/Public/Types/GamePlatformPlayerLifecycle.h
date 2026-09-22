#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformId.h"
#include "UObject/PrimaryAssetId.h"
#include "GamePlatformPlayerLifecycle.generated.h"

class APawn;

/** 单个玩家在当前服务器世界的生命周期；客户端本地输入开放不属于此权威阶段。 */
UENUM(BlueprintType)
enum class EGamePlatformPlayerStage : uint8
{
    /** 尚无可信准入记录；默认值不得解释为已接受。 */
    Unregistered,
    /** 服务器适配已提交并绑定当前真实连接的准入。 */
    Accepted,
    /** 准入有效，正在等待服务器体验Active。 */
    WaitingExperience,
    /** 体验有效，正在等待Pawn定义、区域、碰撞或出生候选。 */
    WaitingSpawn,
    /** 已占用候选并正在生成；同一生成代次不得重复执行。 */
    Spawning,
    /** 服务器已经控制受限Pawn，但尚未等待或接受客户端准备。 */
    Possessed,
    /** 已向拥有者签发当前代次令牌，等待本地准备报告。 */
    AwaitingClient,
    /** 服务器重新验证全部资格后允许该玩家执行受限玩法命令。 */
    Active,
    /** 已停止新操作，正在撤销令牌、Pawn、占位和租约。 */
    Leaving,
    /** 本次记录已清理；迟到回调不得恢复。 */
    Removed,
    /** 本玩家操作失败；其他玩家与服务器体验保持独立。 */
    Failed
};

/**
 * 可复制给所有相关客户端的玩家公开快照；只含匿名玩法身份和当前实体事实。
 * 不包含账号、票据、AssignmentId、服务器启动身份、内部错误正文或准备令牌。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformPlayerLifecycleSnapshot
{
    GENERATED_BODY()

    /** 由可信服务器适配提供的公开匿名参与者身份；不等于登录凭据。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGamePlatformId ParticipantId;

    /** 当前世界内玩家记录代次；重连创建新记录，不按显示名接管。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 PlayerGeneration = 0;

    /** 当前出生操作代次；每次服务器授权重生成递增。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 SpawnGeneration = 0;

    /** 当前活动或待确认Pawn代次；旧Pawn事件不得推进新Pawn。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 PawnGeneration = 0;

    /** 每次公开玩家状态变化递增。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 StateRevision = 0;

    /** 当前服务器玩家阶段。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    EGamePlatformPlayerStage Stage = EGamePlatformPlayerStage::Unregistered;

    /** 服务器批准的中立Pawn定义身份；不是客户端提交的原生类路径。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGamePlatformId PawnDefinitionId;

    /** 当前服务器认可的受控Pawn；可能因复制顺序暂时为空，客户端应幂等重算。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    TObjectPtr<APawn> ControlledPawn = nullptr;

    /** 脱敏阻塞或失败码；不得包含私有后端信息。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName StatusCode;

    /** 是否包含一条真实登记记录；不表示准入仍有效或玩家已激活。 */
    bool IsRegistered() const { return ParticipantId.IsValid() && PlayerGeneration > 0 && StateRevision > 0; }

    /** 是否为服务器活动玩家；仍不代表客户端本地输入当前开放。 */
    bool IsServerActive() const { return IsRegistered() && Stage == EGamePlatformPlayerStage::Active; }
};

/**
 * 由服务器组合根提交的非敏感可信准入投影。结构本身不是安全证明；调用方必须先查询真实准入登记，
 * 并把本值与参数中的实际PlayerController连接绑定。该值只保存在服务器GameMode私有登记表中。
 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformVerifiedPlayerContext
{
    /** 每次已提交准入的服务器登记身份；撤销必须精确匹配。 */
    FGuid AdmissionId;
    /** 可公开给其他玩家的匿名参与者逻辑身份；禁止放账号口令或完整档案。 */
    FGamePlatformId ParticipantId;
    /** 当前控制面分配身份，仅服务器诊断使用；不得复制。 */
    FString AssignmentId;
    /** 当前服务器实例身份，仅服务器比对；不得由客户端提供。 */
    FString ServerInstanceId;
    /** 当前服务器启动代次；0无效，防止旧实例准入复用。 */
    uint64 ServerStartGeneration = 0;
    /** 当前真实网络连接代次；换连接必须变化。 */
    uint64 ConnectionGeneration = 0;
    /** 已提交会话绑定代次；旧SessionEpoch不得清理新绑定。 */
    uint64 SessionEpoch = 0;
    /** 服务器允许的体验身份；必须匹配当前体验，不是客户端选择。 */
    FGamePlatformId ExperienceId;
    /** 可选服务器批准Pawn定义；无效值表示使用体验默认定义。 */
    FPrimaryAssetId PawnDefinitionId;

    /** 仅校验完整性与长度；是否来自可信服务及是否绑定实际连接由提交适配负责。 */
    bool IsStructurallyValid() const;
};

/** 服务器保存的准入撤销键；只匹配当前AdmissionId和SessionEpoch，旧来源不能撤销新记录。 */
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformAdmissionRevocation
{
    FGuid AdmissionId;
    uint64 ConnectionGeneration = 0;
    uint64 SessionEpoch = 0;

    bool IsStructurallyValid() const
    {
        return AdmissionId.IsValid() && ConnectionGeneration > 0 && SessionEpoch > 0;
    }
};
