// 平台通用鸟群GPU局部轨迹。输入只包含中立运动参数，无任何项目物种身份。
// 由Monolith导入真实Niagara模块；单粒子就是单只鸟，使用System.Age而非CPU逐鸟Tick。
// 不做权威碰撞：设计师提供安全飞行体积，最低局部高度保证不穿过锚点平面。
// 原Individual随机输入仅保留尺寸差异；位置从ParameterMap读本次一次Burst的Particles.UniqueID。
// User.BirdCount为实际生成数，User.FlockMinimumSpacing为含缩放、振翅WPO与余量的中心距厘米。
// 直接读取已注册参数并保持模块接口/资产身份；作者须刷新同模块调用引脚并重绑原输入。
// 三模式均为左右交替的连续S/伸长八字掠行；共用相位速度/漂移/起伏，避免单向圆周旋转。
// 固定高度层在所有时间保留间距，不增加逐鸟CPU工作或邻域扫描；Velocity是实际位置的解析导数。
#if GPU_SIMULATION
float count = max(1.0, floor(User.BirdCount));
float index = fmod(max(0.0, floor(Particles.UniqueID)), count);
float seedPhase = frac(Individual * 1.6180339887 + Seed * 0.000137);
float seedAngle = frac(Seed * 0.000137) * 6.28318530718;
// 原轨迹半径均值约0.885R；0.9R保留原飞行区域，取消独立半径/角速度导致的追近。
float r = max(Radius, 200.0) * 0.9;
float spacing = max(1.0, User.FlockMinimumSpacing);
// 模式0宽缓滑翔、1长距离S型掠过、2较紧凑八字；参数均为中立路线尺寸，不识别项目鸟种。
float routeWidth = 1.7;
float routeDepth = 0.8;
float routeDistanceBound = 1.10;
float bobAmplitude = 120.0;
if (Mode > 1.5)
{
    routeWidth = 1.6;
    routeDepth = 0.65;
    routeDistanceBound = 1.015;
    bobAmplitude = 60.0;
}
else if (Mode > 0.5)
{
    routeWidth = 1.8;
    routeDepth = 0.55;
    routeDistanceBound = 1.02;
    bobAmplitude = 72.0;
}
// (A*sin(a),B*sin(2a))在半相位内的距离>=c*r*abs(sin(delta))，
// c取低于sqrt(A*A/2-A*A*A*A/(64*B*B))的保守值。共享漂移相减为零。
// 每层仅占半相位且保留一个间隔；异层由固定Z间距隔开，中心交汇时仍不会相撞。
float capacity = max(1.0, floor(3.14159265359 / asin(min(1.0, spacing / (routeDistanceBound*r)))));
if (spacing > routeDistanceBound*r)
{
    // asin夹到1会误算成2；单层几何尺寸不足时，只能给每只鸟独立高度层。
    capacity = 1.0;
}
float laneCount = max(2.0, ceil(count / capacity));
float slotsPerLane = max(1.0, ceil(count / laneCount));
laneCount = ceil(count / slotsPerLane);
float lane = floor(index / slotsPerLane);
float slot = fmod(index, slotsPerLane);
float phase = slot * 3.14159265359 / slotsPerLane + seedAngle + fmod(lane,2.0)*3.14159265359;
float speed = clamp(Speed, 10.0, 5000.0);
// 1.75是路线长度倍率：FlightSpeed仍是厘米/秒的名义速度尺度，转弯平滑变速而非恒角速度圆周。
float angularSpeed = speed / (1.75*r);
float routeClock = Clock*angularSpeed+seedAngle;
float angle = phase+Clock*angularSpeed;
float sx = sin(angle);
float cx = cos(angle);
float3 pos = float3(routeWidth*r*sx,routeDepth*r*sin(2.0*angle),Height);
float3 velocity = float3(routeWidth*r*cx*angularSpeed,2.0*routeDepth*r*cos(2.0*angle)*angularSpeed,0.0);
// 慢漂移让整群经过不同掠行带，不重复描一条固定闭环；所有鸟共用，最小间距不受影响。
float wind = clamp(Wind,0.0,2000.0);
float drift = wind*sin(Clock*0.24+seedAngle);
float driftVelocity = wind*0.24*cos(Clock*0.24+seedAngle);
pos.xy += float2(drift+0.22*r*sin(routeClock*0.17),drift*0.26+0.13*r*sin(routeClock*0.23+0.7));
velocity.xy += float2(driftVelocity+0.22*r*0.17*angularSpeed*cos(routeClock*0.17),driftVelocity*0.26+0.13*r*0.23*angularSpeed*cos(routeClock*0.23+0.7));
float verticalSpan = (laneCount-1.0)*spacing;
// 数量超出单层容量时仅增加所需层数；整体上移而不逐鸟钳制，避免底部多层被压成同一高度。
float centerHeight = max(Height,Height*0.35+verticalSpan*0.5+bobAmplitude);
pos.z = centerHeight+(lane-(laneCount-1.0)*0.5)*spacing+bobAmplitude*(0.75*sin(routeClock*0.31)+0.25*sin(routeClock*0.53+0.9));
velocity.z = bobAmplitude*angularSpeed*(0.75*0.31*cos(routeClock*0.31)+0.25*0.53*cos(routeClock*0.53+0.9));
float3 tangent = normalize(velocity+float3(0.000001,0.0,0.0));
float bank = 0.16*sin(angle);
float yaw = atan2(tangent.y,tangent.x);
float sy=sin(yaw*0.5), cy=cos(yaw*0.5);
float sr=sin(bank*0.5), cr=cos(bank*0.5);
Particles.Position = pos;
Particles.Velocity = velocity;
Particles.MeshOrientation = float4(cy*sr,sy*sr,sy*cr,cy*cr);
Particles.Scale = (0.86+frac(seedPhase*4.17)*0.25).xxx;
Particles.Color = float4(1.0,1.0,1.0,1.0);
#endif
