// GamePlatformSaveClient（游戏平台本地存档客户端模块）入口。
// 模块启动阶段不读写用户存档；真实生命周期由GameInstance作用域子系统管理。
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformSaveClient)