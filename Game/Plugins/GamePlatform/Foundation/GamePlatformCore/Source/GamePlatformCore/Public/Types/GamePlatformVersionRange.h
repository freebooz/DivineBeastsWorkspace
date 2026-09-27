#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformVersion.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "GamePlatformVersionRange.generated.h"

/**
 * FGamePlatformVersionRange（平台版本兼容区间）。
 * 表示闭区间[MinimumInclusive, MaximumInclusive]；默认未配置并按Fail Closed（失败关闭）拒绝所有版本。
 * 本类型只表达版本范围，不推导协议、插件、内容或网络兼容政策，具体接纳规则仍由所属领域决定。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCORE_API FGamePlatformVersionRange
{
    GENERATED_BODY()

    /** 未显式配置时整个区间无效，Contains始终返回false。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    bool bConfigured = false;

    /** 最低可接受版本，包含边界。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    FGamePlatformVersion MinimumInclusive;

    /** 最高可接受版本，包含边界。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core")
    FGamePlatformVersion MaximumInclusive;

    /** 显式创建闭区间；输入无效或上下界逆序时返回未配置的无效区间。 */
    static FGamePlatformVersionRange Inclusive(
        const FGamePlatformVersion& Minimum,
        const FGamePlatformVersion& Maximum);

    /** 必须已配置、两个边界有效且Minimum<=Maximum。 */
    bool IsValid() const;

    /** 仅在区间和Candidate均有效且位于闭区间内时返回true。 */
    bool Contains(const FGamePlatformVersion& Candidate) const;

    /** 有效时返回min..max；无效时返回空字符串。 */
    FString ToString() const;
};
