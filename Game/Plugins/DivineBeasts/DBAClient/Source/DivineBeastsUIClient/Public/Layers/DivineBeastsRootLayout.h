#pragma once

#include "Layers/GamePlatformRootLayout.h"

class APawn;
class UDivineBeastsCombatPanelBase;
class UAbilitySystemComponent;
class UGamePlatformStatusEffectTrayWidget;
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

protected:
    /** 战斗主HUD只应在拥有本地有效战斗Pawn时出现；登录、选角及切换世界时自动隐藏。 */
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /** 只绑定项目层WBP中的组合入口，继续沿用平台RootLayout的HUDLayer/ScreenLayer。 */
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UDivineBeastsCombatPanelBase> CombatHUD = nullptr;

private:
    friend class FDivineBeastsHUDTravelOwnershipTest;
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
