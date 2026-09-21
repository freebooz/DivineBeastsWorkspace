#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"
#include "Types/GamePlatformVFXParameters.h"
#include "Types/GamePlatformVFXPreloadHandle.h"
#include "Types/GamePlatformVFXRegistrationHandle.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Types/GamePlatformVFXResult.h"
#include "Types/GamePlatformVFXTypes.h"

class UObject;
class UGamePlatformVFXCatalog;

/**
 * VFX 低层服务契约。
 * Gameplay 不应直接依赖本接口；推荐由 GamePlatformPresentation 或项目表现适配层调用。
 */
class GAMEPLATFORMVFXCLIENT_API IGamePlatformVFXService
{
public:
    virtual ~IGamePlatformVFXService() = default;

    static IGamePlatformVFXService* Get(const UObject* WorldContextObject);

    virtual FGamePlatformVFXPlayResult Play(const FGamePlatformVFXRequest& Request) = 0;
    virtual bool Stop(const FGamePlatformVFXHandle& Handle, bool bImmediate = false) = 0;
    virtual bool UpdateParameters(const FGamePlatformVFXHandle& Handle, const FGamePlatformVFXParameters& Parameters) = 0;
    virtual EGamePlatformVFXLifecycleState GetState(const FGamePlatformVFXHandle& Handle) const = 0;

    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) = 0;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) = 0;

    virtual FGamePlatformVFXPreloadHandle Preload(const FPrimaryAssetId& DefinitionId) = 0;
    virtual bool ReleasePreload(const FGamePlatformVFXPreloadHandle& Handle) = 0;
};
