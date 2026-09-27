#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "GamePlatformResourceBarWidget.generated.h"

class UProgressBar;

/** FGamePlatformUIResourceBarState（游戏平台通用资源条状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIResourceBarState
{
    GENERATED_BODY()

    /** 当前值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|ResourceBar")
    double CurrentValue = 0.0;

    /** 最大值；小于等于0时归一化比例为0。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|ResourceBar")
    double MaximumValue = 0.0;

    /** 可选语义身份，例如 Health、Shield、Energy；平台只用于样式映射。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|ResourceBar")
    FName ResourceId = NAME_None;

    /** 当前值是否需要显示文本。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|ResourceBar")
    bool bShowValueText = true;

    double GetNormalizedValue() const
    {
        return MaximumValue > KINDA_SMALL_NUMBER
            ? FMath::Clamp(CurrentValue / MaximumValue, 0.0, 1.0)
            : 0.0;
    }
};

/**
 * UGamePlatformResourceBarWidget（游戏平台通用资源条控件）。
 *
 * 可作为生命、护盾、能量、经验、施法进度等视觉基础。
 * 业务层通过事件调用 ApplyResourceState；本控件不主动查询角色/ASC，也不启用Tick。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformResourceBarWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    /** 仅在状态真实变化时更新底层ProgressBar并通知Blueprint。 */
    UFUNCTION(BlueprintCallable, Category="UI|ResourceBar")
    void ApplyResourceState(const FGamePlatformUIResourceBarState& InState);

    UFUNCTION(BlueprintPure, Category="UI|ResourceBar")
    FGamePlatformUIResourceBarState GetResourceState() const
    {
        return ResourceState;
    }

protected:
    /** 蓝图可选绑定标准UMG进度条；没有绑定时仍可自定义完整视觉。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="UI|ResourceBar")
    TObjectPtr<UProgressBar> ProgressBar = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="UI|ResourceBar", meta=(DisplayName="资源条状态已变化"))
    void BP_OnResourceStateChanged(FGamePlatformUIResourceBarState State);

private:
    UPROPERTY(Transient)
    FGamePlatformUIResourceBarState ResourceState;
};
