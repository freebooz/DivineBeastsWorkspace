#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "Types/GamePlatformDataLease.h"
#include "Types/GamePlatformFlowTypes.h"
#include "DBAFoundationCoordinator.generated.h"

class UGameInstance;
class UWorld;
class UGamePlatformApplicationFlowSubsystem;

/** 本项目开发入口装配，显式启用且非发行构建才运行；不承担平台状态机职责。 */
UCLASS(Transient)
class UDBAFoundationCoordinator final : public UObject
{
    GENERATED_BODY()
public:
    /** 游戏线程；Owner 不能跨实例，协调器 Outer 由游戏实例持有。 */
    void Initialize(UGameInstance& Owner);
    /** 游戏线程撤销轮询与弱上下文；重复调用无副作用。 */
    void Shutdown();
    /** 返回拥有自身存储的诊断文字，关闭后不引用旧世界。 */
    FString GetDiagnostics() const { return Diagnostics; }
    /** 游戏线程校验显式开发授权、真实核心算法、资产管理器及已保存地图；失败不启动玩家流程。 */
    FGamePlatformResult ValidateConfiguration() const;
    /** 游戏线程转移已就绪探针租约到协调器；失败不消费输入，终态显示不借用释放后的定义。 */
    FGamePlatformResult AdoptProbe(FGamePlatformDataLease& InOutLease);
    /** 游戏线程从仍持有的租约读取实际资产数值；无就绪租约返回false并清零输出。 */
    bool ReadProbe(int32& OutValue) const;
    /** 真正屏障：当前实例的目标世界开始运行、有本地观察者且探针租约仍可读。 */
    bool IsFoundationReady() const;
    /** 仅主工程拥有地图选择；平台流程资产不引用地图。返回静态不可变包名。 */
    static const TCHAR* SandboxPackage();
    /** 显式开发命令；取消当前申请或运行，并释放探针。重复调用保持清理幂等。 */
    void CancelDevelopmentFlow();
    /** 仅显式开发入口可用；撤销旧异步代次后允许新运行，执行器自行分配新RunId。 */
    void RetryDevelopmentFlow();
private:
    bool Tick(float DeltaSeconds);
    void StartDevelopmentFlow();
    void OnFlowFinished(const FGamePlatformFlowSnapshot& Snapshot);
    void ReleaseDataLeases();
    TWeakObjectPtr<UGameInstance> OwnerInstance;
    TWeakObjectPtr<UWorld> ReportedWorld;
    FTSTicker::FDelegateHandle TickerHandle;
    FString RunId;
    FString Diagnostics = TEXT("基础工程开发验证未启用");
    FGamePlatformDataLease ProbeLease;
    FGamePlatformDataLease FlowLease;
    TWeakObjectPtr<UGamePlatformApplicationFlowSubsystem> FlowService;
    TArray<FGamePlatformFlowFactoryHandle> FactoryHandles;
    FGamePlatformFlowHandle ActiveFlow;
    FDelegateHandle FinishedHandle;
    FGamePlatformResult LastResult;
    uint64 RequestGeneration = 0;
    double DiscoveryDeadlineSeconds = 0.0;
    bool bStartAttempted = false;
    bool bStopping = true;
};
