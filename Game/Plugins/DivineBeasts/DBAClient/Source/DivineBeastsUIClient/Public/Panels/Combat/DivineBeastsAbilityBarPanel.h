#pragma once

#include "Components/GamePlatformSlotWidget.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "DivineBeastsAbilityBarPanel.generated.h"

class UDivineBeastsAbilityBarViewModel;
class APawn;

/**
 * UDivineBeastsAbilityBarPanel（神兽联盟技能条面板）。
 *
 * 技能领域适配器把技能图标、充能、冷却和可用状态映射成平台通用 SlotState（槽位状态）。
 * 本类只保存UI投影数组，不直接访问ASC，不创建第二套技能模型。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsAbilityBarPanel
    : public UDivineBeastsCombatPanelBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    void ApplyAbilitySlots(const TArray<FGamePlatformUISlotState>& InSlots);

    /** 重新绑定当前拥有者 Pawn 的真实技能授予快照；暂无 Pawn 时展示空槽位并明确返回 false。
     * Pawn 更换/重新附身时通过 Controller 的 OnPossessedPawnChanged 自动调用。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability")
    bool RefreshAbilitySourceFromOwningPawn();

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
    /** Widget 构造/销毁订阅当前玩家换 Pawn 事件；不以 Tick 轮询角色对象。 */
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Ability", meta=(DisplayName="技能槽视图已变化"))
    void BP_OnAbilitySlotsChanged();

private:
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

    void HandleViewModelSlotsChanged(const TArray<FGamePlatformUISlotState>& NewSlots);

    /** 该控件唯一拥有的本地 UI ViewModel，与游戏实例和网络身份解耦。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsAbilityBarViewModel> AbilityBarViewModel;
    FDelegateHandle ViewModelSlotsHandle;

    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> AbilitySlots;
};
