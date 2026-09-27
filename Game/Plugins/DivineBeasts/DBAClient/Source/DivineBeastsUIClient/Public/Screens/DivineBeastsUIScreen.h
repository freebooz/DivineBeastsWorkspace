#pragma once

#include "CoreMinimal.h"
#include "Screens/GamePlatformUIScreen.h"
#include "DivineBeastsUIScreen.generated.h"

/**
 * UDivineBeastsUIScreen（神兽联盟项目页面基类）。
 * 视觉页面Blueprint应继承它，实际页面栈仍由GamePlatformUI管理。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsUIScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
};
