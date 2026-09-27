#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Engine/Texture2D.h"
#include "GamePlatformPortraitWidget.generated.h"

/** FGamePlatformUIPortraitState（游戏平台通用肖像状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIPortraitState
{
    GENERATED_BODY()

    /** 稳定显示对象身份；不要求等于后端主键。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Portrait")
    FName DisplayId = NAME_None;

    /** 本地化后的显示名称。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Portrait")
    FText DisplayName;

    /** 可选等级；小于0表示不显示等级。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Portrait")
    int32 Level = INDEX_NONE;

    /** 肖像软引用；控件不得同步加载大纹理。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Portrait")
    TSoftObjectPtr<UTexture2D> PortraitTexture;

    /** 可选状态身份，例如 Online、Dead、Ready；由上层样式解释。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Portrait")
    FName StatusId = NAME_None;

    /** 是否高亮。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Portrait")
    bool bHighlighted = false;
};

/**
 * UGamePlatformPortraitWidget（游戏平台通用肖像控件）。
 *
 * 用于玩家头像、英雄肖像、目标肖像、队友肖像等。
 * 平台只传递软资源与显示状态，具体头像框、品质、项目美术由上层Blueprint/Style处理。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformPortraitWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|Portrait")
    void ApplyPortraitState(const FGamePlatformUIPortraitState& InState);

    UFUNCTION(BlueprintPure, Category="UI|Portrait")
    FGamePlatformUIPortraitState GetPortraitState() const
    {
        return PortraitState;
    }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Portrait", meta=(DisplayName="肖像状态已变化"))
    void BP_OnPortraitStateChanged(FGamePlatformUIPortraitState State);

private:
    UPROPERTY(Transient)
    FGamePlatformUIPortraitState PortraitState;
};
