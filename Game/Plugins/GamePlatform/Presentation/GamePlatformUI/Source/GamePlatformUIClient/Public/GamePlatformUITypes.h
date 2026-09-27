#pragma once

#include "CoreMinimal.h"
#include "GamePlatformUITypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformUILayer : uint8
{
    HUD,
    Screen,
    Modal,
    System,
    Notification,
    Loading,
    Debug
};

UENUM(BlueprintType)
enum class EGamePlatformUIInputMode : uint8
{
    GameOnly,
    UIOnly,
    GameAndUI
};

UENUM(BlueprintType)
enum class EGamePlatformUIPausePolicy : uint8
{
    Never,
    StandaloneOnly
};

UENUM(BlueprintType)
enum class EGamePlatformUITransition : uint8
{
    None,
    Default,
    Instant
};

/** 跨游戏可访问性偏好钩子；视觉资产由上层Blueprint/Style消费。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIAccessibilityPreferences
{
    GENERATED_BODY()

    /** 文本缩放倍率；Manager会拒绝不可读的过小值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    float TextScale = 1.0f;

    /** 触控目标放大倍率；可访问性设置不允许把目标缩得比默认更小。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    float TouchTargetScale = 1.0f;

    /** 减少非必要界面动画；默认Transition会降级为Instant。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    bool bReducedMotion = false;

    /** 请求高对比度样式；具体样式资源由客户端组合层提供。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    bool bPreferHighContrast = false;

    /** 状态表达需要图标/文本等非颜色线索，不能只依赖颜色。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Accessibility")
    bool bRequireNonColorStatusCues = true;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIAsyncRequest
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    FGuid RequestId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    FName ScreenId = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI")
    int32 Generation = 0;

    bool IsValid() const { return RequestId.IsValid() && !ScreenId.IsNone(); }
};
