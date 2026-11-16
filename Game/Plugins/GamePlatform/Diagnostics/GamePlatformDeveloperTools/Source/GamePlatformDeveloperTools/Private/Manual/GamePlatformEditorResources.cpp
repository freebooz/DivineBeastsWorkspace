// 平台Editor中立全局资源桥：由既有DeveloperTools模块签发精确排他租约。
// 仅游戏线程；调用者负责资格，完整句柄负责释放，不包含上层feature/vendor白名单。
#include "Manual/GamePlatformEditorResources.h"
#include "Modules/ModuleManager.h"
IGamePlatformEditorResources* IGamePlatformEditorResources::Get(){return FModuleManager::GetModulePtr<IGamePlatformEditorResources>(TEXT("GamePlatformDeveloperTools"));}
