#pragma once

#include "CommonUserWidget.h"
#include "GamePlatformToastWidget.generated.h"

/** 通用短提示；不抢输入焦点。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformToastWidget : public UCommonUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="UI|Toast")
    FName ToastKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="UI|Toast")
    int32 ToastPriority = 0;

    UPROPERTY(BlueprintReadOnly, Category="UI|Toast")
    float DurationSeconds = 2.0f;

    UFUNCTION(BlueprintCallable, Category="UI|Toast")
    void ConfigureToast(FName InToastKey, int32 InPriority, float InDurationSeconds)
    {
        ToastKey = InToastKey;
        ToastPriority = InPriority;
        DurationSeconds = FMath::Max(0.0f, InDurationSeconds);
    }
};
