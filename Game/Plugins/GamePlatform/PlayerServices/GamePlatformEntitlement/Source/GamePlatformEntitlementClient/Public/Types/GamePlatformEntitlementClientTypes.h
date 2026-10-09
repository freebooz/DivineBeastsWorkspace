// 平台中立只读领域投影：后端拥有权益/成长/运营权威；游戏线程服务产生值副本，单位/空值语义如下，不保存认证秘密。
#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEntitlementTypes.h"
#include "GamePlatformEntitlementClientTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformEntitlementClientState : uint8
{
    Uninitialized,
    Loading,
    Ready,
    Reconciling,
    Error
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMENTITLEMENTCLIENT_API FGamePlatformEntitlementViewModel
{
    GENERATED_BODY()

    /** 稳定权益身份；None未知，无对应条目时只读查询返回false。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName EntitlementId = NAME_None;

    /** 权益分类语义；None未提供，不携带项目规则或资源路径。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName Category = NAME_None;

    /** 权益目标类型；Unknown无法识别，不据此显示已解锁。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    EGamePlatformEntitlementTargetType TargetType =
        EGamePlatformEntitlementTargetType::Unknown;

    /** 目标定义稳定身份；None无目标，服务器仍需独立验证授权。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FName TargetId = NAME_None;

    /** 仅按当前Active权威投影派生的解锁显示；默认false，不能授予真实权益。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bUnlocked = false;

    /** 是否有到期UTC；false代表未声明期限，不读取ExpiresAtUtc。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    bool bHasExpiresAt = false;

    /** 可选到期UTC时刻；只在bHasExpiresAt=true时解释，不使用本地时区。 */
    UPROPERTY(BlueprintReadOnly, Category="Entitlement")
    FDateTime ExpiresAtUtc;
};
