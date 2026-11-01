// SSLM当前标准火球的纯表现适配；由唯一原生模块读取，不拥有权威弹道或结算。
// Phase：0前摇、1飞行、2命中；Role：0主体、1外焰/余烬、2拖尾/碎焰、3空气扰动。
// Age为粒子秒数，Elapsed仅为前摇服务器窗口的迟到偏移，Quality为Niagara原生质量档。
float seconds=max(0.0,Age)+(Phase==0 ? max(0.0,Elapsed) : 0.0);
float alpha=saturate(InColor.a);
float3 position=InPosition;
float3 scale=InScale;
float2 sprite=InSpriteSize;
float rotation=InRotation;
// 不再把旧Layer值写入新材质Dissolve通道；粒子颜色只保留亮度与生命周期。
float strength=max(InColor.r,max(InColor.g,InColor.b));
float3 color=float3(strength,strength,strength);
if(Phase==1 && Role==0){
    // Engine Sphere全尺寸100cm；InScale由唯一视觉合同写入，不在模块复制尺寸真源。
    position=float3(0,0,0);
    scale=InScale;
    color=float3(1,1,1);
}
if(Phase==1 && Role==1){
    // 外焰按同一球头尺寸缩放，保持圆形包络。
    float surfaceScale=InScale.x/.5;
    float phase=float(ParticleIndex)*2.399963+seconds*.55;
    float z=2.0*frac(float(ParticleIndex)*.618034)-1.0;
    float radial=sqrt(max(0.0,1.0-z*z));
    position=float3(-5+radial*cos(phase)*11,radial*sin(phase)*11,z*12)*surfaceScale;
    sprite=float2(10,14)*surfaceScale;
    color=float3(1,1,1);
}
if(Phase==1 && Role==2){
    // 世界空间历史点保留；较缓的Alpha使延长后的整条尾焰可读，宽度仍由既有TrailFade负责。
    alpha=pow(alpha,.4);
    color=float3(1,1,1);
}
if(Phase==2 && Role==0){
    position=float3(0,0,0);
    sprite=float2(180,180);
    rotation=0;
    color=float3(1,1,1);
    alpha=saturate((.66-Age)/.12);
}
if(Phase==2 && Role==1){
    sprite=float2(4,18);
    color=float3(1,1,1);
    alpha*=.7;
}
if(Role==3){
    // 局部有界热扰动，四档保持主体不变。Low透明度归零并由Renderer平台合同禁用。
    float ripple=Quality<=0 ? 0.0 : (Quality==1 ? .5 : (Quality==2 ? 1.0 : 1.5));
    color=float3(1,1,1);
    if(Phase==1){
        position=float3(0,0,0);
        float extent=InScale.x*(Quality==1 ? 1.05 : (Quality==2 ? 1.10 : 1.15));
        scale=float3(extent,extent,extent);
    }
    if(Phase==2){
        position=float3(0,0,0);
        float extent=(.4+Age*1.8)*(Quality==1 ? 1.05 : (Quality==2 ? 1.10 : 1.15));
        scale=float3(extent,extent,extent);
        alpha=saturate((.70-Age)/.20);
    }
    alpha*=ripple;
}
Particles.Position=position;
Particles.Scale=scale;
Particles.SpriteSize=sprite;
Particles.SpriteRotation=rotation;
Particles.Color=float4(color,alpha);
Particles.DynamicMaterialParameter=float4(seconds,0,0,1);
Particles.SubImageIndex=0.0;
