// Niagara Spline DI须按Particles.RouteProgress返回局部SplinePosition和SplineTangent。
// 此模块仅合成局部群飞/姿态，Spline样条数据接口由Editor真实绑定到当前Niagara附着父组件。
// 输入：Clock(s)、Individual(0..1)、RoutePosition(cm)、RouteTangent、Radius(cm)、Wind(cm)、Speed(cm/s)。
#if GPU_SIMULATION
float phase = frac(Individual * 1.618033989 + Seed * 0.000139) * 6.28318530718;
float3 forward = normalize(RouteTangent);
if (length(RouteTangent) < 0.0001) forward = float3(1.0,0.0,0.0);
float3 right = cross(float3(0.0,0.0,1.0), forward);
if (length(right) < 0.0001) right = cross(float3(0.0,1.0,0.0), forward);
right = normalize(right);
float3 up = normalize(cross(forward,right));
float localPhase = phase + Clock * (0.7 + 0.4*frac(phase*0.31));
float spread = max(0.0,Radius) * (0.12 + 0.17*frac(phase * 0.7));
float sway = sin(localPhase)*spread;
float bob = cos(localPhase*0.78)*spread*0.34;
float3 windOffset = right*clamp(Wind,0,2000)*0.3*sin(Clock*0.24+phase);
float3 position = RoutePosition + right*sway + up*bob + windOffset;
float3 velocity = forward*clamp(Speed,10.0,5000.0);
Particles.Position = position;
Particles.Velocity = velocity;
float yaw = atan2(forward.y,forward.x);
float roll = clamp(sway/max(200.0,Radius),-0.18,0.18);
float sy=sin(yaw*0.5),cy=cos(yaw*0.5),sr=sin(roll*0.5),cr=cos(roll*0.5);
Particles.MeshOrientation = float4(cy*sr,sy*sr,sy*cr,cy*cr);
#endif
