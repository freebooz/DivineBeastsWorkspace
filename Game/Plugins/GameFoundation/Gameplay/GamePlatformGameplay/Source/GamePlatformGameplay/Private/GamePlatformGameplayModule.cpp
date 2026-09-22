#include "GamePlatformGameplay.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGamePlatformGameplay);

/** 模块只注册类型与日志，不在StartupModule中选择体验、访问世界或启动玩家流程。 */
class FGamePlatformGameplayModule final : public IModuleInterface
{
};

IMPLEMENT_MODULE(FGamePlatformGameplayModule, GamePlatformGameplay)
