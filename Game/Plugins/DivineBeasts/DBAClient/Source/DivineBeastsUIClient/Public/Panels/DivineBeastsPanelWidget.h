#pragma once

#include "Panels/GamePlatformPanelWidget.h"
#include "Contracts/DivineBeastsUIDomainTypes.h"
#include "DivineBeastsPanelWidget.generated.h"

/**
 * UDivineBeastsPanelWidget（神兽联盟功能面板基类）。
 *
 * 用于玩家状态、技能、背包、装备、任务、队伍等复合界面内部面板。
 * 项目面板继续复用平台事件和自适应能力，不自行维护页面导航栈。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSUICLIENT_API UDivineBeastsPanelWidget
    : public UGamePlatformPanelWidget
{
    GENERATED_BODY()

public:
    /** 项目面板默认语义Style绑定；仅在有同名Widget和有效主题时覆盖本实例视觉。 */
    UDivineBeastsPanelWidget();

    /** 表现层领域标识，不作为服务器授权判断。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Domain")
    EDivineBeastsUIDomain GetBusinessDomain() const { return UIDomain; }

protected:
    UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|UI|Domain")
    EDivineBeastsUIDomain UIDomain = EDivineBeastsUIDomain::Core;
};
