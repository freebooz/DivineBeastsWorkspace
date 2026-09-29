// GamePlatformSFXClient（游戏平台音效客户端模块）入口。
// 模块启动时不加载具体声音资源；播放资源均由运行期Definition租约按需取得。
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GamePlatformSFXClient)