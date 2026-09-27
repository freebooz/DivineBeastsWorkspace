#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/NoExportTypes.h"
#include "GamePlatformVersion.generated.h"

/**
 * 三段非负数值版本；默认0.0.0有效，不支持预发布、构建元数据或自动兼容策略。
 * 各分量范围0..INT32_MAX。此值与逻辑身份版本、UE版本、协议版本分别由所属领域解释。
 * 无资源所有权；独立值可在任意线程操作，共享可变实例需调用者同步。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMCORE_API FGamePlatformVersion
{
    GENERATED_BODY()

    /** 主版本数字；平台不根据它自动接受或拒绝兼容。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core", meta=(ClampMin="0"))
    int32 Major = 0;

    /** 次版本数字，比较优先级低于主版本。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core", meta=(ClampMin="0"))
    int32 Minor = 0;

    /** 补丁数字，比较优先级最低。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GamePlatform|Core", meta=(ClampMin="0"))
    int32 Patch = 0;

    /** 三个分量均非负才有效；不修改编辑后的值。 */
    bool IsValid() const;

    /** 返回无前导零的Major.Minor.Patch；非法值返回空字符串。 */
    FString ToString() const;

    /** 严格解析三个无符号十进制分量；拒绝空白、符号、前导零与溢出，失败将OutVersion置0.0.0。 */
    static bool TryParse(const FString& Text, FGamePlatformVersion& OutVersion);

    /** 按Major、Minor、Patch顺序返回-1/0/1；支持所有int32值排序，业务接纳前须分别检查IsValid。 */
    int32 Compare(const FGamePlatformVersion& Other) const;

    /** 数字完全相等；不表示两版本之间兼容。 */
    bool operator==(const FGamePlatformVersion& Other) const { return Compare(Other) == 0; }

    /** 数字不相等；不表示两版本之间不兼容。 */
    bool operator!=(const FGamePlatformVersion& Other) const { return !(*this == Other); }
};
