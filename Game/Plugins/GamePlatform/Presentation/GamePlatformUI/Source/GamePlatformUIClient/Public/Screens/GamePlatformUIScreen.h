#pragma once

#include "Core/GamePlatformActivatableWidgetBase.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUIScreen.generated.h"

class UGamePlatformViewModelBase;
class UWidget;

class UGamePlatformUIScreen;
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUIScreenDeactivatedNative,
    UGamePlatformUIScreen*);

/**
 * UGamePlatformUIScreen（游戏平台页面基类）。
 *
 * 职责：
 * - 在 UGamePlatformActivatableWidgetBase 的事件驱动生命周期上增加页面身份、Back、焦点和输入策略。
 * - 供 Menu、Modal、Loading、System 以及项目业务 Screen 单向继承。
 * - 页面不得直接 AddToViewport，由 UGamePlatformUIManagerSubsystem 统一放入 CommonUI 层栈。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIScreen
    : public UGamePlatformActivatableWidgetBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|Screen")
    void InitializeScreen(
        FName InScreenId,
        UGamePlatformViewModelBase* InViewModel,
        FName InDesiredFocusWidgetName,
        EGamePlatformUIInputMode InInputMode,
        EGamePlatformUIPausePolicy InPausePolicy);

    UFUNCTION(BlueprintCallable, Category="UI|Screen")
    void CloseScreen();

    UFUNCTION(BlueprintPure, Category="UI|Screen")
    FName GetScreenId() const { return ScreenId; }

    /** 返回当前页面 ViewModel；保留原接口名称以兼容既有项目层代码。 */
    UFUNCTION(BlueprintPure, Category="UI|Screen")
    UGamePlatformViewModelBase* GetViewModel() const
    {
        return GetPlatformViewModel();
    }

    EGamePlatformUIInputMode GetInputMode() const { return InputMode; }
    EGamePlatformUIPausePolicy GetPausePolicy() const { return PausePolicy; }
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    FGamePlatformUIScreenDeactivatedNative& OnPlatformReleased() { return PlatformReleased; }
    FGamePlatformUIScreenDeactivatedNative& OnPlatformDeactivated() { return PlatformDeactivated; }

protected:
    virtual void NativeDestruct() override;
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Screen")
    bool bAllowBack = true;

    FGamePlatformUIScreenDeactivatedNative PlatformDeactivated;
    FGamePlatformUIScreenDeactivatedNative PlatformReleased;

private:
    UPROPERTY(Transient)
    FName ScreenId = NAME_None;

    UPROPERTY(Transient)
    FName DesiredFocusWidgetName = NAME_None;

    UPROPERTY(Transient)
    EGamePlatformUIInputMode InputMode = EGamePlatformUIInputMode::GameAndUI;

    UPROPERTY(Transient)
    EGamePlatformUIPausePolicy PausePolicy = EGamePlatformUIPausePolicy::Never;
};
