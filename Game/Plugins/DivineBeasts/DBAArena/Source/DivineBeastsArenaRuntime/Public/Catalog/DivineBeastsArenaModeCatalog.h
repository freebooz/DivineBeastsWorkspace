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
    static bool ValidateProductionCatalog(FString& OutError);

    static bool ValidateAssignmentAgainstProduction(
        const struct FGamePlatformArenaAssignment& Assignment,
        FGamePlatformArenaModeSpec& OutModeSpec,
        FString& OutError);
};
