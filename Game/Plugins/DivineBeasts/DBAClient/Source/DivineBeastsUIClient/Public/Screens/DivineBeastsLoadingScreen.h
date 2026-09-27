#pragma once

#include "Screens/GamePlatformLoadingScreen.h"
#include "DivineBeastsLoadingScreen.generated.h"

/**
 * UDivineBeastsLoadingScreen（神兽联盟加载页面基类）。
 *
 * 所有项目加载页面必须从本类继承，并消费真实 Loading Snapshot（加载快照）。
 * 不允许使用假计时器伪造 0～100% 进度。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsLoadingScreen
    : public UGamePlatformLoadingScreen
{
    GENERATED_BODY()
};
