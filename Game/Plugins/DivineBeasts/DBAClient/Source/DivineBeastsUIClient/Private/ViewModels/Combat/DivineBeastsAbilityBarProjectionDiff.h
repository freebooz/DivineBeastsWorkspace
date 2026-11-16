#pragma once

#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"

/**
 * FDivineBeastsAbilityBarProjectionDiff（技能栏只读快照差异比较）。
 * 比较公开显示合同中的字段，而不订阅GAS（游戏技能系统）事件或更改任何技能授权。
 * 差异判定单独复用/测试，避免在视图模型内维护大型嵌套通知判断。
 */
struct FDivineBeastsAbilityBarProjectionDiff final
{
    /** 技能槽状态的变化，冷却遮罩采用0.0001的现有误差阈值。 */
    static bool HasSlotsChanged(
        const TArray<FGamePlatformUISlotState>& Previous,
        const TArray<FGamePlatformUISlotState>& Current);

    /** 技能文字、封禁原因和材质倒计时，时间比较保持0.01秒现有阈值。 */
    static bool HasDetailsChanged(
        const TArray<FDivineBeastsAbilitySlotDetails>& Previous,
        const TArray<FDivineBeastsAbilitySlotDetails>& Current);
};
