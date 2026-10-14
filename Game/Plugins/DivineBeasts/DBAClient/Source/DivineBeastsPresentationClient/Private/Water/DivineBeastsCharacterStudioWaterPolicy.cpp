// 前端预览水效可选路径的失败关闭实现：Server不会编译此ClientOnly模块，
// 游戏主世界不加载FluidFlux，不把对插件名称的字符串查找当实际表现已验证。
#include "Water/DivineBeastsCharacterStudioWaterPolicy.h"

#include "Interfaces/IPluginManager.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformProperties.h"

bool UDivineBeastsCharacterStudioWaterPolicy::IsFluidFluxPluginEligible()
{
#if PLATFORM_DESKTOP
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FluidFlux"));
    return Plugin.IsValid() && Plugin->IsEnabled();
#else
    return false; // 移动端和其它非桌面平台不能启用未经验证的可选模拟。
#endif
}

EDivineBeastsCharacterStudioWaterMode
UDivineBeastsCharacterStudioWaterPolicy::ResolveWaterMode(
    const AActor* PreviewStage, bool bPreferFluidFlux, bool bHighQuality)
{
    if (!bPreferFluidFlux || !bHighQuality ||
        !IsFluidFluxPluginEligible() || !IsValid(PreviewStage))
        return EDivineBeastsCharacterStudioWaterMode::Native;

    const UWorld* World = PreviewStage->GetWorld();
    const ULevel* Level = PreviewStage->GetLevel();
    // 前端工作室是唯一可试验区域，旧世界/PIE重入必须全部回退原生水效。
    if (!IsValid(World) || !IsValid(Level) || !World->IsGameWorld() ||
        World->GetNetMode() != NM_Standalone || World->bIsTearingDown)
        return EDivineBeastsCharacterStudioWaterMode::Native;

    const FString LevelPackage = Level->GetOutermost()->GetName();
    if (!LevelPackage.Contains(TEXT("L_DBA_CharacterStudio"), ESearchCase::CaseSensitive) ||
        !LevelPackage.Contains(TEXT("DBAFrontEndPack"), ESearchCase::CaseSensitive))
        return EDivineBeastsCharacterStudioWaterMode::Native;

    return EDivineBeastsCharacterStudioWaterMode::FluidFluxEligible;
}
