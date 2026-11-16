#pragma once

#include "CoreMinimal.h"

/** FDivineBeastsWorldHUDRoute（神兽联盟世界HUD路由策略）。
 * 只将经确认的世界角色/体验身份映射到已有作者WidgetSwitcher槽位。
 * 不决定服务器准入、不创建UI对象、不修改领域状态或启动Tick。 */
struct FDivineBeastsWorldHUDRoute final
{
    /** 只有常驻世界与新手村使用常规世界HUD；竞技使用独立HUD。 */
    static bool IsSupportedWorldRole(FName ServerRole);

    /** 现有作者资产固定顺序：0常驻世界、1新手村、2教学、3训练；未知返回INDEX_NONE。 */
    static int32 ResolveSwitcherIndex(FName ServerRole, FName ExperienceId);
};
