#pragma once

#include "Components/GamePlatformSlotWidget.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
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

    /** 直接向 Widget Blueprint（界面蓝图）提供单格中文名称、等级、冷却与禁用原因。
     * 仅返回客户端只读 ViewModel 数据，不访问服务端或更改 GAS 属性。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Ability")
    TArray<FDivineBeastsAbilitySlotDetails> GetAbilitySlotDetails() const;

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
    // 自动化夹具验证同一Widget反复构造/失活的真实事件投影，不为测试公开产品接口。
    friend class FDivineBeastsAbilityBarReconstructTest;
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

    void HandleViewModelSlotsChanged(const TArray<FGamePlatformUISlotState>& NewSlots);

    /** 将服务器授权的技能槽投影到五个可选的真实子Widget；未授权槽展示为空且禁用。
     * Passive（被动）按稳定ID前缀识别，其余槽位按现有项目GAS输入标签识别，
     * 不为十二生肖逐个硬编码控件，也不创建客户端技能授权。 */
    void RefreshVisualSlots();

    /** 五个子控件可选绑定：旧WBP及纯代码父类不需要全部具备，缺失不造成UHT失败。 */
    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> PrimaryAbilitySlot;

    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> PassiveAbilitySlot;

    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> ActiveAbilitySlot1;

    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> ActiveAbilitySlot2;

    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> UltimateAbilitySlot;

    /** 该控件唯一拥有的本地 UI ViewModel，与游戏实例和网络身份解耦。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsAbilityBarViewModel> AbilityBarViewModel;
    FDelegateHandle ViewModelSlotsHandle;

    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> AbilitySlots;
};
