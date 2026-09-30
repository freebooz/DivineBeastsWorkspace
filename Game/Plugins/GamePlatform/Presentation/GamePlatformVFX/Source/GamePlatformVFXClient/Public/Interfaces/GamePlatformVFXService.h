#pragma once

#include "Types/GamePlatformVFXPreloadHandle.h"
#include "Types/GamePlatformVFXRegistrationHandle.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Types/GamePlatformVFXResult.h"

class UGamePlatformVFXCatalog;
class UWorld;

/**
 * GamePlatformVFX 对外稳定服务接口。调用方不直接访问 WorldSubsystem。
 * 所有接口仅允许在 Game Thread（游戏线程）调用；异步资源完成也回到游戏线程处理。
 */
class GAMEPLATFORMVFXCLIENT_API IGamePlatformVFXService
{
public:
    virtual ~IGamePlatformVFXService() = default;

    static IGamePlatformVFXService* Get(UWorld* World);

    virtual FGamePlatformVFXResult Play(const FGamePlatformVFXRequest& Request) = 0;
    virtual bool Stop(const FGamePlatformVFXHandle& Handle) = 0;
    virtual bool IsActive(const FGamePlatformVFXHandle& Handle) const = 0;

    virtual FGamePlatformVFXPreloadHandle Preload(const FGamePlatformVFXRequest& Request) = 0;
    virtual bool CancelPreload(const FGamePlatformVFXPreloadHandle& Handle) = 0;

    /**
     * 旧低层工具兼容入口。标准Gameplay必须由GamePlatformPresentation解析DefinitionId，
     * 新业务不得继续建立第二套VFX语义Catalog。
     */
    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) = 0;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) = 0;
};
