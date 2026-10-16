// 平台客户端施法展示：只消费获准的事件快照，瞬态UMG动画负责有限视觉插值；不调度技能或资源。
#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformCastProgressWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UWidgetAnimation;

/**
 * FGamePlatformUICastProgressState（游戏通用施法/引导进度显示快照）。
 *
 * 只表示经过观察者可见性过滤的施法事实。来源适配器负责处理GAS（能力系统）、
 * 网络复制和预测撤销；UI不能据此直接打断、完成或触发一个权威技能。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUICastProgressState
{
    GENERATED_BODY()

    /** 当前LocalPlayer/观察目标的数据源作用域；切地图或换角色必须更换。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    FGuid SourceScopeId;

    /** 本次施法实例身份；同类技能连续施放应具有不同实例身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    FName CastInstanceId = NAME_None;

    /** 允许展示的施法者身份；未获许可时可以留空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    FName CasterDisplayId = NAME_None;

    /** 已本地化且获准展示的技能名称。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    FText DisplayName;

    /** 预计总时长（秒），0表示来源不提供进度比例。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    double TotalSeconds = 0.0;

    /** 本事件发生时上游确认的剩余时长（秒）；仅启动有限视觉曲线，不推断权威完成。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    double RemainingSeconds = 0.0;

    /** 活跃状态由来源给定；false表示已结束/中断/撤销，不代表成功施放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    bool bActive = false;

    /** true为Channel（引导），false为普通Cast（施法）；显示方向由蓝图决定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    bool bChanneling = false;

    /** 可打断标记仅影响视觉样式，不授予任何打断权限。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    bool bInterruptible = false;

    /** 由领域事实明确给出的关键施法标记，供高级警告样式选取。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    bool bCritical = false;

    /** 所有施法实例在同一来源下的单调递增修订号，防迟到回调。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Cast")
    int64 Revision = -1;
};

/** FGamePlatformUICastProgressPresentation（施法显示纯函数，无世界/网络依赖）。 */
struct GAMEPLATFORMUICLIENT_API FGamePlatformUICastProgressPresentation
{
    /** 输入校验：身份与来源有效、时间有限且有合理上限。 */
    static bool IsValidSnapshot(const FGamePlatformUICastProgressState& InState);

    /** 返回0~1的经过比例；总时长未知时返回0，并由蓝图选择不定进度样式。 */
    static float GetNormalizedProgress(const FGamePlatformUICastProgressState& InState);
};

/**
 * UGamePlatformCastProgressWidget（通用施法条/引导条基类）。
 *
 * 生命周期：BindCastSource（绑定来源）→ ApplyCastSnapshot（按事件更新）
 * → ClearCastSource（解绑清空）。禁止Tick、客户端业务施法判定及直接遍历GameplayActor。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformCastProgressWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 切换观察者/世界时先清空旧施法；返回false表示无效来源。 */
    UFUNCTION(BlueprintCallable, Category="UI|Cast")
    bool BindCastSource(FGuid InScopeId);

    /** 仅接受当前作用域中严格递增的合法快照，拒绝迟到或无权限来源。 */
    UFUNCTION(BlueprintCallable, Category="UI|Cast")
    bool ApplyCastSnapshot(const FGamePlatformUICastProgressState& InState);

    /** 断开绑定、清空控件；不会中断真实施法。 */
    UFUNCTION(BlueprintCallable, Category="UI|Cast")
    void ClearCastSource();

    /** 当前只读显示快照。 */
    UFUNCTION(BlueprintPure, Category="UI|Cast")
    FGamePlatformUICastProgressState GetCastSnapshot() const { return State; }

    /** 普通施法的已完成进度或引导消耗比例，0~1。 */
    UFUNCTION(BlueprintPure, Category="UI|Cast")
    float GetNormalizedCastProgress() const
    {
        return FGamePlatformUICastProgressPresentation::GetNormalizedProgress(State);
    }

protected:
    /** 从父视图移除时撤销本控件自有动画与来源；重新显示须由作用域适配器首次快照重绑。 */
    virtual void NativeDestruct() override;
    /** 可选绑定命名进度条；缺少控件时蓝图可自定义样式。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|Cast")
    TObjectPtr<UProgressBar> CastProgressBar = nullptr;

    /** 可选绑定已本地化技能名称文本。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|Cast")
    TObjectPtr<UTextBlock> CastNameText = nullptr;

    /** 活跃/结束/取消均走同一个变化事件，蓝图不得自行改变业务状态。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Cast",
        meta=(DisplayName="施法显示状态已变化"))
    void BP_OnCastStateChanged(FGamePlatformUICastProgressState UpdatedState);

private:
    /** 每次受理绑定/快照/清理都换代；防同步可见性/字段通知清空重绑到同一Scope和Revision的ABA重入。 */
    FGuid DisplayUpdateId;
    /** 当前事件自有的瞬态引擎动画；从不保存成资产，不查询世界/GAS，也不在完成时改bActive。 */
    UPROPERTY(Transient)
    TObjectPtr<UWidgetAnimation> CastVisualAnimation;
    /** 按本次已接受快照建立Percent线性曲线；未知/零剩余时长不伪造动画。仅游戏线程。 */
    void PlayCastVisualAnimation();
    /** 先撤销本控件所有权再停止引擎动画，避免停止回调误清同步创建的后继动画。 */
    void StopCastVisualAnimation();
    /** 当前上下文的随机作用域，只接受相同SourceScopeId。 */
    UPROPERTY(Transient)
    FGuid BoundScopeId;

    /** 当前控件只读状态。 */
    UPROPERTY(Transient)
    FGamePlatformUICastProgressState State;
};
