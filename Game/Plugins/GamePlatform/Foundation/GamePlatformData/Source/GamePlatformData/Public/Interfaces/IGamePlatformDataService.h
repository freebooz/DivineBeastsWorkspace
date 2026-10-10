#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformDataLease.h"

class UGameInstance;
/** 游戏线程下一调度轮或更晚通知；弱调用者失效时抑制外部回调并清理租约。定义指针仅在成功且未释放时有效。 */
using FGamePlatformDataCompletion = TFunction<void(const FGamePlatformDataLease&, const FGamePlatformResult&)>;

/** 游戏实例作用域的数据只读门面。所有接口仅游戏线程调用，无网络授权含义；不包含私有子系统。 */
class GAMEPLATFORMDATA_API IGamePlatformDataService
{
public:
    virtual ~IGamePlatformDataService() = default;
    /** 取得已初始化实例的私有服务；无服务时返回nullptr，不创建全局替身。 */
    static IGamePlatformDataService* Get(UGameInstance& GameInstance);
    /**
     * 先登记再加载。ExpectedClass不能为空或废弃，可为抽象定义根类；仅用作已加载对象的IsA约束，
     * 不实例化约束类，实际对象仍必须通过定义身份、版本、依赖和内容校验。
     * WeakCaller必须存活且属于本实例，实例本身允许跨图。
     * World期限要求调用者有属于本实例的世界。Completion必须非空；OutResult说明是否接纳。
     * 接纳后返回Loading句柄，成功／失败／取消至多通知一次且总是延后；成功保留资源至Release。
     * 同步拒绝返回无效句柄，仍向有效WeakCaller延后通知失败；调用者销毁时不再执行其回调。
     */
    virtual FGamePlatformDataLease AcquireDefinition(const FPrimaryAssetId& DefinitionId,
        TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass, const TArray<FName>& Bundles,
        EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
        FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult) = 0;
    /**
     * 为普通软对象/软类建立中央资源租约，保留发布路径/主资产身份。仅游戏线程，路径不能为空且不得重复超限。
     * 调用方/期限约束与AcquireDefinition相同；完成总是延后且至多一次。成功后可ResolveObject直到释放。
     * 唯一AssetManager持有原生StreamableHandle；取消只撤销本租约，不Unload其他世界或外部请求。
     */
    virtual FGamePlatformDataLease AcquireResources(const TArray<FSoftObjectPath>& ResourcePaths,
        EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
        FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult) = 0;
    /** 释放普通资源的完整签发句柄；成功/失败/取消后重复释放幂等，错实例或篡改失败。 */
    virtual FGamePlatformResult ReleaseResources(const FGamePlatformDataLease& Lease) = 0;
    /** 成功租约读取只读定义；跨作用域、过期、已释放、非就绪或调用者失效均返回nullptr。 */
    virtual const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease& Lease) const = 0;
    /** 仅释放本作用域本代次需求；签发证明支持旧完整句柄无状态幂等，伪造／跨作用域失败。 */
    virtual FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease& Lease) = 0;
    /**
     * 返回即时可访问状态；调用方/世界失效或合法已移除句柄返回Released，非法/跨作用域返回Invalid。
     * 世界退出由事件即时撤销；无统一销毁事件的GC弱调用者最多0.25秒维护周期后释放实际资源需求。
     */
    virtual EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const = 0;
    /** 返回本实例的值诊断，不暴露进程登记表或其他实例资源。 */
    virtual FGamePlatformDataDiagnostics GetDiagnostics() const = 0;
};
