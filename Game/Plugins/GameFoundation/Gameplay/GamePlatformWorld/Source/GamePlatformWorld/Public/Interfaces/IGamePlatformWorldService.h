#pragma once
#include "Types/GamePlatformRegion.h"
#include "Types/GamePlatformWorldStreaming.h"
#include "Interfaces/IGamePlatformWorldReadinessContributor.h"
class UWorld;
/** 仅游戏线程的世界作用域门面；所有UObject弱引用必须属于该世界，无万能对象查找。 */
class GAMEPLATFORMWORLD_API IGamePlatformWorldService
{
public:
    virtual ~IGamePlatformWorldService() = default;
    /** 只返回已有Game/PIE运行世界服务；无服务返回nullptr，不创建全局替代。 */
    static IGamePlatformWorldService* Get(UWorld& World);
    /** 只在显式FoundationWorld、非Shipping且离线/开发专服启动；联网客户端拒绝此例外。
     * DefinitionId由Data加载；BuildVersion是项目构建版本；重复调用返回Busy，不覆盖已有上下文。
     */
    virtual FGamePlatformResult InitializeDevelopment(const FPrimaryAssetId& DefinitionId, const FGamePlatformVersion& BuildVersion, FName ServerRole = NAME_None) = 0;
    /** 无真实Session公开快照时明确Unsupported；不接收客户端自编Assignment。 */
    virtual FGamePlatformResult InitializeSessionWorld() = 0;
    /** 立即采样现状的值副本，不能借用内部定义指针。 */
    virtual FGamePlatformWorldReadinessSnapshot GetReadiness() = 0;
    /** 只登记当前已声明且已加载区域；重复ID失败。 */
    virtual FGamePlatformWorldRegistration RegisterRegionProvider(const FGamePlatformRegionProvider& Provider, FGamePlatformResult& OutResult) = 0;
    /** 精确身份幂等撤销；随后采样发布观察者离开，不影响其他世界。 */
    virtual bool UnregisterRegionProvider(const FGamePlatformWorldRegistration& Registration) = 0;
    /** 空区域表示不在区域内；歧义返回失败，输出清空，不按注册顺序任选。 */
    virtual FGamePlatformResult QueryRegion(const FVector& Position, FGamePlatformId& OutRegion) = 0;
    /** 显式观察者位置，单位厘米；世界/代次不符拒绝；同位置不重复进入通知。 */
    virtual FGamePlatformResult UpdateObserver(TWeakObjectPtr<UObject> Observer, const FVector& Position, const FGuid& Generation) = 0;
    /** 弱订阅拥有者，回调仅游戏线程延后发布；失效自动撤销。 */
    virtual FGamePlatformWorldRegistration SubscribeRegions(TWeakObjectPtr<UObject> Owner, TFunction<void(const FGamePlatformRegionEvent&)> Callback) = 0;
    /** 精确句柄撤销订阅或贡献者，不接受跨世界句柄。 */
    virtual bool Unregister(const FGamePlatformWorldRegistration& Registration) = 0;
    /** 贡献者由本次注册持有，Owner失效则失败就绪并自动撤销；取消后须显式恢复。 */
    virtual FGamePlatformWorldRegistration RegisterReadinessContributor(TWeakObjectPtr<UObject> Owner, TSharedRef<IGamePlatformWorldReadinessContributor> Contributor, FGamePlatformResult& OutResult) = 0;
    /** 资源使用权到取消/世界关闭为止，Ready不自动撤销流送源。 */
    virtual FGamePlatformWorldStreamingHandle RequestStreaming(const FGamePlatformWorldStreamingRequest& Request, FGamePlatformResult& OutResult) = 0;
    /** 撤销本次请求，不主动卸载其他用户所需资源。 */
    virtual FGamePlatformResult CancelStreaming(const FGamePlatformWorldStreamingHandle& Handle) = 0;
};
