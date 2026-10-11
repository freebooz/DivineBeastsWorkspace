// 平台通用鸟群GPU局部轨迹。输入只包含中立运动参数，无任何项目物种身份。
// 由Monolith导入真实Niagara模块；单粒子就是单只鸟，使用System.Age而非CPU逐鸟Tick。
// 不做权威碰撞：设计师提供安全飞行体积，最低局部高度保证不穿过锚点平面。
#if GPU_SIMULATION
float seedPhase = frac(Individual * 1.6180339887 + Seed * 0.000137);
float phase = seedPhase * 6.28318530718;
float r = max(Radius, 200.0) * (0.73 + 0.31 * frac(seedPhase * 7.37));
float speedRatio = 0.85 + 0.24 * frac(seedPhase * 3.19);
float speed = clamp(Speed, 10.0, 5000.0) * speedRatio;
float angle = phase + Clock * speed / r;
float sx = sin(angle);
float cx = cos(angle);
float3 pos = float3(r*cx,r*sx,Height);
float3 tangent = float3(-sx,cx,0.0);
float bank = 0.12;
if (Mode > 1.5)
{
    pos.xy = float2(r*sx,0.55*r*sin(2.0*angle));
    tangent = normalize(float3(cx,1.1*cos(2.0*angle),0.0));
    bank = 0.18*sin(angle);
}
else if (Mode > 0.5)
{
    // 拉长闭合轨迹模拟定向掠过，避免原型直线边缘瞬移。
    pos.xy = float2(1.7*r*cx,0.42*r*sx);
    tangent = normalize(float3(-1.7*sx,0.42*cx,0.0));
    bank = 0.12 + 0.08*cx;
}
float drift = clamp(Wind,0.0,2000.0)*sin(Clock*0.24+phase);
pos.xy += float2(drift,drift*0.26);
pos.z = max(Height*0.35,Height+(seedPhase-0.5)*180.0+sin(Clock*0.63+phase)*42.0);
float yaw = atan2(tangent.y,tangent.x);
float sy=sin(yaw*0.5), cy=cos(yaw*0.5);
float sr=sin(bank*0.5), cr=cos(bank*0.5);
Particles.Position = pos;
Particles.Velocity = tangent*speed;
Particles.MeshOrientation = float4(cy*sr,sy*sr,sy*cr,cy*cr);
Particles.Scale = (0.86+frac(seedPhase*4.17)*0.25).xxx;
Particles.Color = float4(1.0,1.0,1.0,1.0);
#endif
