// Niagara GPU中立样条进度计算。Speed(cm/s)、Length(cm)、Clock(s)；0/1/2旧行为不得调入此模块。
// 整群共享由Seed派生的路径进度，Individual仅留给局部绕群和翅膀相位；否则鸟会散满整条Spline。
#if GPU_SIMULATION
float routeLength = max(PathLength, 1.0);
float groupPhase = frac(Seed * 0.000123);
float normTime = groupPhase + max(0.0, Clock) * max(0.0, Speed) / routeLength;
float progress = saturate(normTime);
float finished = normTime >= 1.0 ? 1.0 : 0.0;
if (Mode > 4.5)
{
    float t = frac(normTime * 0.5) * 2.0;
    progress = t <= 1.0 ? t : 2.0 - t;
    finished = 0.0;
}
else if (Mode > 3.5)
{
    progress = frac(normTime);
    finished = 0.0;
}
Particles.RouteProgress = progress;
Particles.RouteFinished = finished;
#endif
