#pragma once

#include "Components/GamePlatformSlotWidget.h"
#include "Panels/DivineBeastsPanelWidget.h"
#include "DivineBeastsAbilityBarPanel.generated.h"

/**
 * UDivineBeastsAbilityBarPanel（神兽联盟技能条面板）。
 *
 * 技能领域适配器把技能图标、充能、冷却和可用状态映射成平台通用 SlotState（槽位状态）。
 * 本类只保存UI投影数组，不直接访问ASC，不创建第二套技能模型。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsAbilityBarPanel
    : public UDivineBeastsPanelWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    void ApplyAbilitySlots(const TArray<FGamePlatformUISlotState>& InSlots);

    /**
     * C++高频读取接口；返回const引用避免复制槽位数组。
     * 不暴露为UFUNCTION，Blueprint使用下方值返回接口。
     */
    const TArray<FGamePlatformUISlotState>& GetAbilitySlotsView() const
    {
        return AbilitySlots;
    }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FGamePlatformUISlotState> GetAbilitySlots() const
    {
        return AbilitySlots;
    }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Ability", meta=(DisplayName="技能槽视图已变化"))
    void BP_OnAbilitySlotsChanged();

private:
    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> AbilitySlots;
};
