#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformId.h"
#include "GamePlatformExperienceState.generated.h"

/** 服务器体验阶段；客户端复制到Active只表示服务器已激活，不表示本机资源已经准备。 */
UENUM(BlueprintType)
enum class EGamePlatformExperienceStage : uint8
{
    /** 当前世界尚未选择体验。 */
    Unassigned,
    /** 服务器正在校验定义、申请租约并装配必需规则。 */
    Preparing,
    /** 服务器资源与规则已经激活，可逐玩家推进出生；租约仍由体验持有。 */
    Active,
    /** 已停止新加入和新生成，正在逆序撤销玩家、装配与资源。 */
    Draining,
    /** 本次体验资源已经逻辑释放；旧回调不得重新激活。 */
    Released,
    /** 本次体验失败并已开始或完成回滚；FailureCode提供脱敏原因。 */
    Failed
};

/** 当前客户端对服务器体验快照的本地资源准备阶段；不复制回其他客户端。 */
UENUM(BlueprintType)
enum class EGamePlatformClientExperienceStage : uint8
{
    /** 尚无可准备的服务器快照。 */
    Unassigned,
    /** 正在通过本实例Data服务申请本地体验和Pawn定义。 */
    Preparing,
    /** 本机体验资源可用；仍须识别正确Pawn和完成输入绑定。 */
    Prepared,
    /** 本机资源准备失败；不修改服务器体验阶段。 */
    Failed,
    /** 体验排空或世界关闭后，本机租约已经释放。 */
    Released
};

/**
 * 体验的当前复制快照。字段共同构成一次运行事实，晚加入者直接读取当前值，不依赖历史多播。
 * 不包含定义对象、Data租约、服务器凭据、账号信息或任意可执行类路径。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformExperienceSnapshot
{
    GENERATED_BODY()

    /** 来自当前GamePlatformWorld上下文的运行身份；跨图旧快照不可操作新世界。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGuid WorldContextGeneration;

    /** 来自真实体验定义LogicalId的稳定逻辑身份，不是资产文件路径。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FGamePlatformId ExperienceId;

    /** 体验定义的内容修订；仅用于兼容诊断，不充当运行代次。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int32 ContentRevision = 0;

    /** 当前世界内每次体验启动递增的运行代次；0表示尚未启动。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 ExperienceEpoch = 0;

    /** 每次公开状态变化递增；用于幂等重算和拒绝旧准备报告。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 StateRevision = 0;

    /** 服务器体验阶段；默认未选定，不能被误认为成功。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    EGamePlatformExperienceStage Stage = EGamePlatformExperienceStage::Unassigned;

    /** 脱敏稳定错误码；成功状态为None，不携带敏感后端正文。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName FailureCode;

    /** 是否包含可关联到当前世界的一次真实体验选择；不等价于Active。 */
    bool IsAssigned() const
    {
        return WorldContextGeneration.IsValid() && ExperienceId.IsValid() && ExperienceEpoch > 0 && StateRevision > 0;
    }

    /** 是否为服务器活动体验；客户端仍须独立检查本地准备快照。 */
    bool IsServerActive() const { return IsAssigned() && Stage == EGamePlatformExperienceStage::Active; }
};

/** 本机体验资源准备快照；仅当前进程读取，不包含可复制的认证结论。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMGAMEPLAY_API FGamePlatformClientExperienceSnapshot
{
    GENERATED_BODY()

    /** 对应的服务器体验代次；不匹配时本地结果必须丢弃。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    int64 ExperienceEpoch = 0;

    /** 当前本机准备阶段。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    EGamePlatformClientExperienceStage Stage = EGamePlatformClientExperienceStage::Unassigned;

    /** 脱敏失败码；Prepared时为None。 */
    UPROPERTY(BlueprintReadOnly, Category="GamePlatform|Gameplay")
    FName FailureCode;
};
