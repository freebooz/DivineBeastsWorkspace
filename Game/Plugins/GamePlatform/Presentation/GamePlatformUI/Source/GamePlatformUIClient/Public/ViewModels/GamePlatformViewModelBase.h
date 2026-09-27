#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GamePlatformViewModelBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FGamePlatformUIViewStateChanged,
    int32, Revision,
    int32, PageGeneration);

/** 事件驱动的平台 ViewModel 基类；不强制依赖 UE MVVM Beta 插件。 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformViewModelBase : public UObject
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void BeginPage();

    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void EndPage();

    UFUNCTION(BlueprintCallable, Category="UI|ViewModel")
    void MarkStateChanged();

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    bool IsCallbackCurrent(int32 ExpectedRevision, int32 ExpectedPageGeneration) const;

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    int32 GetRevision() const { return Revision; }

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    int32 GetPageGeneration() const { return PageGeneration; }

    UFUNCTION(BlueprintPure, Category="UI|ViewModel")
    bool IsPageActive() const { return bPageActive; }

    UPROPERTY(BlueprintAssignable, Category="UI|ViewModel")
    FGamePlatformUIViewStateChanged OnViewStateChanged;

private:
    UPROPERTY(Transient)
    int32 Revision = 0;

    UPROPERTY(Transient)
    int32 PageGeneration = 0;

    UPROPERTY(Transient)
    bool bPageActive = false;
};
