// GPU有限邻域Steering模块；Near*由Niagara NeighborGrid3D空间邻居阶段填充。
// 禁止在此执行全粒子配对O(N^2)。未配置邻域采样时Neighbors=0，返回零Steering安全降级。
// 分离/对齐/聚合三规则先聚合再限幅，后续Spline跟随保留设计师路径牵引优先。
#if GPU_SIMULATION
float3 steer = float3(0.0,0.0,0.0);
if (Neighbors > 0.5)
{
    float inv = rcp(max(1.0,Neighbors));
    float3 meanPos = NeighborPositionSum * inv;
    float3 meanVel = NeighborVelocitySum * inv;
    float3 separation = length(SeparationSum) > 0.0001 ? normalize(SeparationSum) : float3(0,0,0);
    float3 alignment = length(meanVel) > 0.0001 ? normalize(meanVel) : float3(0,0,0);
    float3 cohesion = length(meanPos-Particles.Position)>0.0001 ?
        normalize(meanPos-Particles.Position) : float3(0,0,0);
    steer = separation*clamp(SeparationWeight,0,4)+
        alignment*clamp(AlignmentWeight,0,4)+cohesion*clamp(CohesionWeight,0,4);
    float magnitude = length(steer);
    if (magnitude>1.0) steer /= magnitude;
}
Particles.BirdFlockSteering = steer;
#endif
