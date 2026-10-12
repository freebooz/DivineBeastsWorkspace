// GamePlatformSurface客户端默认配置。
#include "Settings/GamePlatformSurfaceSettings.h"

UGamePlatformSurfaceSettings::UGamePlatformSurfaceSettings()
{
    GlobalParameterCollection = TSoftObjectPtr<UMaterialParameterCollection>(
        FSoftObjectPath(TEXT("/GamePlatformSurface/ParameterCollections/MPC_GP_SurfaceGlobal.MPC_GP_SurfaceGlobal")));    MarkCatalog = TSoftObjectPtr<UGamePlatformSurfaceMarkCatalog>(
        FSoftObjectPath(TEXT("/GamePlatformSurface/Marks/DA_GP_SurfaceMarkCatalog.DA_GP_SurfaceMarkCatalog")));
}
