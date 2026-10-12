// Niagara GPU中立样条进度计算。Speed(cm/s)、Length(cm)、Clock(s)；0/1/2旧行为不得调入此模块。
// ParticleID相位差避免整群鸟同步飞行，RouteProgress在Spline DI绑定模块采样路径。
#if GPU_SIMULATION
float routeLength = max(PathLength, 1.0);
float stablePhase = frac(Individual * 1.61803398875 + Seed * 0.000123);
float speedRatio = 0.86 + 0.25 * frac(stablePhase * 3.137);
float normTime = stablePhase + max(0.0, Clock) * max(0.0, Speed) * speedRatio / routeLength;
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
