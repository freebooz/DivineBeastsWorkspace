#pragma once

#include "Components/GamePlatformResourceBarWidget.h"
#include "Panels/DivineBeastsPanelWidget.h"
#include "DivineBeastsPlayerStatusPanel.generated.h"

/**
 * FDivineBeastsPlayerStatusViewData（神兽联盟玩家状态视图数据）。
 *
 * 这是UI只读投影，不是Gameplay属性真源。
 * Health/Shield来自平台战斗或GAS确认状态；Momentum（气势）来自项目正式玩法状态源。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsPlayerStatusViewData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Health = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double MaxHealth = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Shield = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double MaxShield = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double Momentum = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    double MaxMomentum = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DivineBeasts|UI|Combat")
    bool bDead = false;
};

/**
 * UDivineBeastsPlayerStatusPanel（神兽联盟玩家状态面板）。
 *
 * 组合平台 ResourceBar（资源条）视觉原子，统一展示生命、护盾和气势。
 * 页面/HUD的领域ViewModel在事实变化时调用 ApplyStatus；Panel自身不轮询Gameplay对象。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPlayerStatusPanel
    : public UDivineBeastsPanelWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|UI|Combat")
    void ApplyStatus(const FDivineBeastsPlayerStatusViewData& InStatus);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Combat")
    FDivineBeastsPlayerStatusViewData GetStatus() const { return Status; }

protected:
    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> HealthBar = nullptr;

    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> ShieldBar = nullptr;

    UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="DivineBeasts|UI|Combat")
    TObjectPtr<UGamePlatformResourceBarWidget> MomentumBar = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Combat", meta=(DisplayName="玩家状态已变化"))
    void BP_OnPlayerStatusChanged(FDivineBeastsPlayerStatusViewData NewStatus);

private:
    UPROPERTY(Transient)
    FDivineBeastsPlayerStatusViewData Status;
};
