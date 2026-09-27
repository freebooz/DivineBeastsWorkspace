#pragma once

#include "Overlays/GamePlatformOverlayWidget.h"
#include "DivineBeastsOverlayWidget.generated.h"

/**
 * UDivineBeastsOverlayWidget（神兽联盟覆盖界面基类）。
 *
 * 用于交互提示、技能瞄准、Tooltip（悬浮提示）、目标详情和装备比较等临时信息。
 * 覆盖界面必须由事件或 ViewModel 驱动，不持续扫描世界对象。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsOverlayWidget
    : public UGamePlatformOverlayWidget
{
    GENERATED_BODY()
};
