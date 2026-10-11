// 单群固定步与逐帧插值上传；数组对应单次 Burst 的稳定 Particles.UniqueID。
// 不创建组件、不加载资源；首次激活前上传相同的前后快照，后续渲染有一固定步的平滑延迟。
#include "Environment/Fauna/GamePlatformFaunaRuntime.h"
#include "Environment/Fauna/GamePlatformFaunaBridge.h"
#include "Types/GamePlatformVFXParameters.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraTypes.h"
#include "Math/RotationMatrix.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
constexpr double FaunaFixedStepSeconds=1.0/30.0;
constexpr int32 FaunaMaximumSubsteps=8;

/** 局部球只在正等比缩放下仍是球；运行中父组件变换也必须满足同一约束。 */
bool IsPositiveUniformFaunaScale(const FVector& Scale)
{
    return !Scale.ContainsNaN() && Scale.GetMin()>UE_SMALL_NUMBER &&
        FMath::IsNearlyEqual(Scale.X/Scale.Y,1.0,1.e-4) && FMath::IsNearlyEqual(Scale.X/Scale.Z,1.0,1.e-4);
}
}

bool FGamePlatformFaunaRuntime::Initialize(UNiagaraComponent* InComponent,
    const FGamePlatformFaunaConfig& InConfig,const FGamePlatformVFXParameters& Overrides,
    const FName InContextId,FString& OutError)
{
    check(IsInGameThread());
    if (!IsValid(InComponent)) { OutError=TEXT("群落渲染组件无效。"); return false; }
    if (!IsPositiveUniformFaunaScale(InComponent->GetComponentScale()))
    { OutError=TEXT("群落只接受正的等比缩放，水域与惊扰球不能用非均匀缩放。"); return false; }

    // 实际系统可能来自质量/平台变体；void 数组 setter 失败不会报告，必须先核验类型。
    const UNiagaraSystem* System=InComponent->GetAsset();
    if (!IsValid(System) ||
        !UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayFloat3>(InComponent,TEXT("User.FaunaPositions")) ||
        !UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayQuat>(InComponent,TEXT("User.FaunaOrientations")) ||
        !UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayFloat4>(InComponent,TEXT("User.FaunaMotionStates")))
    {
        OutError=TEXT("群落Niagara系统必须提供FaunaPositions(Float3数组)、FaunaOrientations(Quat数组)、FaunaMotionStates(Float4数组)。");
        return false;
    }
    const FNiagaraVariableBase CountParameter(FNiagaraTypeDefinition::GetIntDef(),TEXT("User.FaunaCount"));
    if (System->GetExposedParameters().IndexOf(CountParameter)==INDEX_NONE)
    {
        OutError=TEXT("群落Niagara系统必须公开整数User.FaunaCount，供单次Burst绑定数量。");
        return false;
    }

    FGamePlatformFaunaConfig CandidateConfig=InConfig;
    if (const int32* Count=Overrides.IntegerParameters.Find(TEXT("User.FaunaCount"))) CandidateConfig.Count=*Count;
    if (const int32* Seed=Overrides.IntegerParameters.Find(TEXT("User.FaunaSeed"))) CandidateConfig.Seed=*Seed;
    if (const float* Speed=Overrides.FloatParameters.Find(TEXT("User.FaunaSpeed"))) CandidateConfig.CruiseSpeed=*Speed;
    GamePlatformFauna::FGamePlatformFaunaSimulation CandidateSimulation;
    if (!GamePlatformFaunaBridge::Initialize(CandidateSimulation,CandidateConfig,OutError)) return false;

    // 所有可失败预检结束后才发布本实例状态，保留 Initialize 的失败原子性。
    Config=MoveTemp(CandidateConfig);
    Simulation=MoveTemp(CandidateSimulation);
    Component=InComponent; ContextId=InContextId; AccumulatorSeconds=0.0;
    const FVector Padding(FMath::Max(50.0f,Config.BodyRadius*3.0f));
    InComponent->SetSystemFixedBounds(FBox(-Config.HalfExtent-Padding,Config.HalfExtent+Padding));
    InComponent->SetVariableInt(TEXT("User.FaunaCount"),Config.Count);
    CaptureSnapshot(CurrentSnapshot);
    PreviousSnapshot=CurrentSnapshot;
    UploadInterpolated(0.0);
    OutError.Reset();
    return true;
}

