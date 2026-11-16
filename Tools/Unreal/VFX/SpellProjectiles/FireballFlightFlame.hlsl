// 已审核sRGB源纹理由Color sampler解码一次；Alpha只送Opacity一次。
float age=lerp(Clock,D.x,saturate(UseNiagaraAge));
float4 t;
if(IsShell>1.5){
    float angle=age*.14;float2 p=UV-.5;
    float2 q=.5+float2(p.x*cos(angle)-p.y*sin(angle),p.x*sin(angle)+p.y*cos(angle));
    t=Texture2DSample(Flame,FlameSampler,q);
}else if(IsShell>.5){
    // 球面经线方向对应飞行轴；火舌沿表面向尾端卷动。
    float angle=(UV.x+age*.09)*6.2831853;
    float radius=.32+.13*sin(UV.y*3.14159265);
    float2 q=.5+radius*float2(cos(angle),sin(angle));
    q+=.018*float2(sin(UV.y*12-age*1.4),cos(UV.y*9+age*1.1));
    t=Texture2DSample(Flame,FlameSampler,q);
    t.a*=.60+.40*sin(UV.y*3.14159265);
}else{
    // 两个半周期错开的样本在各自回绕点权重为零，不用0-1裁边抹掉长尾。
    float u0=frac(UV.x+age*.55),u1=frac(UV.x+age*.55+.5);
    float w0=pow(sin(3.14159265*u0),2),w1=1-w0;
    float y=clamp(UV.y+.022*sin(UV.x*12-age*2.1),.001,.999);
    float4 a=Texture2DSample(Flame,FlameSampler,float2(u0,y));
    float4 b=Texture2DSample(Flame,FlameSampler,float2(u1,y));
    float alpha=a.a*w0+b.a*w1;
    t=float4((a.rgb*a.a*w0+b.rgb*b.a*w1)/max(alpha,.0001),alpha);
    // 新端在球体后半部内软融入，避免Ribbon末端的竖直截面露出球体轮廓。
    // 尾根仅在球体内部约5%尾长淡入，外露衔接段保留饱满密度；末端14%连续透明。
    t.a*=smoothstep(0.0,.14,UV.x)*(1.0-smoothstep(.95,1.0,UV.x));
    // 老粒子向橙红冷却，主体保留原稿金黄的宽窄交织。
    t.rgb*=lerp(float3(1,1,1),float3(1,.12,.018),smoothstep(.12,.85,saturate(D.y))*saturate(CoolingStrength));
}
return float4(t.rgb*PC*max(0,Emission),saturate(t.a*PA*OpacityScale));
