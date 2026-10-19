#pragma once

#include "WorldUI/GamePlatformWorldWidgetBase.h"
#include "Styling/SlateColor.h"
#include "GamePlatformWorldNameplateWidget.generated.h"

/** UGamePlatformWorldNameplateWidget（游戏平台世界名称板基类）：玩家、NPC、敌方名称与世界血条的共用表面。 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformWorldNameplateWidget
    : public UGamePlatformWorldWidgetBase
{
    GENERATED_BODY()
public:
    /** 事件快照投影为已有ResourceFill/NameText/ValueText及可选选中边框/标记；不读取Actor。 */
    virtual void ApplyWorldUIRequest(const FGamePlatformWorldUIRequest& InRequest) override;
private:
    /** 首次应用前保存作者默认颜色，目标切换时恢复；不全局更改共享样式资产。 */
    bool bDefaultColorsCaptured = false;
    uint64 VisualApplyGeneration = 0; // 蓝图同步重入同RequestId时，旧快照不得覆盖后继选中状态。
    FLinearColor DefaultResourceColor = FLinearColor::White;
    FSlateColor DefaultNameColor = FSlateColor(FLinearColor::White);
};
