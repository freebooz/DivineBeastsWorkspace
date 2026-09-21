#include "Modules/ModuleManager.h"

// 模块加载不启动玩家流程；开发装配由具体 GameInstance 的生命周期管理。
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, DivineBeastsArena, "DivineBeastsArena");
