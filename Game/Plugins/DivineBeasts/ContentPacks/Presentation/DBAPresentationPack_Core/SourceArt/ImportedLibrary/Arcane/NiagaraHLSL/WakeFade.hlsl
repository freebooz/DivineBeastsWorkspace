float p=saturate(NormalizedAge);
Particles.RibbonWidth=40*pow(1-p,.55);
Particles.Color=float4(1,1,1,pow(1-p,1.5)*.8);
