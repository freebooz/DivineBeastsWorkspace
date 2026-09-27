#pragma once

#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "GamePlatformWorldNameplateWidget.generated.h"

/** UGamePlatformWorldNameplateWidget（游戏平台世界名称板基类）：玩家、NPC、敌方名称与世界血条的共用表面。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldNameplateWidget
    : public UGamePlatformWorldWidgetBase
{
    GENERATED_BODY()
};
