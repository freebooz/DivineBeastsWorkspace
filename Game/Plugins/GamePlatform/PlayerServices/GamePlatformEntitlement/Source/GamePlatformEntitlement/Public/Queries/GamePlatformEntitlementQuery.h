#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformEntitlementTypes.h"

class GAMEPLATFORMENTITLEMENT_API FGamePlatformEntitlementQuery
{
public:
    /** 输入快照须调用期间不可变；非None身份命中Active投影为true，否则false，不进行服务器授权。 */
    static bool HasEntitlement(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        FName EntitlementId);

    /** 只读集合查询；任一Active身份命中为true，空数组false，不修改快照。 */
    static bool HasAny(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        const TArray<FName>& EntitlementIds);

    /** 只读集合查询；全部Active身份命中为true，空数组按现有集合语义true。 */
    static bool HasAll(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        const TArray<FName>& EntitlementIds);

    /** 按中立目标类型/身份查询Active投影，Unknown/None或缺失返回false，不加载项目资源。 */
    static bool HasTarget(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        EGamePlatformEntitlementTargetType TargetType,
        FName TargetId);

    /** 返回输入快照内首个匹配条目的借用指针，缺失nullptr；指针只在快照未变更/未销毁时有效。 */
    static const FGamePlatformEntitlementEntry* FindEntitlement(
        const FGamePlatformEntitlementSnapshot& Snapshot,
        FName EntitlementId);
};
