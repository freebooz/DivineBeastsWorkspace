#include "Modules/ModuleManager.h"

// 启动行为由Dedicated Server专属GameInstanceSubsystem按显式Profile执行；模块入口不联网或载图。
IMPLEMENT_MODULE(FDefaultModuleImpl, DBAServer)
