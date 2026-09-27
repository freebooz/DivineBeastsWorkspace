#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Engine/Texture2D.h"
#include "GamePlatformSlotWidget.generated.h"

/** FGamePlatformUISlotState（游戏平台通用槽位显示状态）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUISlotState
{
    GENERATED_BODY()

    /** 槽位稳定身份，例如Quickbar.0、Ability.Primary；平台不解释业务语义。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    FName SlotId = NAME_None;

    /** 当前内容显示身份；空表示槽位为空。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    FName ContentId = NAME_None;

    /** 图标软引用；禁止槽位控件同步加载资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    TSoftObjectPtr<UTexture2D> Icon;

    /** 可选数量/充能次数；小于0表示不显示。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    int32 Count = INDEX_NONE;

    /** 0～1归一化覆盖比例，可用于冷却/进度遮罩。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    float OverlayProgress = 0.0f;

    /** 当前槽位是否允许交互。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    bool bEnabled = true;

    /** 是否处于Pending（待确认）状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Slot")
    bool bPending = false;
};

/**
 * UGamePlatformSlotWidget（游戏平台通用槽位控件）。
 *
 * 背包物品槽、装备槽、技能槽、快捷栏均可复用本视觉原子。
 * 业务差异由领域ViewModel映射成FGamePlatformUISlotState，不在平台UI中建立业务继承树。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformSlotWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="UI|Slot")
    void ApplySlotState(const FGamePlatformUISlotState& InState);

    UFUNCTION(BlueprintPure, Category="UI|Slot")
    FGamePlatformUISlotState GetSlotState() const { return SlotState; }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Slot", meta=(DisplayName="槽位状态已变化"))
    void BP_OnSlotStateChanged(FGamePlatformUISlotState State);

private:
    UPROPERTY(Transient)
    FGamePlatformUISlotState SlotState;
};
