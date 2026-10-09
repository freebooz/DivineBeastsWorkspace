#pragma once

#include "CoreMinimal.h"
#include "DivineBeastsUIDomainTypes.generated.h"

/** EDivineBeastsUIDomain（十大用户界面业务域）。
 * 分类只负责界面组织和本地展示，不用于服务器授权；竞技实现归可选DBAArena插件。
 */
UENUM(BlueprintType)
enum class EDivineBeastsUIDomain : uint8
{
    Core UMETA(DisplayName="核心框架"),
    Account UMETA(DisplayName="账号与登录"),
    World UMETA(DisplayName="世界与任务"),
    Character UMETA(DisplayName="角色与成长"),
    Inventory UMETA(DisplayName="背包与装备"),
    Combat UMETA(DisplayName="战斗与状态"),
    Social UMETA(DisplayName="社交与组队"),
    Arena UMETA(DisplayName="竞技对局"),
    System UMETA(DisplayName="系统设置"),
    LiveOps UMETA(DisplayName="运营与服务")
};
