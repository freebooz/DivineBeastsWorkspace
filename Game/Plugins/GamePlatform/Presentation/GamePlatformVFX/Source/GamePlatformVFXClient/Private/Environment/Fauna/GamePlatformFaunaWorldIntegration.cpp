// 群落能力附属于现有世界VFX实例：共享句柄、取消、资源租约、池与终态。
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Environment/Fauna/GamePlatformFaunaRuntime.h"
#include "Engine/World.h"

void UGamePlatformVFXWorldSubsystem::HandleFaunaWorldTick(UWorld* World,ELevelTick TickType,float DeltaSeconds)
{
    if (World==GetWorld() && !bClosing) TickFaunaInstances();
}

void UGamePlatformVFXWorldSubsystem::TickFaunaInstances()
{
    check(IsInGameThread());
    UWorld* World=GetWorld();
    if (bClosing || !World) return;
    const double Now=World->GetTimeSeconds();
    const double Delta=FMath::Clamp(Now-LastFaunaWorldTime,0.0,0.25);
    LastFaunaWorldTime=Now;
    if (Delta<=0.0) return; // 暂停世界或重复时间戳不推进模拟。
    TArray<FGuid> Keys; FaunaInstances.GetKeys(Keys);
    for (const FGuid& Id:Keys)
    {
        const auto* Entry=FaunaInstances.Find(Id);
        if (!Entry) continue;
        const auto Handle=Entry->Handle;
        const auto Runtime=Entry->Runtime;
        if (!InstanceRegistry.GetComponent(Handle))
        {
            CleanupInstance(Handle,false,EGamePlatformVFXPlaybackState::Completed,
                EGamePlatformVFXResultCode::AlreadyCompleted,TEXT("群落所属Niagara组件已释放。"));
            continue;
        }
        if (Runtime) Runtime->Tick(Delta);
    }
}

int32 UGamePlatformVFXWorldSubsystem::DisturbFauna(const FVector& WorldCenter,
    const float Radius,const float Strength,const float DurationSeconds,FString& OutError)
{
    check(IsInGameThread()); OutError.Reset();
    if (bClosing || WorldCenter.ContainsNaN() || !FMath::IsFinite(Radius) || Radius<=0.0f || Radius>100000.0f ||
        !FMath::IsFinite(Strength) || Strength<0.0f || Strength>1.0f ||
        !FMath::IsFinite(DurationSeconds) || DurationSeconds<=0.0f || DurationSeconds>60.0f)
    { OutError=TEXT("群落惊扰必须是有限位置、正半径、0..1强度及不超过60秒的持续时间。"); return 0; }
    int32 Affected=0;
    for (const auto& Pair:FaunaInstances)
        if (Pair.Value.Runtime && Pair.Value.Runtime->Disturb(WorldCenter,Radius,Strength,DurationSeconds,OutError)) ++Affected;
    return Affected;
}

bool UGamePlatformVFXWorldSubsystem::GetFaunaSnapshot(const FName ContextId,FGamePlatformFaunaSnapshot& OutSnapshot) const
{
    check(IsInGameThread()); OutSnapshot=FGamePlatformFaunaSnapshot();
    if (bClosing || ContextId.IsNone()) return false;
    for (const auto& Pair:FaunaInstances)
        if (Pair.Value.Runtime && Pair.Value.Runtime->GetContextId()==ContextId)
        { OutSnapshot=Pair.Value.Runtime->GetSnapshot(); return true; }
    return false;
}