bool FGamePlatformFaunaRuntime::IsRenderingActive() const
{
    const UNiagaraComponent* C=Component.Get();
    if (!C || !C->IsActive() || C->IsPaused()) return false;
    const auto Controller=C->GetSystemInstanceController();
    // Controller->IsValid 保护尚未初始化或已被回收的内部实例，随后读取真实伸缩暂停状态。
    return Controller && Controller->IsValid() && !Controller->IsPaused() &&
        Controller->GetRequestedExecutionState()==ENiagaraExecutionState::Active &&
        Controller->GetActualExecutionState()==ENiagaraExecutionState::Active;
}

void FGamePlatformFaunaRuntime::Tick(const double DeltaSeconds)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformVFX_Fauna);
    check(IsInGameThread());
    if (!IsRenderingActive() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds<=0.0) return;

    // 每帧时间只在本插值层累计一次；生产模拟器始终收到整固定步，避免双重可变步推进。
    // 限制补步量可防长帧拖垮后续帧；丢弃过量时间，不把它转换成穿越水域的大位移。
    AccumulatorSeconds+=FMath::Min(DeltaSeconds,FaunaFixedStepSeconds*FaunaMaximumSubsteps);
    int32 Substeps=0;
    while (AccumulatorSeconds+FaunaFixedStepSeconds*1.e-9>=FaunaFixedStepSeconds && Substeps<FaunaMaximumSubsteps)
    {
        Swap(PreviousSnapshot,CurrentSnapshot);
        Simulation.Step(FaunaFixedStepSeconds);
        CaptureSnapshot(CurrentSnapshot);
        AccumulatorSeconds=FMath::Max(0.0,AccumulatorSeconds-FaunaFixedStepSeconds);
        ++Substeps;
    }
    UploadInterpolated(FMath::Clamp(AccumulatorSeconds/FaunaFixedStepSeconds,0.0,1.0));
}

void FGamePlatformFaunaRuntime::CaptureSnapshot(FRenderSnapshot& OutSnapshot) const
{
    const auto& Agents=Simulation.GetAgents();
    const int32 Count=static_cast<int32>(Agents.size());
    OutSnapshot.Positions.SetNum(Count,EAllowShrinking::No);
    OutSnapshot.Orientations.SetNum(Count,EAllowShrinking::No);
    OutSnapshot.MotionStates.SetNum(Count,EAllowShrinking::No);
    for (int32 Index=0;Index<Count;++Index)
    {
        const auto& A=Agents[static_cast<std::size_t>(Index)];
        OutSnapshot.Positions[Index]=FVector(A.Position.X,A.Position.Y,A.Position.Z);
        const FVector Velocity(A.Velocity.X,A.Velocity.Y,A.Velocity.Z);
        FVector Forward=Velocity;
        FVector Up(A.UpVector.X,A.UpVector.Y,A.UpVector.Z);
        Up=Up.GetSafeNormal();
        if (Up.IsNearlyZero()) Up=FVector::UpVector;
        if (Forward.IsNearlyZero())
        {
            Forward=FVector::CrossProduct(FVector::RightVector,Up).GetSafeNormal();
            if (Forward.IsNearlyZero()) Forward=FVector::ForwardVector;
        }
        OutSnapshot.Orientations[Index]=FRotationMatrix::MakeFromXZ(Forward.GetSafeNormal(),Up).ToQuat().GetNormalized();
        // 速度比必须取真实速度；停栖用于朝向的单位切线不能被误算成仍在移动。
        OutSnapshot.MotionStates[Index]=FVector4(A.AnimationPhase,A.FlightWeight,
            Velocity.Size()/static_cast<double>(Config.CruiseSpeed),static_cast<double>(A.Behavior));
    }
}

