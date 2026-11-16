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
float distanceAge=0.0;
float distanceFraction=0.0;
if(Phase==1 && (Role==2 || Role==4)){
    // 首次更新从真实出生点量距，此后积分Owner实际位移；不依赖某一出生帧是否恰有网络更新。
    float previous=Particles.DynamicMaterialParameter.z;
    distanceAge=previous<0.0 ? length(Engine.Owner.Position-InPosition) : previous+length(Engine.Owner.Velocity)*Engine.DeltaTime;
    float budget=DBA_FB_TRAIL_LENGTH*clamp(User.TrailLengthScale,.05,1.0);
    distanceFraction=saturate(distanceAge/max(budget,1.0));
    float remaining=saturate(1.0-distanceFraction);
    // 空间距离负责整条尾迹渐隐；年龄只在最后35%寿命收尾，避免加长后尚未到尾端就变暗。
    float ageFade=saturate((1.0-Age/DBA_FB_TRAIL_MAX_LIFE)/.35);
    alpha=pow(remaining,Role==2 ? .4 : .55)*pow(ageFade,1.2);
    // 球后约15%尾长保持饱满截面，其后连续收窄；不让尾根一离开球体就塌细。
    float taper=saturate((distanceFraction-.15)/.85);
    Particles.RibbonWidth=(Role==2 ? DBA_FB_TRAIL_WIDTH : DBA_FB_FILAMENT_WIDTH)*pow(1.0-taper,.75)*pow(ageFade,.15);
    if(distanceAge>budget)DataInstance.Alive=false;
}
if(Phase==1 && Role==0){
    // Engine Sphere全尺寸100cm；InScale由唯一视觉合同写入，不在模块复制尺寸真源。
    position=float3(0,0,0);
    scale=InScale;
    color=float3(1,1,1);
    Particles.MeshOrientation=float4(0,0,0,1);
}
if(Phase==1 && Role==1){
    // 外焰按同一球头尺寸缩放，保持圆形包络。
    position=float3(0,0,0);
    scale=InScale;
    sprite=float2(120,120)*InScale.x;
    rotation=seconds*.12;
    Particles.MeshOrientation=float4(sin(seconds*.75),0,0,cos(seconds*.75));
    color=float3(1,1,1);
}
if(Phase==1 && Role==2){
    // 世界空间位置不回写；距离和年龄共用上方唯一宽度/消散实现。
    color=float3(1,1,1);
}
if(Phase==1 && Role==4){
    // 三条金色丝带复用世界历史位置；出生时已分配稳定int Ribbon ID。
    color=float3(1.2,1.2,1.0);
}
if(Phase==1 && Role==5){
    position=float3(0,0,0);
    sprite=float2(100,100)*InScale.x;
    // 此共享柔光母材质无发光标量，仅火球自有粒子颜色消费相同亮度合同。
    color=float3(3.5,1.8,.12)*DBA_FB_EMISSION_SCALE;
    alpha=.13;
    Particles.LightRadius=200.0;
    Particles.LightEnabled=Quality>=2;
}
if(Phase==1 && Role==6){
    // 世界空间火星速度由原生Solver推进，不能重新跟随弹体平移。
    sprite=float2(3,9);
    color=float3(2.5,.8,.08);
    alpha=saturate(1.0-Age/.35);
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
Particles.DynamicMaterialParameter=float4(seconds,distanceFraction,distanceAge,1);
Particles.SubImageIndex=0.0;
