// 仅粒子出生时生成局部截面偏移，再保留世界空间历史；不拥有弹体或命中。
int strand=int(fmod(float(Index),float(FIREBALL_FILAMENT_COUNT)));
float phase=Clock*FIREBALL_FILAMENT_PHASE+float(strand)*(6.2831853/float(FIREBALL_FILAMENT_COUNT));
float3 radial=AxisY*cos(phase)+AxisZ*sin(phase);
Particles.Position=InPosition+radial*RadiusCm;
Particles.RibbonLinkOrder=floor(float(Index)/float(FIREBALL_FILAMENT_COUNT));
Particles.RibbonID.Index=strand;
Particles.RibbonID.AcquireTag=0;
Particles.RibbonWidth=WidthCm;
Particles.Color=float4(1,1,1,1);
Particles.Velocity=Mode==1 ? radial*90.0-normalize(cross(AxisY,AxisZ))*70.0 : float3(0,0,0);
