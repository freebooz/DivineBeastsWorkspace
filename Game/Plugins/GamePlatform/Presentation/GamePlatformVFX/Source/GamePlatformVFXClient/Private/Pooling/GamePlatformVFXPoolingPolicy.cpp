#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Settings/GamePlatformVFXSettings.h"

bool FGamePlatformVFXPoolingPolicy::ShouldUseNiagaraPool(
    const UGamePlatformVFXDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    const UGamePlatformVFXSettings& Settings)
{
    return Settings.bEnablePooling &&
           Definition.AllowsPooling() &&
           Request.Importance != EGamePlatformVFXImportance::Critical;
}
