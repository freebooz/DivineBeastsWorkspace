#pragma once
#include "Core/GamePlatformWidgetBase.h"
#include "Core/GamePlatformActivatableWidgetBase.h"
#include "GamePlatformUIWindowDragFixture.generated.h"

/** 仅原生回归使用的瞬态具体宿主；不注册页面、不生成视觉资产。 */
UCLASS(Transient, NotBlueprintable)
class UGamePlatformUIWindowDragPlainFixture final : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
public:
    void EndForTest() { NativeDestruct(); }
};

/** 用真实 CommonUI 激活/失活调用链验证捕获清理。 */
UCLASS(Transient, NotBlueprintable)
class UGamePlatformUIWindowDragActiveFixture final : public UGamePlatformActivatableWidgetBase
{
    GENERATED_BODY()
};
