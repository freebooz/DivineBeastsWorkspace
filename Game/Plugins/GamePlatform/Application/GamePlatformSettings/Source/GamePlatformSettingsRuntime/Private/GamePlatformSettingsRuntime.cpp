// 平台共享设置模块：唯一拥有统一日志类别的定义；生命周期仅注册默认模块，无用户/世界状态与启动IO。
#include "Modules/ModuleManager.h"

#include "GamePlatformSettingsLog.h"

DEFINE_LOG_CATEGORY(LogGamePlatformSettings);

// Runtime 模块只提供契约/子系统，不在模块启动阶段读取用户配置或访问世界。
IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformSettingsRuntime)
