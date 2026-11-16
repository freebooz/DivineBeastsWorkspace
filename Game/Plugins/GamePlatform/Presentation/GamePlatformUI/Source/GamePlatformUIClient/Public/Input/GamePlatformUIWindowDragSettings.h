#pragma once
#include "CoreMinimal.h"
#include "GamePlatformUIWindowDragSettings.generated.h"

/** 通用窗口拖拽声明。名称仅解析当前宿主 WidgetTree；不拥有业务或持久化位置。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIWindowDragSettings
{
    GENERATED_BODY()

    /** 默认关闭，保持普通 HUD、全屏页面及世界 UI 的输入合同。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Window")
    bool bEnabled = false;

    /** 可见窗口边界及顶部抓取区所用控件；缺失时拒绝拖动，不搜索其他窗口。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Window", meta=(EditCondition="bEnabled"))
    FName BoundsWidgetName = NAME_None;

    /** 同时平移的根节点，不允许重复或祖先/子孙重叠；空数组采用边界控件。遮罩不应列入。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Window", meta=(EditCondition="bEnabled"))
    TArray<FName> MoveTargetWidgetNames;

    /** 从窗口边界顶部开始的抓取带高度，单位为该控件逻辑像素；子控件先消费鼠标。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Window", meta=(ClampMin="1.0", EditCondition="bEnabled"))
    float HeaderHeight = 40.f;

    /** 视口安全区内额外保留的逻辑像素；不缩放文字或控件。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="UI|Window", meta=(ClampMin="0.0", EditCondition="bEnabled"))
    float ViewportInset = 8.f;
};
