#pragma once

#include "Layers/GamePlatformUILayerStack.h"
#include "GamePlatformRootLayout.generated.h"

/**
 * UGamePlatformRootLayout（游戏平台根布局基类）。
 *
 * 职责：
 * - 作为每个 LocalPlayer 唯一 UI 根容器的语义扩展点。
 * - 复用 LayerStack（层栈）中的 HUD、Screen、Modal、System、Notification、Loading、Debug 层。
 * - 向 Blueprint 提供当前响应式布局等级，便于 PC / Mobile 使用同一根布局逻辑。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformRootLayout
    : public UGamePlatformUILayerStack
{
    GENERATED_BODY()

public:
    /** 返回当前响应式布局等级；未取得自适应上下文时返回 Regular（标准）。 */
    UFUNCTION(BlueprintPure, Category="UI|RootLayout")
    EGamePlatformUILayoutClass GetCurrentLayoutClass() const
    {
        return GetAdaptiveContext().LayoutClass;
    }
};
