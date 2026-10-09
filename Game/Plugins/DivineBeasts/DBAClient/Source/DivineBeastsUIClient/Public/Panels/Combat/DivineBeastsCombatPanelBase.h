#pragma once

#include "Panels/DivineBeastsPanelWidget.h"
#include "DivineBeastsCombatPanelBase.generated.h"

/** 战斗面板基础类：只在客户端事实变化时发出局部重绘事件，不执行业务Tick。 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsCombatPanelBase : public UDivineBeastsPanelWidget
{
    GENERATED_BODY()
public:
    UDivineBeastsCombatPanelBase() { UIDomain = EDivineBeastsUIDomain::Combat; }
    /** 本地视觉修订号，不作为网络权威状态的版本。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Combat")
    int32 GetPresentationRevision() const { return PresentationRevision; }
protected:
    /** 子面板完成实际数据更新后调用；相同快照由子类负责去重。 */
    void NotifyCombatPresentationChanged();
    UFUNCTION(BlueprintImplementableEvent, Category="DivineBeasts|UI|Combat",
        meta=(DisplayName="战斗面板表现已变化"))
    void BP_OnCombatPresentationChanged(int32 NewRevision);
private:
    UPROPERTY(Transient)
    int32 PresentationRevision = 0;
};
