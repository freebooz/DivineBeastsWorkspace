#include "Pooling/GamePlatformVFXPoolingPolicy.h"

ENCPoolMethod FGamePlatformVFXPoolingPolicy::ToNiagaraPoolingMethod(EGamePlatformVFXPoolingMode Mode)
{
    switch (Mode)
    {
    case EGamePlatformVFXPoolingMode::AutoRelease:   return ENCPoolMethod::AutoRelease;
    case EGamePlatformVFXPoolingMode::ManualRelease: return ENCPoolMethod::ManualRelease;
    case EGamePlatformVFXPoolingMode::None:
    default: return ENCPoolMethod::None;
    }
}
