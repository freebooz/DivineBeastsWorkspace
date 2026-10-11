// 既有 VFX 实例附属的群落计算器：不创建 Niagara 组件，不加载资源，不建立独立池。
// 单线程拥有 CPU 固定步状态和前后渲染快照，随世界实例句柄销毁；数组为组件自有 DI 的副本。
#pragma once
#include "CoreMinimal.h"
#include "Environment/GamePlatformFaunaConfig.h"
#include "Environment/Fauna/GamePlatformFaunaSimulation.h"
class UNiagaraComponent;
struct FGamePlatformVFXParameters;

/** 仅游戏线程，由既有 VFX 世界服务持有，与实例句柄同时销毁。 */
class FGamePlatformFaunaRuntime final
{
public:
    /** 激活前校验当前实际系统的数组与数量参数类型；失败不替换已有合法运行状态。 */
    bool Initialize(UNiagaraComponent* InComponent,const FGamePlatformFaunaConfig& InConfig,
        const FGamePlatformVFXParameters& Overrides,FName InContextId,FString& OutError);
    /** 由世界逐帧调用；30 Hz 固定步最多补八步，每帧插值上传，伸缩/用户暂停时冻结。 */
    void Tick(double DeltaSeconds);
    /** 世界厘米刺激只支持组件正等比缩放，转换成局部半径；非法输入不覆盖原刺激。 */
    bool Disturb(const FVector& WorldCenter,float Radius,float Strength,float DurationSeconds,FString& OutError);
    /** 返回当前 CPU 状态；LocalPositions 不冒充存在一固定步延迟的渲染插值位置。 */
    FGamePlatformFaunaSnapshot GetSnapshot() const;
    FName GetContextId() const { return ContextId; }
private:
    /** 单个已完成固定步的姿态快照，三数组槽位均对应单次 Burst 的稳定 Particles.UniqueID。 */
    struct FRenderSnapshot
    {
        TArray<FVector> Positions;     // 局部厘米中心。
        TArray<FQuat> Orientations;   // 单位旋转四元数。
        TArray<FVector4> MotionStates;// X 周期相位，Y 飞行权重，Z 实际速度比，W 离散状态。
    };
    /** 使用 Niagara 控制器真实状态，避免 IsPaused 只反映用户暂停而漏掉伸缩暂停。 */
    bool IsRenderingActive() const;
    /** 只从生产模拟器读取已完成状态；不推进模拟，不生成第二套行为。 */
    void CaptureSnapshot(FRenderSnapshot& OutSnapshot) const;
    /** Alpha 为 [0,1]：位置线性、朝向球面、周期相位短路径环绕插值；状态枚举保持离散。 */
    void UploadInterpolated(double Alpha);

    TWeakObjectPtr<UNiagaraComponent> Component;
    FGamePlatformFaunaConfig Config;
    FName ContextId=NAME_None;
    GamePlatformFauna::FGamePlatformFaunaSimulation Simulation;
    FRenderSnapshot PreviousSnapshot;
    FRenderSnapshot CurrentSnapshot;
    double AccumulatorSeconds=0.0;     // 渲染帧未满固定步的余量；暂停期间不增长。
    TArray<FVector> Positions;         // 本帧渲染副本，独立于当前 CPU 快照。
    TArray<FQuat> Orientations;
    TArray<FVector4> MotionStates;
};
