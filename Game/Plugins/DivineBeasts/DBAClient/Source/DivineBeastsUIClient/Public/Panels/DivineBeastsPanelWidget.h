#pragma once

#include "Panels/GamePlatformPanelWidget.h"
#include "DivineBeastsPanelWidget.generated.h"

/**
 * UDivineBeastsPanelWidget（神兽联盟功能面板基类）。
 *
 * 用于玩家状态、技能、背包、装备、任务、队伍等复合界面内部面板。
 * 项目面板继续复用平台事件和自适应能力，不自行维护页面导航栈。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPanelWidget
    : public UGamePlatformPanelWidget
{
    GENERATED_BODY()
};
