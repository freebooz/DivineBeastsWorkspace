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

public:
    /** 游戏线程应用已授权的短时反馈；可选命名控件FeedbackText只显示请求值，不读取Gameplay。 */
    virtual void ApplyFeedbackRequest(const FGamePlatformUIFeedbackRequest& InRequest) override;
    /** 合并后刷新累计数值，并重启本次视觉淡出；身份/样式匹配仍由父类验证。 */
    virtual bool MergeFeedbackRequest(const FGamePlatformUIFeedbackRequest& InRequest) override;
    /** 回池清除位移、透明度及动画时钟，下一次复用不能继承上一条状态。 */
    virtual void ResetFeedbackState() override;

protected:
    /** 仅在可见浮字期间插值位置/透明度；不轮询生命、背包、会话或其他业务状态。 */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    /** 单位秒；反馈服务拥有最终回收权，此值只影响本控件视觉。 */
    float VisualElapsedSeconds = 0.0f;
    void RefreshFloatingText();
};
