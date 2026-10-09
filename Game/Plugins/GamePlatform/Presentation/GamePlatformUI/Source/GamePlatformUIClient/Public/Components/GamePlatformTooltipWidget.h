#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Engine/Texture2D.h"
#include "GamePlatformTooltipWidget.generated.h"

/** FGamePlatformUITooltipState（通用悬浮提示只读状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUITooltipState
{
    GENERATED_BODY()
    /** 当前可见内容身份；用于防止迟到异步请求覆盖新提示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Tooltip")
    FName ContentId = NAME_None;
    /** 已本地化标题。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Tooltip")
    FText Title;
    /** 已本地化正文，不包含技术堆栈/隐私字段。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Tooltip")
    FText Description;
    /** 图标软资源由视觉蓝图异步读取，禁止同步加载。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Tooltip")
    TSoftObjectPtr<UTexture2D> Icon;
    /** 当前显示内容版本。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Tooltip")
    int64 Revision = -1;
};

/** UGamePlatformTooltipWidget（技能、装备、地图标记等复用提示控件）。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformTooltipWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="UI|Tooltip")
    bool ApplyTooltip(const FGamePlatformUITooltipState& InState);
    UFUNCTION(BlueprintCallable, Category="UI|Tooltip")
    void ClearTooltip();
    UFUNCTION(BlueprintPure, Category="UI|Tooltip")
    FGamePlatformUITooltipState GetTooltip() const { return State; }
protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Tooltip",
        meta=(DisplayName="悬浮提示已更新"))
    void BP_OnTooltipChanged(FGamePlatformUITooltipState UpdatedState);
private:
    UPROPERTY(Transient)
    FGamePlatformUITooltipState State;
};
