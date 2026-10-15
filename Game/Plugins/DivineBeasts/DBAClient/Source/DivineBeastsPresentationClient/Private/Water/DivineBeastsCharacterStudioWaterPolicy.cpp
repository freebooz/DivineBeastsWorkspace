// 前端水体可选供应商资格门禁：未经UE5.8真实审核的Fluid Flux即使安装也默认禁止。
// 本模块仅作表现选型，不创建Fluid Flux对象，不改变正式世界或Dedicated Server规则。
#include "Water/DivineBeastsCharacterStudioWaterPolicy.h"

#include "Interfaces/IPluginManager.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "UObject/Package.h" // GetOutermost返回UPackage，UE5.8不允许仅依赖前置声明调用GetName。
#include "GameFramework/Actor.h"
#include "HAL/PlatformProperties.h"
#include "Misc/ConfigCacheIni.h"

bool UDivineBeastsCharacterStudioWaterPolicy::IsFluidFluxPluginEligible()
{
#if PLATFORM_DESKTOP
    // 只有对版本、客户端Cook、Shader、性能与降级全部完成实际验收后，才允许显式打开门禁。
    // 键缺失、命令行/专服进程、配置系统尚未初始化时，均坚持原生方案。
    if (!IsInGameThread() || IsRunningCommandlet() || IsRunningDedicatedServer() || !GConfig)
        return false;
    bool bVerified = false;
    const bool bHasApproval = GConfig->GetBool(
        TEXT("DivineBeasts.CharacterStudio.Water"),
        TEXT("bFluidFlux3PreviewVerified"), bVerified, GGameIni);
    if (!bHasApproval || !bVerified)
        return false;

    // 插件存在及启用仅说明具备候选资格，不能视为已经创建流体模拟器或成功加载其资产。
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FluidFlux"));
    return Plugin.IsValid() && Plugin->IsEnabled();
#else
    return false; // 非桌面设备不启用未经验证的高成本流体模拟。
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
