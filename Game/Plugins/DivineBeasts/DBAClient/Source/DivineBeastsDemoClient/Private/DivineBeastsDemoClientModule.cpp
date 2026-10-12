// 仅开发Client/Editor可装载；模块启动不登录、不切地图、不配置生产后端。
// 插件及地图发现均由GameInstance演示子系统在允许的本地作用域完成。
#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, DivineBeastsDemoClient)
