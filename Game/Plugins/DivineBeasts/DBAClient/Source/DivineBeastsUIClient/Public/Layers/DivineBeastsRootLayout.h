#pragma once

#include "Layers/GamePlatformRootLayout.h"
#include "DivineBeastsRootLayout.generated.h"

/**
 * UDivineBeastsRootLayout（神兽联盟根布局基类）。
 *
 * 每个 LocalPlayer 只安装一个项目 RootLayout（根布局）。
 * Blueprint 可基于该类组织 HUD、Screen、Modal、Notification、Loading 等平台层，
 * 并通过平台 Adaptive Context（自适应上下文）响应 PC / Mobile 布局变化。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsRootLayout
    : public UGamePlatformRootLayout
{
    GENERATED_BODY()
};
