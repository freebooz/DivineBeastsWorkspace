#pragma once

#include "Components/GamePlatformSlotWidget.h"
#include "Panels/Combat/DivineBeastsCombatPanelBase.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "DivineBeastsAbilityBarPanel.generated.h"

class UDivineBeastsAbilityBarViewModel;
class APawn;
class UTextBlock;
struct FInputActionKeyMapping;

/**
 * UDivineBeastsAbilityBarPanel（神兽联盟技能条面板）。
 *
 * 技能领域适配器把技能图标、充能、冷却和可用状态映射成平台通用 SlotState（槽位状态）。
 * 本类只保存UI投影数组，不直接访问ASC，不创建第二套技能模型。
 * 快捷键文字仅读取拥有玩家的原生映射并写入已有显示控件，映射与输入执行权仍归引擎/输入模块。
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

    /** 用户批准的美术阶段开关；由Monolith设置实际技能栏资产默认值。
     * 开启后按已选择角色投影八张图标，禁止快捷键/点击施放；关闭恢复原真实授权路径。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|UI|Ability|ArtPreview")
    bool bPreviewSelectedHeroIcons = false;

    /** 美术审查显式选择英雄，复用本技能栏唯一VM，不写账号/正式选择或GAS授权。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Ability|ArtPreview")
    bool PreviewHeroIcons(FName HeroDefinitionId);

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
    friend class FDivineBeastsAbilityHotkeyTest;
    friend class FDivineBeastsAbilityDetailsProjectionTest;
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

    void HandleViewModelSlotsChanged(const TArray<FGamePlatformUISlotState>& NewSlots);

    /** 绑定/解绑八个主动槽的中立点击事件；被动作为英雄特质，不占新版八槽。 */
    void BindAbilitySlotCommands(bool bBind);
    /** Widget只提交命令；失败记录中文诊断，真实可用状态继续由ViewModel事件刷新。 */
    void HandleAbilitySlotInvoked(FName SlotId);

    /** 将服务器授权槽按精确GAS输入标签投影到八个可选子Widget；不存在的控件安静跳过。
     * 原五槽WBP仍可消费Primary、Slot1、Slot2、Slot4及被动，无需重建旧二进制资产。
     * 新版八槽被动作为独立Hero Trait，不占可点击槽位。 */
    void RefreshVisualSlots();

    /** 将真实PC映射转换为只读按键文字；保留修饰键及多个绑定，忽略无效/手柄/触摸键。
     * bPassive为true时无条件返回空文字，防止被动槽显示不存在的激活入口；不改变输入映射。 */
    static FText FormatHotkeyMappings(const TArray<FInputActionKeyMapping>& Mappings, bool bPassive);

    /** 游戏线程更新已有HotkeyText，不创建控件。优先读取当前玩家生效映射；
     * 暂无PlayerInput时只读引擎当前InputSettings。缺控件或无映射时安静留空，不制造固定键。 */
    void RefreshSlotHotkey(UGamePlatformSlotWidget* SlotWidget, FName ActionName, bool bPassive);

    /** 现有五控件继续兼容；新增四项BindWidgetOptional，蓝图尚未更新时不伪造Widget。 */
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
    /** 数字5／Slot3：移动。 */
    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> MovementAbilitySlot;

    /** 数字6／Slot5：防御。 */
    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> DefenseAbilitySlot;

    /** 数字7／Slot6：控制。 */
    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> ControlAbilitySlot;

    /** 数字8／Slot7：特色或治疗。 */
    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UGamePlatformSlotWidget> SignatureAbilitySlot;

    /** 该控件唯一拥有的本地 UI ViewModel，与游戏实例和网络身份解耦。 */
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsAbilityBarViewModel> AbilityBarViewModel;
    /** 审查模式必须有可见标识，字号与位置由Monolith资产拥有。 */
    UPROPERTY(Transient, meta=(BindWidgetOptional))
    TObjectPtr<UTextBlock> ArtPreviewLabel;
    FDelegateHandle ViewModelSlotsHandle;
    /** 实际委托来源；控制器跨地图换代后从旧来源解绑，不能从新GetOwningPlayer移除旧订阅。 */
    TWeakObjectPtr<APlayerController> BoundPawnController;

    UPROPERTY(Transient)
    TArray<FGamePlatformUISlotState> AbilitySlots;
    /** 槽事件可同步换Pawn/解绑；每次刷新换代，旧状态事件返回后不得尾写新图标。 */
    FGuid VisualProjectionToken;
};
