// 仅外焰Mesh做有界卷曲；Sprite与Opaque核心不位移，核心轮廓保持稳定。
if(IsShell>1.5)return float3(0,0,0);
float3 n=normalize(NormalWS),f=normalize(ForwardWS);
float back=1.0-smoothstep(-.1,.8,dot(n,f));
float wave=sin(UV.y*18.0-Clock*2.6+sin(UV.x*6.2831853+Clock*.5)*1.4);
float curl=pow(.5+.5*wave,2.0);
return n*(1.5+4.5*curl)*(.4+.6*back)-f*(back*curl*6.0);
