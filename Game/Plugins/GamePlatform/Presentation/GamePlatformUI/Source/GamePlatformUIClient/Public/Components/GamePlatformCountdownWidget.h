#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformCountdownWidget.generated.h"

class UTextBlock;

/** FGamePlatformUICountdownState（通用计时显示快照）。
 * 来源可以是服务器复制的结束时间、技能冷却或本地教学状态，但控件不推算权威剩余时间。
 * 上层适配器应按必要的显示节奏推送更新，不允许Widget逐帧轮询游戏状态。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUICountdownState
{
    GENERATED_BODY()

    /** 计时器语义身份，供蓝图选择不同外观；空ID为无效状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Countdown")
    FName CountdownId = NAME_None;

    /** 来源修订号，同一身份必须严格递增。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Countdown")
    int64 Revision = -1;

    /** 剩余时间，单位秒；未知时不应构造假的倒计时。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Countdown")
    double RemainingSeconds = 0.0;

    /** 总时长，单位秒；0表示没有确定总时长，剩余秒数仍可展示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Countdown")
    double TotalSeconds = 0.0;

    /** 计时事件是否仍处于进行状态；false显示结束，不推断业务完成。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Countdown")
    bool bActive = false;

    /** 暂停只改变显示态，不代替业务计时器控制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Countdown")
    bool bPaused = false;
};

/**
 * UGamePlatformCountdownWidget（通用倒计时控件）。
 * 复用范围：匹配准备、比赛阶段、技能、任务、教学和活动期限。
 * 不启用Tick，不创建Timer，不自行补秒；由已授权数据源按需提供快照。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformCountdownWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 拒绝无效身份、非有限时间、反向版本及异常超长值，成功才广播。 */
    UFUNCTION(BlueprintCallable, Category="UI|Countdown")
    bool ApplyCountdownState(const FGamePlatformUICountdownState& InState);

    /** 当前展示快照，仅用于界面；不是服务器时间真源。 */
    UFUNCTION(BlueprintPure, Category="UI|Countdown")
    FGamePlatformUICountdownState GetCountdownState() const { return State; }

    /** 生成hh:mm:ss或mm:ss显示文本；不改变状态。 */
    UFUNCTION(BlueprintPure, Category="UI|Countdown")
    FText GetCountdownText() const;

protected:
    /** 蓝图可选择绑定文字；未绑定时仍保留事件投影自定义外观。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|Countdown")
    TObjectPtr<UTextBlock> CountdownText = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="UI|Countdown",
        meta=(DisplayName="倒计时状态已更新"))
    void BP_OnCountdownChanged(FGamePlatformUICountdownState UpdatedState);

private:
    UPROPERTY(Transient)
    FGamePlatformUICountdownState State;
};
