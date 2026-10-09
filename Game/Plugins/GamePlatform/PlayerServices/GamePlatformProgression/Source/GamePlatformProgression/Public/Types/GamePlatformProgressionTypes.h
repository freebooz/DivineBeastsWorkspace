// 平台中立只读领域投影：后端拥有权益/成长/运营权威；游戏线程服务产生值副本，单位/空值语义如下，不保存认证秘密。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformProgressionSubjectType : uint8
{
    Unknown,
    Character,
    Player
};

UENUM(BlueprintType)
enum class EGamePlatformProgressionPostMaxXPPolicy : uint8
{
    ClampAtMax
};

UENUM(BlueprintType)
enum class EGamePlatformProgressionError : uint8
{
    None, // 当前操作无错误，不表示客户端获得权威写权限
    ProgressionNotFound, // 主体没有所需成长记录
    TrackNotFound, // 成长轨道不存在
    CurveNotFound, // 曲线定义未加载/不存在
    CurveVersionMismatch, // 权威轨道与本地定义版本不兼容，不伪造进度
    InvalidCurve, // 曲线结构或阈值不合法
    InvalidXPAmount, // XP输入范围/单位不合法
    XPOverflow, // 累计XP运算将溢出整数范围
    MaxLevelReached, // 轨道已达权威等级上限
    RevisionConflict, // 期望版本与权威版本不一致，必须重读
    DuplicateOperation, // 操作身份已登记，后端须幂等返回原结果
    OperationInProgress, // 已有未决操作，禁止并行修改
    GrantNotAuthorized, // 调用方没有权威授予权限
    InvalidSource, // 来源身份无法验证
    RequirementNotMet, // 权威成长/资格需求未满足
    SnapshotUnavailable, // 目前无可读权威快照
    BackendUnavailable, // 领域传输/后端不可用，保留可读投影
    OutcomeUnknown, // 请求可能已提交，必须按原操作/订单身份查询对账
    Unauthorized, // 认证失效或调用方权限不足
    Cancelled, // 仅本地等待取消，不能回滚已提交的权威事务
    TimedOut, // 等待截止时间已到，是否提交依终态与原操作对账
    InvalidResponse // 响应结构/身份/版本无法验证，不能替换旧快照
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionTrackState
{
    GENERATED_BODY()

    /** 成长主体类别；Character或Player，Unknown无效；需求默认Character。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    EGamePlatformProgressionSubjectType SubjectType =
        EGamePlatformProgressionSubjectType::Unknown;

    /** 后端主体稳定身份；空值无主体，客户端不能自行构造授权。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FString SubjectId;

    /** 成长轨道稳定身份；None缺失，客户端只读投影不授予XP。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    /** 非负累计XP单位整数；0合法，不用浮点表示长期经验。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 TotalXP = 0;

    /** 后端正整数等级；默认1，合法范围[1,MaxLevel]。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 Level = 1;

    /** 后端正整数等级上限；不小于Level，客户端不能用本地曲线修改权威上限。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 MaxLevel = 1;

    /** 后端/定义正整数曲线版本；默认1，不兼容时保留权威快照并标记视图。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 CurveVersion = 1;

    /** 后端条目单调修订号；有效轨道/需求结果必须保持来源版本，不在客户端自增。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 Revision = 1;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionSnapshot
{
    GENERATED_BODY()

    /** 后端成长快照单调版本；0未加载，有效快照必须大于0。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 ProgressionRevision = 0;

    /** 后端生成玩家快照UTC；有效响应需有实际时间。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FDateTime GeneratedAtUtc;

    /** 后端完整轨道集合；合法空数组清空轨道，缺失/错类型不能视为空成功。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    TArray<FGamePlatformProgressionTrackState> Tracks;

    bool IsValid() const
    {
        return ProgressionRevision > 0 &&
               GeneratedAtUtc.GetTicks() > 0;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionRequirement
{
    GENERATED_BODY()

    /** 成长主体类别；Character或Player，Unknown无效；需求默认Character。 */
    UPROPERTY(BlueprintReadWrite, Category="Progression")
    EGamePlatformProgressionSubjectType SubjectType =
        EGamePlatformProgressionSubjectType::Character;

    /** 后端主体稳定身份；空值无主体，客户端不能自行构造授权。 */
    UPROPERTY(BlueprintReadWrite, Category="Progression")
    FString SubjectId;

    /** 成长轨道稳定身份；None缺失，客户端只读投影不授予XP。 */
    UPROPERTY(BlueprintReadWrite, Category="Progression")
    FName ProgressionTrackId = NAME_None;

    /** 需求要求的正整数最低等级；默认1，权威校验仍由后端/服务器执行。 */
    UPROPERTY(BlueprintReadWrite, Category="Progression", meta=(ClampMin="1"))
    int32 RequiredLevel = 1;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMPROGRESSION_API FGamePlatformProgressionRequirementResult
{
    GENERATED_BODY()

    /** 只读成长需求判定；默认false，不替代服务器权限/交易校验。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    bool bMet = false;

    /** 需求查询使用的来源等级；默认1，需配合来源Revision与匹配轨道解释。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 CurrentLevel = 1;

    /** 后端条目单调修订号；有效轨道/需求结果必须保持来源版本，不在客户端自增。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int64 Revision = 1;

    /** 后端/定义正整数曲线版本；默认1，不兼容时保留权威快照并标记视图。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    int32 CurveVersion = 1;

    /** 成长轨道稳定身份；None缺失，客户端只读投影不授予XP。 */
    UPROPERTY(BlueprintReadOnly, Category="Progression")
    FName ProgressionTrackId = NAME_None;
};
