#include "GamePlatformCore.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGamePlatformCore);

// 仅注册模块与日志，无进程级用户状态、世界启动、后端连接或服务单例。
IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformCore)
