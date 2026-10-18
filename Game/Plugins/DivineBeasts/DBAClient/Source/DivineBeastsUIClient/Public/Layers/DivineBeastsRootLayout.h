// 项目LocalPlayer根布局：拥有已保存HUD组合的只读事件接线，Pawn/控制器换代时成对撤销；不持有战斗权威。
#pragma once

#include "Layers/GamePlatformRootLayout.h"

class APawn;
class UDivineBeastsCombatPanelBase;
class UAbilitySystemComponent;
class UDivineBeastsUIClientSubsystem;
class UDivineBeastsUIViewModel;
class UDivineBeastsWorldHUDBase;
class UDivineBeastsWorldHealthBarPresenter;
class UGamePlatformWorldWidgetBase;
class UMaterialInterface;
struct FDivineBeastsUIViewState;
class UGamePlatformStatusEffectTrayWidget;
class UGamePlatformAbilityCastComponent;
class UGamePlatformCastProgressWidget;
class UGamePlatformCombatFeedbackWorldSubsystem;
class UGamePlatformFeedbackWidget;
class UDivineBeastsCharacterComponent;
struct FGamePlatformCombatEvent;
struct FGamePlatformAbilityCastFacts;
struct FGameplayTag;
struct FOnAttributeChangeData;
#include "DivineBeastsRootLayout.generated.h"

