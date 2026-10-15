// 平台通用鸟群GPU局部轨迹。输入只包含中立运动参数，无任何项目物种身份。
// 由Monolith导入真实Niagara模块；单粒子就是单只鸟，使用System.Age而非CPU逐鸟Tick。
// 不做权威碰撞：设计师提供安全飞行体积，最低局部高度保证不穿过锚点平面。
// 原Individual随机输入仅保留尺寸差异；位置从ParameterMap读本次一次Burst的Particles.UniqueID。
// User.BirdCount为实际生成数，User.FlockMinimumSpacing为含缩放、振翅WPO与余量的中心距厘米。
// 直接读取已注册参数，保持现有模块输入接口和资产身份，不重建已连接的Niagara图。
// 全群共用角速度/风/起伏；固定高度层在所有时间保留间距，不增加逐鸟CPU工作或邻域扫描。
#if GPU_SIMULATION
float count = max(1.0, floor(User.BirdCount));
float index = fmod(max(0.0, floor(Particles.UniqueID)), count);
float seedPhase = frac(Individual * 1.6180339887 + Seed * 0.000137);
float seedAngle = frac(Seed * 0.000137) * 6.28318530718;
// 原轨迹半径均值约0.885R；0.9R保留原飞行区域，取消独立半径/角速度导致的追近。
float r = max(Radius, 200.0) * 0.9;
float spacing = max(1.0, User.FlockMinimumSpacing);
float minimumAxis = 1.0;
if (Mode > 0.5)
{
    minimumAxis = 0.42;
}
float capacity = max(1.0, floor(3.14159265359 / asin(min(1.0, spacing / (2.0*r*minimumAxis)))));
// asin参数饱和会给出容量2，但实际直径已小于间距；此时只能每层1只，不能夹值后假保底。
if (spacing > 2.0*r*minimumAxis)
{
    capacity = 1.0;
}
float laneCount = max(1.0, ceil(count / capacity));
float phaseExtent = 6.28318530718;
if (Mode > 1.5)
{
    // 八字(x=sin(a),y=.55sin(2a))在半圈内有距离下界.65R*abs(sin(delta))。
    // 每层仅占半圈并留一个相位间隔；不同半圈/重叠层由固定高度隔开，交叉点也不会靠近。
    capacity = max(1.0, floor(3.14159265359 / asin(min(1.0, spacing / (0.65*r)))));
    if (spacing > 0.65*r)
    {
        capacity = 1.0;
    }
    laneCount = max(2.0, ceil(count / capacity));
    phaseExtent = 3.14159265359;
}
float slotsPerLane = max(1.0, ceil(count / laneCount));
laneCount = ceil(count / slotsPerLane);
float lane = floor(index / slotsPerLane);
float slot = fmod(index, slotsPerLane);
float phase = slot * phaseExtent / slotsPerLane + seedAngle;
if (Mode > 1.5)
{
    phase += fmod(lane, 2.0) * 3.14159265359;
}
float speed = clamp(Speed, 10.0, 5000.0);
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
float drift = clamp(Wind,0.0,2000.0)*sin(Clock*0.24+seedAngle);
pos.xy += float2(drift,drift*0.26);
float verticalSpan = (laneCount-1.0)*spacing;
// 数量超出单层容量时仅增加所需层数；整体上移而不逐鸟钳制，避免底部多层被压成同一高度。
float centerHeight = max(Height, Height*0.35+verticalSpan*0.5+42.0);
pos.z = centerHeight+(lane-(laneCount-1.0)*0.5)*spacing+sin(Clock*0.63+seedAngle)*42.0;
float yaw = atan2(tangent.y,tangent.x);
float sy=sin(yaw*0.5), cy=cos(yaw*0.5);
float sr=sin(bank*0.5), cr=cos(bank*0.5);
Particles.Position = pos;
Particles.Velocity = tangent*speed;
Particles.MeshOrientation = float4(cy*sr,sy*sr,sy*cr,cy*cr);
Particles.Scale = (0.86+frac(seedPhase*4.17)*0.25).xxx;
Particles.Color = float4(1.0,1.0,1.0,1.0);
#endif
