// GamePlatformSurfaceClient（游戏平台环境表面客户端模块）入口。
// 模块启动不主动加载任何材质或纹理；具体世界创建后由世界子系统按配置绑定材质参数集合。
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformSurfaceClient)