/**
 * UDivineBeastsRootLayout（神兽联盟根布局基类）。
 *
 * 每个 LocalPlayer 只安装一个项目 RootLayout（根布局）。
 * Blueprint 可基于该类组织 HUD、Screen、Modal、Notification、Loading 等平台层，
 * 并通过平台 Adaptive Context（自适应上下文）响应 PC / Mobile 布局变化。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsRootLayout
    : public UGamePlatformRootLayout
{
    GENERATED_BODY()

public:
    /** 本地玩家控制器换代时恢复作者HUD并更新拥有者上下文；仅游戏线程UI事件调用。
     * 不恢复上一世界动态HUD、不授予技能；空控制器撤销订阅并隐藏当前展示。 */
    void RefreshForPlayerController(APlayerController* Controller);
    /** 本LocalPlayer世界血条显示开关；UI命令或原生V键通知调用，无网络副作用。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|World")
    void ToggleWorldHealthBars();

protected:
    /** 战斗主HUD只应在拥有本地有效战斗Pawn时出现；登录、选角及切换世界时自动隐藏。 */
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** 只绑定项目层WBP中的组合入口，继续沿用平台RootLayout的HUDLayer/ScreenLayer。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UDivineBeastsCombatPanelBase> CombatHUD = nullptr;
    /** 公共UI包真实名称板类；仅客户端组合根引用，专服不加载UI资产。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|UI|World")
    TSubclassOf<UGamePlatformWorldWidgetBase> WorldHealthBarClass;
    /** 公共UI包作者制作的脚下选中贴花；SelectionColor参数由事件提供，不参与服务器或Gameplay。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|UI|World")
    TObjectPtr<UMaterialInterface> SelectionRingMaterial;
    /** 公共界面包的真实浮字控件；仅消费当前世界已确认的伤害事件。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|UI|Combat")
    TSubclassOf<UGamePlatformFeedbackWidget> CombatFloatingTextClass;

private:
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsWorldHealthBarPresenter> WorldHealthBars;
    TWeakObjectPtr<APlayerController> WorldHealthController;
    bool bWorldHealthBarsVisible = true;
    void BindWorldHealthBars(APawn* DisplayPawn);
    void HandleFinalCameraViewChanged();
    void RefreshWorldHealthBarToggleText();
    /** 新手村与开放世界共用的平台反馈通知；竞技场由竞技客户端装配，避免双份飘字。 */
    void BindWorldCombatFeedback(APawn* DisplayPawn);
    void UnbindWorldCombatFeedback();
    void HandleConfirmedWorldCombatFeedback(const FGamePlatformCombatEvent& Event);
    TWeakObjectPtr<UGamePlatformCombatFeedbackWorldSubsystem> BoundCombatFeedbackBus;
    FDelegateHandle CombatFeedbackHandle;
    /** 公共世界四种HUD的实际选择与单一模型；根布局拥有页面代次，子HUD仅订阅显示。 */
    TWeakObjectPtr<UDivineBeastsUIClientSubsystem> WorldUIOwner;
    TWeakObjectPtr<UDivineBeastsWorldHUDBase> ActiveWorldContextHUD;
    UPROPERTY(Transient) TObjectPtr<UDivineBeastsUIViewModel> WorldHUDModel;
    FDelegateHandle WorldUIStateHandle;
    bool bSynchronizingWorldHUD = false;
    /** 取消旧命令可同步产生后继快照；合并为下一轮重算，不能丢弃注销/换世界事件。 */
    bool bWorldHUDRefreshQueued = false;
    bool bWorldHUDHostConstructed = false;
    void ObserveWorldUIOwner();
    void HandleWorldUIState(const FDivineBeastsUIViewState& State);
    void RefreshWorldContextHUD();
    void RefreshWorldContextHUDOnce();
    void ReleaseWorldContextHUD();
    friend class FDivineBeastsHUDTravelOwnershipTest;
    friend class FDivineBeastsCastBarEventBindingTest;
    /** 当前本地Pawn的确认施法动作只读投影；窗口结束不代表本UI完成/释放技能。 */
    TWeakObjectPtr<APawn> BoundCastPawn;
    TWeakObjectPtr<UGamePlatformAbilityCastComponent> BoundCastComponent;
    TWeakObjectPtr<UGamePlatformCastProgressWidget> BoundCastWidget;
    TWeakObjectPtr<UDivineBeastsCharacterComponent> BoundCastIdentity;
    FDelegateHandle CastFactsHandle;
    FDelegateHandle CastIdentityHandle;
    /** 本地绑定作用域每次换Pawn重新生成，UI修订只在同一作用域内递增。 */
    FGuid CastDisplayScopeId;
    int64 CastDisplayRevision = 0;
    /** 只在拥有者生命周期事件中重绑并读取首次快照；重复同来源绑定不重复订阅。 */
    void BindCastDisplay(APawn* Pawn);
    /** 先失效本地作用域、成对解绑，再清空旧Widget；迟到通知不能恢复旧角色。 */
    void UnbindCastDisplay();
    /** 仅接受当前组件/当前复制值的通知，拒绝同步重入发布后已过期的外层广播。 */
    void HandleCastFactsChanged(UGamePlatformAbilityCastComponent* Component, const FGamePlatformAbilityCastFacts& Facts);
    /** 项目身份复制可晚于施法组件；身份通知重投影同代快照，不使用等待Timer。 */
    void RefreshCastDisplayFromIdentity();
    /** 当前Pawn退出即撤销显示；不等待下一次Possess事件或复制。 */
    UFUNCTION()
    void HandleCastPawnEndPlay(AActor* Actor, EEndPlayReason::Type Reason);
    /** 根布局跨地图存活，委托必须从实际绑定的旧控制器移除，不能用已更新的GetOwningPlayer查旧来源。 */
    TWeakObjectPtr<APlayerController> BoundHUDController;
    UFUNCTION()
    void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

    /** 仅在本地拥有者战斗ASC事件变化时投影公开状态，绝不复制或更改Gameplay。 */
    void BindCombatEffects(APawn* Pawn);
    void UnbindCombatEffects();
    void RefreshCombatEffects();
    void RenderCombatEffects();

    void HandleCombatAttributeChanged(const FOnAttributeChangeData& ChangeData);
    void HandleCombatTagChanged(const FGameplayTag Tag, int32 NewCount);

    TWeakObjectPtr<UAbilitySystemComponent> BoundEffectsASC;
    TWeakObjectPtr<UGamePlatformStatusEffectTrayWidget> BoundEffectsWidget;

    FGuid EffectDisplayScopeId;
    int64 EffectDisplayRevision = 0;
    FString LastEffectDisplaySignature;

    FDelegateHandle DamageBonusHandle;
    FDelegateHandle DamageReductionHandle;
    FDelegateHandle ShieldedTagHandle;
    FDelegateHandle StunTagHandle;
    FDelegateHandle SilenceTagHandle;

    /** 事件驱动更新可见性，不扫描世界/不启用Tick，不改变真实Gameplay状态。 */
    void RefreshCombatHUDVisibility();
};
