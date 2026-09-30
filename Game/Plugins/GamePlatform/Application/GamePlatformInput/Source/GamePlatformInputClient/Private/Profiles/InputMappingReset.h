#pragma once
// 当前输入Profile映射重置：只允许其已登记行，服务与UE回归共用生产入口。
// 仅游戏线程内存操作；不保存磁盘，不持有LocalPlayer或其他功能的映射所有权。
#include "CoreMinimal.h"
#include "Types/GamePlatformResult.h"
class UEnhancedInputUserSettings;
/** None重置ProfileRows与原生已登记行的交集；指定外部行拒绝；失败保留已应用键位并明确返回。 */
FGamePlatformResult ResetGamePlatformProfileMappings(UEnhancedInputUserSettings& Settings, const TSet<FName>& ProfileRows, FName RowName);
