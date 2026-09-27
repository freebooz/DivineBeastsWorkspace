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
     * 先登记再加载。ExpectedClass不能为空；WeakCaller必须存活且属于本实例，实例本身允许跨图。
     * World期限要求调用者有属于本实例的世界。Completion必须非空；OutResult说明是否接纳。
     * 接纳后返回Loading句柄，成功／失败／取消至多通知一次且总是延后；成功保留资源至Release。
     * 同步拒绝返回无效句柄，仍向有效WeakCaller延后通知失败；调用者销毁时不再执行其回调。
     */
    virtual FGamePlatformDataLease AcquireDefinition(const FPrimaryAssetId& DefinitionId,
        TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass, const TArray<FName>& Bundles,
        EGamePlatformDataLifetime Lifetime, TWeakObjectPtr<UObject> WeakCaller,
        FGamePlatformDataCompletion Completion, FGamePlatformResult& OutResult) = 0;
    /** 成功租约读取只读定义；跨作用域、过期、已释放、非就绪或调用者失效均返回nullptr。 */
    virtual const UGamePlatformDefinitionBase* GetLoadedDefinition(const FGamePlatformDataLease& Lease) const = 0;
    /** 仅释放本作用域本代次需求；重复释放实际签发过的同一完整句柄成功，伪造／跨作用域失败。 */
    virtual FGamePlatformResult ReleaseDefinition(const FGamePlatformDataLease& Lease) = 0;
    /** 返回即时状态；已移除的合法本作用域句柄返回Released，非法／跨作用域返回Invalid。 */
    virtual EGamePlatformDataRequestState GetLeaseState(const FGamePlatformDataLease& Lease) const = 0;
    /** 返回本实例的值诊断，不暴露进程登记表或其他实例资源。 */
    virtual FGamePlatformDataDiagnostics GetDiagnostics() const = 0;
};
