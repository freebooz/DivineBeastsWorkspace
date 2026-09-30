#pragma once

// 平台共享权益契约：双端纯值用于只读查询与客户端投影；后端拥有授予/撤销权威。
// UTC字段不含认证票据；对象无资源/线程所有权，可复制但不能并发写同一值；反射/字段身份保持。

#include "CoreMinimal.h"
#include "GamePlatformEntitlementTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformEntitlementTargetType : uint8
{
    Unknown, // 未识别类型，失败闭合
    Hero, // 通用可选角色权益，不绑定项目角色规则
    Skin, // 皮肤权益，仅表现使用
    Cosmetic, // 其他装饰权益
    Feature // 通用功能权益
};

UENUM(BlueprintType)
enum class EGamePlatformEntitlementStatus : uint8
{
    Active, // 权威快照判定当前有效
    NotStarted, // 尚未生效
    Expired, // 已到期
    NotEntitled, // 无目标权益
    Revoked // 已被权威撤销
};

UENUM(BlueprintType)
enum class EGamePlatformEntitlementError : uint8
{
    None, // 当前操作无错误
    EntitlementNotFound, // 权益条目不存在
    DefinitionNotFound, // 所需定义缺失
    NotEntitled, // 无目标权益
    EntitlementExpired, // 操作要求的权益已到期
    EntitlementNotStarted, // 操作要求的权益未生效
    SnapshotUnavailable, // 尚无可读权威快照
    RevisionConflict, // 要求版本与权威状态不一致，需重读
    DuplicateOperation, // 操作身份已登记，不重复授予
    GrantNotAuthorized, // 调用方无授予权限
    RevokeNotAuthorized, // 调用方无撤销权限
    GrantNotFound, // 待撤销的授予不存在
    InvalidValidityWindow, // 生效/到期窗口非法
    InvalidSource, // 权威变更来源不合法
    OutcomeUnknown, // 请求可能已提交，结果未知，必须按操作身份对账
    BackendUnavailable, // 当前传输/后端不可用
    Unauthorized, // 认证失效或权限不足
    Cancelled, // 本地请求已取消，不能据此回滚已提交事务
    TimedOut, // 截止时间已到且当前未确认提交
    InvalidResponse // 响应载荷无法验证
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementEntry
{
    GENERATED_BODY()

    /** 稳定权益条目身份；None表示未提供，不能判定拥有。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;

    /** 中立分类标识；None表示无分类，不推断英雄/项目规则。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName Category = NAME_None;

    /** 目标类型；Unknown无有效目标。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementTargetType TargetType =
        EGamePlatformEntitlementTargetType::Unknown;

    /** 目标稳定身份；None表示缺失，不作为资产路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName TargetId = NAME_None;

    /** 后端已计算的权益状态；Active才可显示有效，但不构成服务器准入授权。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementStatus Status =
        EGamePlatformEntitlementStatus::Active;

    /** 是否提供生效UTC；false时StartsAtUtc不参与判断。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bHasStartsAt = false;

    /** 可选生效UTC时刻，与bHasStartsAt配合，不使用本地时区。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime StartsAtUtc;

    /** 是否提供到期UTC；false表示没有期限。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bHasExpiresAt = false;

    /** 可选到期UTC时刻，与bHasExpiresAt配合。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime ExpiresAtUtc;

    /** 仅查询当前投影Active且身份有效；不重新计算UTC或执行权威解锁。 */
    bool IsEffective() const
    {
        return !EntitlementId.IsNone() &&
               Status == EGamePlatformEntitlementStatus::Active;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementSnapshot
{
    GENERATED_BODY()

    /** 后端单调快照修订号；0表示未加载，不能据此授予权益。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    int64 Revision = 0;

    /** 后端生成快照UTC；非正ticks表示无有效时间。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime GeneratedAtUtc;

    /** 当前只读条目集合；空集合合法，客户端不能追加权威授予。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    TArray<FGamePlatformEntitlementEntry> Entitlements;

    /** 检查快照基础身份/时间，合法空集合也为有效；不验证每个领域条目。 */
    bool IsValid() const
    {
        return Revision > 0 && GeneratedAtUtc.GetTicks() > 0;
    }
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementCheckResult
{
    GENERATED_BODY()

    /** 只读查询结果；默认false，结果为true仍不能替代服务器验证。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bEntitled = false;

    /** 后端单调快照修订号；0表示未加载，不能据此授予权益。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    int64 Revision = 0;

    /** 后端已计算的权益状态；Active才可显示有效，但不构成服务器准入授权。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementStatus Status =
        EGamePlatformEntitlementStatus::Expired;

    /** 稳定权益条目身份；None表示未提供，不能判定拥有。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;
};
