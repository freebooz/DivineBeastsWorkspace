#pragma once

#include "Feedback/GamePlatformFeedbackWidget.h"
#include "GamePlatformFloatingTextWidget.generated.h"

/**
 * UGamePlatformFloatingTextWidget（游戏平台浮动文字基类）。
 *
 * 适用于伤害、治疗、护盾、拾取数量等短生命周期文本反馈。
 * 具体颜色、字体、轨迹和动画全部由 Definition / Style / Blueprint 提供，平台层不建立业务子类。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformFloatingTextWidget
    : public UGamePlatformFeedbackWidget
{
    GENERATED_BODY()
};