void FGamePlatformFaunaRuntime::UploadInterpolated(const double Alpha)
{
    UNiagaraComponent* C=Component.Get();
    if (!C) return;
    const int32 Count=CurrentSnapshot.Positions.Num();
    check(PreviousSnapshot.Positions.Num()==Count && PreviousSnapshot.Orientations.Num()==Count &&
        PreviousSnapshot.MotionStates.Num()==Count && CurrentSnapshot.Orientations.Num()==Count &&
        CurrentSnapshot.MotionStates.Num()==Count);
    Positions.SetNum(Count,EAllowShrinking::No);
    Orientations.SetNum(Count,EAllowShrinking::No);
    MotionStates.SetNum(Count,EAllowShrinking::No);
    for (int32 Index=0;Index<Count;++Index)
    {
        Positions[Index]=FMath::Lerp(PreviousSnapshot.Positions[Index],CurrentSnapshot.Positions[Index],Alpha);
        Orientations[Index]=FQuat::Slerp(PreviousSnapshot.Orientations[Index],CurrentSnapshot.Orientations[Index],Alpha);
        const FVector4& Previous=PreviousSnapshot.MotionStates[Index];
        const FVector4& Current=CurrentSnapshot.MotionStates[Index];
        double PhaseDelta=Current.X-Previous.X;
        if (PhaseDelta>0.5) PhaseDelta-=1.0;
        else if (PhaseDelta< -0.5) PhaseDelta+=1.0;
        const double Phase=FMath::Fmod(Previous.X+PhaseDelta*Alpha+1.0,1.0);
        // W 是枚举，不能线性混合成不存在的状态；X 的环绕采用最近周期路径。
        MotionStates[Index]=FVector4(Phase,FMath::Lerp(Previous.Y,Current.Y,Alpha),
            FMath::Lerp(Previous.Z,Current.Z,Alpha),Current.W);
    }
    UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(C,TEXT("User.FaunaPositions"),Positions);
    UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(C,TEXT("User.FaunaOrientations"),Orientations);
    UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(C,TEXT("User.FaunaMotionStates"),MotionStates);
}

bool FGamePlatformFaunaRuntime::Disturb(const FVector& WorldCenter,const float Radius,
    const float Strength,const float DurationSeconds,FString& OutError)
{
    check(IsInGameThread());
    OutError.Reset();
    UNiagaraComponent* C=Component.Get();
    if (!C) { OutError=TEXT("群落渲染组件已失效。"); return false; }
    const FVector Scale=C->GetComponentScale();
    if (!IsPositiveUniformFaunaScale(Scale))
    { OutError=TEXT("群落运行中出现非正等比缩放，无法转换世界惊扰球。"); return false; }
    const FVector Local=C->GetComponentTransform().InverseTransformPosition(WorldCenter);
    const double LocalRadius=Radius/Scale.X;
    if (!FBox(-Config.HalfExtent,Config.HalfExtent).ExpandBy(LocalRadius).IsInsideOrOn(Local)) return false;
    std::string Error;
    const bool Accepted=Simulation.SetDisturbance({Local.X,Local.Y,Local.Z},LocalRadius,Strength,DurationSeconds,Error);
    if (!Accepted) OutError=FString(UTF8_TO_TCHAR(Error.c_str()));
    return Accepted;
}

FGamePlatformFaunaSnapshot FGamePlatformFaunaRuntime::GetSnapshot() const
{
    check(IsInGameThread());
    FGamePlatformFaunaSnapshot Result;
    Result.SimulatedCount=static_cast<int32>(Simulation.GetAgents().size());
    Result.LocalPositions=CurrentSnapshot.Positions;
    Result.bRenderingActive=IsRenderingActive();
    for (const auto& A:Simulation.GetAgents())
    {
        if (A.Behavior==GamePlatformFauna::EBehavior::Perched) ++Result.PerchedCount;
        if (A.FlowerIndex>=0) ++Result.ReservedFlowerSlots;
    }
    return Result;
}
