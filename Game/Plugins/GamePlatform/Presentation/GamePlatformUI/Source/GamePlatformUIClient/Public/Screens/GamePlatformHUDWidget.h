#pragma once

#include "CommonUserWidget.h"
#include "GamePlatformHUDWidget.generated.h"

/** 不参与页面栈输入路由的 HUD 基类。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformHUDWidget : public UCommonUserWidget
{
    GENERATED_BODY()
};
