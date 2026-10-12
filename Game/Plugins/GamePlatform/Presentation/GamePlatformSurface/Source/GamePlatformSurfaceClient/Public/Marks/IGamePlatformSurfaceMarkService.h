// 表面痕迹调用接口，不暴露内部贴花队列或资产加载器。
#pragma once
#include "CoreMinimal.h"
#include "Marks/GamePlatformSurfaceMarkTypes.h"

class UWorld;
/** 真实贴花生成后的客户端事件；VFX／音效可选择监听，不传业务权威。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformSurfaceMarkSpawned, const FGamePlatformSurfaceMarkRequest&);

/** 每个客户端世界独立拥有痕迹、贴花组件、缓存和资源租约。 */
class GAMEPLATFORMSURFACECLIENT_API IGamePlatformSurfaceMarkService
{
public:
    virtual ~IGamePlatformSurfaceMarkService() = default;
    static IGamePlatformSurfaceMarkService* Get(UWorld& World);
    /** 线程安全性：只允许GameThread调用；Pending仅受理异步请求。 */
    virtual FGamePlatformSurfaceMarkResult Submit(const FGamePlatformSurfaceMarkRequest& Request) = 0;
    /** 显式刷新已变更的软目录，先失效旧代次后释放当前世界租约。 */
    virtual bool RefreshCatalog() = 0;
    virtual FDelegateHandle AddSpawnedHandler(const FGamePlatformSurfaceMarkSpawned::FDelegate& Handler) = 0;
    virtual void RemoveSpawnedHandler(FDelegateHandle Handle) = 0;
    /** 获取当前世界已生成/等待痕迹数，不包含其他世界。 */
    virtual void GetCounts(int32& OutActive, int32& OutPending) const = 0;
};
