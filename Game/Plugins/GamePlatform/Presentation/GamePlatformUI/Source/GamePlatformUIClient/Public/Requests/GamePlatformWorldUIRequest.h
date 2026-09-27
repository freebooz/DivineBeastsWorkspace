#pragma once

#include "CoreMinimal.h"
#include "GamePlatformWorldUIRequest.generated.h"

/**
 * FGamePlatformWorldUIRequest（游戏平台世界投影界面请求）。
 *
 * 用于名称板、世界血条、任务标记、队友标记和导航标记等需要持续世界坐标投影的 UI。
 * 请求只保存轻量显示数据和世界位置，不持有 Gameplay Actor 的强引用。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformWorldUIRequest
{
    GENERATED_BODY()

    /** 注册实例身份；未设置时由服务生成。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    FGuid RequestId;

    /** 稳定表面键，用于调用方更新或替换同一世界UI。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    FName SurfaceKey = NAME_None;

    /** 数据驱动样式身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    FName StyleId = NAME_None;

    /** 主文本，例如角色名、NPC名或任务名。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    FText PrimaryText;

    /** 辅助文本，例如等级、距离或副标题。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    FText SecondaryText;

    /** 当前世界坐标；移动实体由领域 Adapter 按事实变化更新该位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    FVector WorldLocation = FVector::ZeroVector;

    /** 最大可见距离，单位厘米；小于等于0表示不做距离裁剪。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    float MaxVisibleDistance = 0.0f;

    /** 屏幕外是否允许钳制到边缘；首版仅保留策略位，由具体样式按需使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    bool bClampToViewport = false;

    /** 同屏数量较多时用于选择保留顺序。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|WorldUI")
    int32 Priority = 0;

    bool IsValid() const
    {
        return !SurfaceKey.IsNone() || !StyleId.IsNone();
    }
};
