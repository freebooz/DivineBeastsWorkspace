#pragma once

#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "GamePlatformWorldMarkerWidget.generated.h"

/** UGamePlatformWorldMarkerWidget（游戏平台世界标记基类）：任务、交互、队伍、目标、Ping等共用机制基类。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldMarkerWidget
    : public UGamePlatformWorldWidgetBase
{
    GENERATED_BODY()
};
