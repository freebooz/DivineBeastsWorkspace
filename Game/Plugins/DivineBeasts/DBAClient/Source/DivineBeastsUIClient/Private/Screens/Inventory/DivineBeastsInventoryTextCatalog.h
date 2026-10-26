#pragma once

#include "CoreMinimal.h"
#include "Types/GamePlatformInventoryTypes.h"

/** FDivineBeastsInventoryTextCatalog（背包本地化信息目录）。
 * 平台只拥有机器状态/稳定错误码；此项目目录统一中文表现。
 * 无Widget生命周期、账户/背包状态或网络写权限，不暴露后端原始自然语言。 */
struct FDivineBeastsInventoryTextCatalog final
{
    static FText ForState(EGamePlatformInventoryClientState State);
    static FText ForError(EGamePlatformInventoryError Error);
};
