// 运行环境由世界子系统维护；编辑器仅注册事件预览，不连接后端或生成正式内容。
#include "Modules/ModuleManager.h"
#if WITH_EDITOR
#include "Editor/GamePlatformWeatherWindPreview.h"
#endif
class FGamePlatformWeatherClientModule : public IModuleInterface
{
#if WITH_EDITOR
    FGamePlatformWeatherWindPreview Preview;
    virtual void StartupModule() override { Preview.Start(); }
    virtual void ShutdownModule() override { Preview.Stop(); }
#endif
};
IMPLEMENT_MODULE(FGamePlatformWeatherClientModule, GamePlatformWeatherClient)
