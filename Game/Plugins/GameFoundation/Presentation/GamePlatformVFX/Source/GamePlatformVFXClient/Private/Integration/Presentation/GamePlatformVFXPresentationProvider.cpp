#include "Integration/Presentation/GamePlatformVFXPresentationProvider.h"
#include "Interfaces/GamePlatformVFXService.h"

FGamePlatformVFXPlayResult FGamePlatformVFXPresentationProvider::Submit(const UObject* WorldContextObject, const FGamePlatformVFXRequest& Request) const
{
    if (IGamePlatformVFXService* Service = IGamePlatformVFXService::Get(WorldContextObject))
    {
        return Service->Play(Request);
    }

    FGamePlatformVFXPlayResult Result;
    Result.Code = EGamePlatformVFXPlayResultCode::WorldUnavailable;
    Result.Message = TEXT("GamePlatformVFX 服务不可用。");
    return Result;
}
