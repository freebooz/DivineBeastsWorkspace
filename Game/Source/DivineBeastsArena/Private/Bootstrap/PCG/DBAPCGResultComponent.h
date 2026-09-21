#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformPCGTypes.h"
#include "DBAPCGResultComponent.generated.h"

class UGameInstance;

/** 项目世界结果持有者，不复制、不Tick、不持有原生PCG组件或Data租约。
 * 组合根在当前世界Actor上NewObject、AddInstanceComponent、RegisterComponent后配置。
 * Actor/组件必须独立于Loading任务存活；Outer本身不等于GC强引用。
 * 所有入口只允许游戏线程；一组件同时仅持有一个请求，Cleaning不是可复用状态。
 */
UCLASS(Transient)
class UDBAPCGResultComponent final : public UActorComponent
{
    GENERATED_BODY()
public:
    UDBAPCGResultComponent();

    /** 一次绑定World公开代次、逻辑区域与厘米中心；不创建区域、不初始化World或Session。
     * 只消费World基础事实及QueryRegion，不等待总Ready；前置不足明确失败，调用方可稍后重试。
     */
    FGamePlatformResult BindWorldRegion(const FGuid& ContextGeneration,
        const FGamePlatformId& RegionId, const FVector& CenterCm);

    /** Loading任务调用；ProfileId交给PCG执行真实Data加载。拒绝跨实例及未清理的旧请求。
     * 成功仅表示接纳；OutHandle用于之后所有精确操作，不能用组件的最新句柄代替旧任务句柄。
     */
    FGamePlatformResult StartGeneration(UGameInstance& Instance, const FPrimaryAssetId& ProfileId,
        FGamePlatformPCGHandle& OutHandle);

    /** 精确句柄值快照；基础世界、区域或组件失效时立即拒绝历史成功，不修改PCG状态。
     * 返回的失败不代表已完成清理；区域/Owner失效的实际输出清理由PCG服务负责。
     */
    FGamePlatformPCGSnapshot ReadGeneration(const FGamePlatformPCGHandle& Handle) const;

    /** 显式结束本结果保留期或取消未完成请求；服务负责异步清理，保留句柄供查询Cleaning/Cleaned。
     * 返回失败时仍保留所有权，可在非回调发布阶段重试；不直接销毁原生组件、Actor或资产。
     */
    FGamePlatformResult ReleaseGeneration(const FGamePlatformPCGHandle& Handle);

    /** 当前已签发句柄的值副本，仅供组合根诊断/显式释放，不转移所有权。 */
    FGamePlatformPCGHandle GetGenerationHandle() const;

protected:
    /** 世界退出或宿主Actor结束时拒绝新任务，并请求PCG清理；不宣称同步清理完成。 */
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    /** 显式销毁组件也结束保留期，不依赖Loading任务或GC回调触发资源回收。 */
    virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
private:
    FGamePlatformResult ValidateBoundContext() const;
    void StopOwnedGeneration();

    TWeakObjectPtr<UWorld> BoundWorld;
    FGuid BoundGeneration;
    FGamePlatformId BoundRegion;
    FVector BoundCenterCm = FVector::ZeroVector;
    FGamePlatformPCGHandle GenerationHandle;
    bool bIsBound = false;
    bool bIsStopping = false;
};
