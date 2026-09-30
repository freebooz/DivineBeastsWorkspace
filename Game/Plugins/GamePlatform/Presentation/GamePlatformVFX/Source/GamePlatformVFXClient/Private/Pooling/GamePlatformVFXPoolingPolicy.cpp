#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Settings/GamePlatformVFXSettings.h"

bool FGamePlatformVFXPoolingPolicy::ShouldUseNiagaraPool(
    const UGamePlatformVFXDefinition& Definition,
    const FGamePlatformVFXRequest& Request,
    const UGamePlatformVFXSettings& Settings)
{
    // Importance决定裁剪优先级，不决定组件能否复用；关键效果同样应避免高峰期频繁创建/销毁组件。
    static_cast<void>(Request);
    return Settings.bEnablePooling && Definition.AllowsPooling();
}
