#pragma once

#include "Screens/GamePlatformModalScreen.h"
#include "GamePlatformDialogWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamePlatformDialogResolved, FName, ResultId);

/**
 * UGamePlatformDialogWidget（游戏平台通用对话框基类）。
 *
 * Dialog（对话框）本质是阻断下层输入的结构化 Modal（模态页面），
 * 因此固定继承 UGamePlatformModalScreen，避免与普通 Screen 形成平级重复体系。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformDialogWidget : public UGamePlatformModalScreen
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|Dialog")
    bool Resolve(FName ResultId);

    UPROPERTY(BlueprintAssignable, Category="UI|Dialog")
    FGamePlatformDialogResolved OnResolved;

protected:
    virtual void NativeOnActivated() override;

private:
    UPROPERTY(Transient)
    bool bResolved = false;
};
