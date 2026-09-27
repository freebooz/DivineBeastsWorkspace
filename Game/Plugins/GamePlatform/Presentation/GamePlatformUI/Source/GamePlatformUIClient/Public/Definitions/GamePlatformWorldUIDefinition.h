#pragma once

#include "Definitions/GamePlatformUIDefinitionBase.h"
#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "GamePlatformWorldUIDefinition.generated.h"

/** UGamePlatformWorldUIDefinition（游戏平台世界投影UI定义）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldUIDefinition
    : public UGamePlatformUIDefinitionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|WorldUI")
    TSoftClassPtr<UGamePlatformWorldWidgetBase> WidgetClass;

    /** 默认最大可见距离，单位厘米；0表示不做距离裁剪。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|WorldUI", meta=(ClampMin="0.0"))
    float DefaultMaxVisibleDistance = 0.0f;

    /** 默认是否允许屏幕边缘钳制。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|WorldUI")
    bool bClampToViewport = false;

    virtual FGamePlatformResult ValidateDefinition() const override;
};
