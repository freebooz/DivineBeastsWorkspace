// 真正不透明球面：纹理只改变颜色与亮度，不改变轮廓或制造透明孔洞。
float age=lerp(Clock,D.x,saturate(UseNiagaraAge));
float2 ua=frac(UV+float2(.06,.03)*age);
float2 ub=frac(UV*5.0+float2(-.035,.065)*age);
float3 a=Texture2DSample(FlowAtlas,FlowAtlasSampler,ua).rgb;
float3 b=Texture2DSample(FlowAtlas,FlowAtlasSampler,ub).rgb;
float3 c=Texture2DSample(FlowAtlas,FlowAtlasSampler,frac(UV*11.0+float2(.025,-.04)*age)).rgb;
float3 n=normalize(NormalWS),v=normalize(CameraWS-PixelWS);
float front=pow(saturate(dot(n,normalize(ForwardWS))),3.0);
float vein=saturate(.8*b.b+.65*c.b+.18*a.r);
float heat=saturate(vein+.8*front+.5*pow(saturate(dot(n,v)),3.0));
float3 base=Gold*lerp(.20,.52,a.g);
float3 fire=lerp(base,WarmWhite,smoothstep(.20,.86,heat));
// 发光球的中心比视线切边更厚、更亮；保持清晰球体层次。
fire*=.65+.35*sqrt(saturate(dot(n,v)));
float edge=pow(1.0-saturate(dot(n,v)),3.0);
fire+=Gold*edge*.12;
return fire*max(0,FireIntensity);
