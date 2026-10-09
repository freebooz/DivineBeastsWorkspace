#pragma once

#include "Logging/LogMacros.h"

/** Runtime拥有的统一日志类别，Client/Server/Editor借用；模块全局不保存用户或世界状态。
 * 跨DLL声明必须使用Runtime导出宏，定义仅在Runtime模块一处；敏感设置值不得直接写入日志。 */
GAMEPLATFORMSETTINGSRUNTIME_API DECLARE_LOG_CATEGORY_EXTERN(LogGamePlatformSettings, Log, All);
