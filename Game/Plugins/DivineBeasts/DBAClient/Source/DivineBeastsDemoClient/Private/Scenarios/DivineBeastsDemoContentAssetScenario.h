// SSLM内容包人工预览；开发客户端私有适配，正式资源所有权仍由GamePlatformData管理。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"

namespace DivineBeasts::Demo
{
/** 纯选择合同；只允许当前内容包顶层对象，不接受子对象、别名包、外部路径或脚本文本。 */
FGamePlatformResult ValidateContentAssetSelection(FName OwnerPlugin, const FSoftObjectPath& Asset);
/** 每World/运行单独构造；自身对象、Data租约和Editor窗口成对撤销。 */
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateContentAssetScenario();
}
