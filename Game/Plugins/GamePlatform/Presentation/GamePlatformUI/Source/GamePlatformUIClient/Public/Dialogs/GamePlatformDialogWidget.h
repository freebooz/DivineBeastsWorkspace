#pragma once

#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformDialogWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGamePlatformDialogResolved, FName, ResultId);

/** 一次性 Resolve 的通用对话框基类。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformDialogWidget : public UGamePlatformUIScreen
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
