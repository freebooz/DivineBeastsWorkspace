#include "Types/GamePlatformCombatProjectileFlight.h"

bool FGamePlatformCombatProjectileFlightConfig::IsValid(FString* OutError) const
{
    const auto Fail=[&](const TCHAR* Reason){if(OutError)*OutError=Reason;return false;};
    for(float V : {InitialSpeed,MaxSpeed,MinSpeed,Acceleration,Deceleration,AccelerationDuration,
        DecelerationDuration,CurveDuration,GravityScale,HomingAcceleration,MaxFlightDistance,MaxFlightTime})
        if(!FMath::IsFinite(V))return Fail(TEXT("飞行配置存在非有限数值。"));
    if(static_cast<uint8>(SpeedMode)>static_cast<uint8>(EGamePlatformProjectileSpeedMode::SpeedCurve) ||
        MaxSpeed<=0 || MaxSpeed>20000 || MinSpeed<0 || InitialSpeed<MinSpeed || InitialSpeed>MaxSpeed ||
        Acceleration<0 || Deceleration<0 || AccelerationDuration<0 || DecelerationDuration<0 ||
        CurveDuration<=0 || FMath::Abs(GravityScale)>10 || HomingAcceleration<0 || HomingAcceleration>1000000 ||
        MaxFlightDistance<=0 || MaxFlightDistance>100000 || MaxFlightTime<=0 || MaxFlightTime>120)
        return Fail(TEXT("飞行模式、速度、时长或距离超出合法边界。"));
    if(GravityScale!=0 && bEnableHoming)return Fail(TEXT("重力弹道须关闭锁定追踪，不能覆盖Bezier目标轨迹。"));
    if(SpeedMode==EGamePlatformProjectileSpeedMode::AccelerateThenCruise &&
        (AccelerationDuration<=0 || Acceleration*AccelerationDuration+.01f<MaxSpeed-InitialSpeed))
        return Fail(TEXT("加速时长与推进不足以达到巡航速度。"));
    if(SpeedMode==EGamePlatformProjectileSpeedMode::Decelerate && DecelerationDuration<=0)
        return Fail(TEXT("减速时长必须为正。"));
    if(SpeedMode==EGamePlatformProjectileSpeedMode::SpeedCurve)
    {
        if(!SpeedCurve || SpeedCurve->FloatCurve.GetNumKeys()<2)return Fail(TEXT("速度曲线缺失或关键帧不足。"));
        float Lo=0,Hi=0;SpeedCurve->FloatCurve.GetTimeRange(Lo,Hi);
        if(!FMath::IsNearlyZero(Lo) || !FMath::IsNearlyEqual(Hi,1.f))return Fail(TEXT("速度曲线时间范围必须为0..1。"));
        for(const auto& K:SpeedCurve->FloatCurve.GetConstRefOfKeys())
            if(!FMath::IsFinite(K.Time)||!FMath::IsFinite(K.Value)||K.Value<0||K.Value>1 ||
                !FMath::IsFinite(K.ArriveTangent)||!FMath::IsFinite(K.LeaveTangent)||
                !FMath::IsFinite(K.ArriveTangentWeight)||!FMath::IsFinite(K.LeaveTangentWeight))
                return Fail(TEXT("速度曲线关键帧必须有限且处于0..1。"));
        for(int32 I=0;I<=64;++I)
            if(!FMath::IsFinite(SpeedCurve->GetFloatValue(I/64.f)))return Fail(TEXT("速度曲线求值必须有限。"));
        if(!FMath::IsNearlyEqual(FMath::Lerp(MinSpeed,MaxSpeed,FMath::Clamp(SpeedCurve->GetFloatValue(0),0.f,1.f)),InitialSpeed,.1f))
            return Fail(TEXT("速度曲线起点必须匹配InitialSpeed，避免发射瞬间跳速。"));
    }
    if(OutError)OutError->Reset();return true;
}

float FGamePlatformCombatProjectileFlightConfig::SpeedAt(float Seconds) const
{
    if(!FMath::IsFinite(Seconds))return 0;
    const float T=FMath::Max(0.f,Seconds);
    switch(SpeedMode)
    {
    case EGamePlatformProjectileSpeedMode::ConstantAcceleration:return FMath::Min(MaxSpeed,InitialSpeed+Acceleration*T);
    case EGamePlatformProjectileSpeedMode::AccelerateThenCruise:return FMath::Min(MaxSpeed,InitialSpeed+Acceleration*FMath::Min(T,AccelerationDuration));
    case EGamePlatformProjectileSpeedMode::Decelerate:return FMath::Max(MinSpeed,InitialSpeed-Deceleration*FMath::Min(T,DecelerationDuration));
    case EGamePlatformProjectileSpeedMode::SpeedCurve:
        return SpeedCurve ? FMath::Lerp(MinSpeed,MaxSpeed,FMath::Clamp(SpeedCurve->GetFloatValue(FMath::Clamp(T/CurveDuration,0.f,1.f)),0.f,1.f)) : InitialSpeed;
    default:return InitialSpeed;
    }
}

float FGamePlatformCombatProjectileFlightConfig::DistanceBetween(float StartSeconds,float EndSeconds) const
{
    if(!FMath::IsFinite(StartSeconds)||!FMath::IsFinite(EndSeconds))return 0;
    const double A=FMath::Max(0.f,StartSeconds),B=FMath::Max(A,static_cast<double>(EndSeconds));
    if(SpeedMode==EGamePlatformProjectileSpeedMode::SpeedCurve)
    {
        // 时间域Simpson积分，不按帧数累计速度；每小段最多1/240秒，输出严格受速度上限约束。
        const int32 N=FMath::Clamp(FMath::CeilToInt((B-A)*240),1,28800);const double H=(B-A)/N;
        double Sum=0;for(int32 I=0;I<N;++I){const double T=A+I*H;Sum+=H*(SpeedAt(T)+4*SpeedAt(T+H*.5)+SpeedAt(T+H))/6;}
        return static_cast<float>(Sum);
    }
    const auto Primitive=[&](double T)
    {
        const bool Down=SpeedMode==EGamePlatformProjectileSpeedMode::Decelerate;
        if(SpeedMode==EGamePlatformProjectileSpeedMode::ConstantSpeed)return T*InitialSpeed;
        const double Rate=Down ? -Deceleration : Acceleration;
        const double Limit=Down ? MinSpeed : MaxSpeed;
        const double Duration=Down ? DecelerationDuration : (SpeedMode==EGamePlatformProjectileSpeedMode::AccelerateThenCruise ? AccelerationDuration : 1.e10);
        const double Cap=FMath::Abs(Rate)>UE_SMALL_NUMBER ? FMath::Max(0.,(Limit-InitialSpeed)/Rate) : 1.e10;
        const double Ramp=FMath::Min(T,FMath::Min(Duration,Cap));
        return InitialSpeed*Ramp+.5*Rate*Ramp*Ramp+(T-Ramp)*(InitialSpeed+Rate*Ramp);
    };
    return static_cast<float>(Primitive(B)-Primitive(A));
}
