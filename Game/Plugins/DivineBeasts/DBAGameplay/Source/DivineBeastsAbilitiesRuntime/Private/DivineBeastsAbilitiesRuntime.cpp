#include "Modules/ModuleManager.h"

/** 神兽联盟技能运行模块只注册反射类型；不在模块启动时创建世界、授权玩家或加载英雄。 */
IMPLEMENT_MODULE(FDefaultModuleImpl, DivineBeastsAbilitiesRuntime)
