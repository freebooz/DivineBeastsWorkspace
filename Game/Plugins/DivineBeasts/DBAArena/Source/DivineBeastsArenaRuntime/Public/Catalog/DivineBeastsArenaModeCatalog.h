#pragma once

#include "CoreMinimal.h"
#include "Config/DivineBeastsArenaProjectConfig.h"

/** FDivineBeastsArenaModeCatalog（神兽联盟五模式项目目录）。 */
class DIVINEBEASTSARENARUNTIME_API FDivineBeastsArenaModeCatalog
{
public:
    static const TArray<FDivineBeastsArenaProjectModeSpec>& GetAll();
    static const FDivineBeastsArenaProjectModeSpec* Find(FName ArenaModeId);

    static bool ValidateStructuralCatalog(FString& OutError);
    /** 双开关开发准入：CustomConfig与命令行同时显式启用，Shipping/Test始终拒绝。 */
    static bool IsDevelopmentEnabledForCurrentProcess();
    static bool ValidateAssignmentForRuntime(const struct FGamePlatformArenaAssignment& Assignment,FGamePlatformArenaModeSpec& OutModeSpec,FString& OutError,bool bAllowDevelopment);
    static bool ValidateProductionCatalog(FString& OutError);

    static bool ValidateAssignmentAgainstProduction(
        const struct FGamePlatformArenaAssignment& Assignment,
        FGamePlatformArenaModeSpec& OutModeSpec,
        FString& OutError);
};
