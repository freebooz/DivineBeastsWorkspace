#include "Modules/ModuleManager.h"
// 模块只注册反射类型；启动世界逻辑归真实UWorld作用域，不在模块启动发请求。
IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformWorld)
