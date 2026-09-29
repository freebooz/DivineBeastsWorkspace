#include "Modules/ModuleManager.h"

#include "GamePlatformSettingsLog.h"

DEFINE_LOG_CATEGORY(LogGamePlatformSettings);

// Runtime 模块只提供契约/子系统，不在模块启动阶段读取用户配置或访问世界。
IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformSettingsRuntime)
