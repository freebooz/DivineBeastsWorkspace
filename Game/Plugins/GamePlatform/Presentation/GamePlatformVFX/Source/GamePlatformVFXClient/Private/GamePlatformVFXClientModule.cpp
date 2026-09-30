#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

class FGamePlatformVFXClientModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // 将插件 Shaders（着色器源目录）映射到稳定虚拟路径，供 Niagara Custom HLSL / Module Script 使用。
        // Cooked Client（已烘焙客户端）不允许现场编译 Shader 时，RenderCore 会安全忽略注册。
        const FString VirtualShaderDirectory(TEXT("/Plugin/GamePlatformVFX"));
        if (AllShaderSourceDirectoryMappings().Contains(VirtualShaderDirectory))
        {
            return;
        }

        const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GamePlatformVFX"));
        if (!Plugin.IsValid())
        {
            return;
        }

        const FString RealShaderDirectory = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
        AddShaderSourceDirectoryMapping(VirtualShaderDirectory, RealShaderDirectory);
    }

    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGamePlatformVFXClientModule, GamePlatformVFXClient)
