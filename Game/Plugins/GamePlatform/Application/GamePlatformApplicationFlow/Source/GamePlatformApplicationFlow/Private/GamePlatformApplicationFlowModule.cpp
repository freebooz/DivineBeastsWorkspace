#include "Modules/ModuleManager.h"

// 模块仅注册类型。真实流程由 GameInstance 的组合根显式配置、启动。
IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformApplicationFlow)
