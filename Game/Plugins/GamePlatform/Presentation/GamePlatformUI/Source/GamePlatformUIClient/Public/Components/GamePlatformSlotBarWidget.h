#pragma once

#include "Components/GamePlatformComponentWidget.h"
#include "Components/GamePlatformSlotWidget.h"
#include "GamePlatformSlotBarWidget.generated.h"

/**
 * FGamePlatformUISlotBarState（通用槽位条的只读UI状态）。
 * 可用于技能栏、物品快捷栏、装备操作栏、表情栏，平台不判断技能或物品是否合法。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUISlotBarState
{
    GENERATED_BODY()
    /** 槽位条语义标识；同一Widget切换身份时允许从低版本开始。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|SlotBar")
    FName BarId = NAME_None;
    /** 当前业务数据源的单调修订号，非网络权威版本。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|SlotBar")
    int64 Revision = -1;
    /** 实际投影的槽位列表；不包含业务对象引用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|SlotBar")
    TArray<FGamePlatformUISlotState> Slots;
};

/** UGamePlatformSlotBarWidget（通用横向/纵向技能与快捷栏组件）。
 * 布局方向、图标、快捷键样式由内容插件蓝图决定，不绑定特定GameplayAbility。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformSlotBarWidget
    : public UGamePlatformComponentWidget
{
    GENERATED_BODY()
public:
    /** 严格校验容量、身份和非有限进度；按版本驱动一次完整刷新。 */
    UFUNCTION(BlueprintCallable, Category="UI|SlotBar")
    bool ApplySlotBarState(const FGamePlatformUISlotBarState& NewState);
    UFUNCTION(BlueprintPure, Category="UI|SlotBar")
    FGamePlatformUISlotBarState GetSlotBarState() const { return State; }
    const FGamePlatformUISlotBarState& GetSlotBarStateView() const { return State; }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|SlotBar",
        meta=(DisplayName="槽位栏状态已更新"))
    void BP_OnSlotBarChanged(FGamePlatformUISlotBarState NewState);

private:
    UPROPERTY(Transient)
    FGamePlatformUISlotBarState State;
};
