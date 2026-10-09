// 本文件属于GamePlatform平台层 GamePlatformVFX，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
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
    /** 仅本世界完整实例句柄可查；历史有界，淘汰后返回Invalid。 */
    virtual FGamePlatformVFXPlaybackSnapshot GetPlaybackSnapshot(const FGamePlatformVFXHandle& Handle) const = 0;
    /** 资源/实例清理后游戏线程通知一次；订阅者不得保存服务裸指针越过世界生命周期。 */
    virtual FDelegateHandle AddCompletionHandler(const FGamePlatformVFXPlaybackCompleted::FDelegate& Handler) = 0;
    virtual void RemoveCompletionHandler(FDelegateHandle Handle) = 0;

    virtual FGamePlatformVFXPreloadHandle Preload(const FGamePlatformVFXRequest& Request) = 0;
    virtual bool CancelPreload(const FGamePlatformVFXPreloadHandle& Handle) = 0;
    /** 游戏线程查询真实预载终态；Failed原因只诊断本请求，不影响玩法结果。 */
    virtual EGamePlatformVFXPreloadState GetPreloadState(const FGamePlatformVFXPreloadHandle& Handle, FString& OutError) const = 0;

    /**
     * 旧低层工具兼容入口。标准Gameplay必须由GamePlatformPresentation解析DefinitionId，
     * 新业务不得继续建立第二套VFX语义Catalog。
     */
    virtual FGamePlatformVFXRegistrationHandle RegisterCatalog(UGamePlatformVFXCatalog* Catalog) = 0;
    virtual bool UnregisterCatalog(const FGamePlatformVFXRegistrationHandle& Handle) = 0;
};
