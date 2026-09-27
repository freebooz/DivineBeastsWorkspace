#pragma once

#include "CommonActivatableWidget.h"
#include "GamePlatformUITypes.h"
#include "GamePlatformUIScreen.generated.h"

class UGamePlatformViewModelBase;
class UWidget;

class UGamePlatformUIScreen;
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformUIScreenDeactivatedNative,
    UGamePlatformUIScreen*);

/** CommonUI 页面基类；负责激活、关闭、Back、焦点和 ViewModel 生命周期。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformUIScreen : public UCommonActivatableWidget
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

    UFUNCTION(BlueprintPure, Category="UI|Screen")
    UGamePlatformViewModelBase* GetViewModel() const { return ViewModel; }

    EGamePlatformUIInputMode GetInputMode() const { return InputMode; }
    EGamePlatformUIPausePolicy GetPausePolicy() const { return PausePolicy; }
    virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
    FGamePlatformUIScreenDeactivatedNative& OnPlatformDeactivated() { return PlatformDeactivated; }

protected:
    virtual void NativeOnActivated() override;
    virtual void NativeOnDeactivated() override;
    virtual bool NativeOnHandleBackAction() override;
    virtual UWidget* NativeGetDesiredFocusTarget() const override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Screen")
    bool bAllowBack = true;

    FGamePlatformUIScreenDeactivatedNative PlatformDeactivated;

private:
    UPROPERTY(Transient)
    FName ScreenId = NAME_None;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformViewModelBase> ViewModel = nullptr;

    UPROPERTY(Transient)
    FName DesiredFocusWidgetName = NAME_None;

    UPROPERTY(Transient)
    EGamePlatformUIInputMode InputMode = EGamePlatformUIInputMode::GameAndUI;

    UPROPERTY(Transient)
    EGamePlatformUIPausePolicy PausePolicy = EGamePlatformUIPausePolicy::Never;
};
