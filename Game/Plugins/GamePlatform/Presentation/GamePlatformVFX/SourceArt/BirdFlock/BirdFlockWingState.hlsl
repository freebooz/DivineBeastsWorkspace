// Niagara每只鸟独立相位与扑翼/滑翔标量；材质DynamicParameter通道需要真实Editor绑定。
// 顶点RGB继续保留羽色、Alpha继续保留翼尖变形权重，不重写原顶点色。
#if GPU_SIMULATION
float seedPhase=frac(Individual*1.61803398875 + Seed*0.000177);
float wingHz = max(0.1,BaseFlapHz)*(0.83 + 0.34*frac(seedPhase*8.11));
float wingAmplitude = saturate(BaseAmplitude)*(0.85 + 0.15*frac(seedPhase*2.71));
float glide=saturate(GlideBlend);
float phase = frac(Clock*wingHz+seedPhase);
Particles.DynamicMaterialParameter = float4(phase,wingHz,glide,wingAmplitude);
#endif
