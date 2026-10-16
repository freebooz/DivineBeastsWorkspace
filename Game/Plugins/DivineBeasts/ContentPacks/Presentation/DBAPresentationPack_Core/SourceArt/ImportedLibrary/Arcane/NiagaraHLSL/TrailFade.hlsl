float p=saturate(NormalizedAge);
Particles.RibbonWidth=11*pow(1-p,.65);
Particles.Color=float4(lerp(float3(.6,.58,1),float3(.30,.03,.65),p),pow(1-p,1.5));
