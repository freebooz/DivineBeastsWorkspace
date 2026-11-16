// 第三层唯一原稿适配：只重排UV，保留源RGB与线性Alpha；不做Gamma、调色、锐化或业务轮询。
// Layer: 0边框/暗底，1生命填充，2能量填充。肖像独立使用登录角色纹理，不从此原稿采样。
struct PlayerFrameMapping
{
    float Remap(float X, float A, float B, float C, float D)
    { return lerp(C,D,(X-A)/(B-A)); }
    float SourceX(float X)
    {
        if(X<24) return Remap(X,19.5,24,580,604);
        if(X<88) return Remap(X,24,88,604,896);
        if(X<90) return Remap(X,88,90,896,916);
        if(X<203) return Remap(X,90,203,916,1282);
        return Remap(X,203,212.5,1282,1328);
    }
    float PortraitY(float Y)
    {
        if(Y<16) return Remap(Y,11.5,16,202,244);
        if(Y<80) return Remap(Y,16,80,244,536);
        return Remap(Y,80,88.5,536,576);
    }
    float BarY(float Y)
    {
        if(Y<26) return Remap(Y,11.5,26,250,308);
        if(Y<43) return Remap(Y,26,43,308,378);
        if(Y<45) return Remap(Y,43,45,378,384);
        if(Y<57) return Remap(Y,45,57,384,435);
        if(Y<58) return Remap(Y,57,58,435,442);
        if(Y<70) return Remap(Y,58,70,442,491);
        if(Y<75) return Remap(Y,70,75,491,514);
        return Remap(Y,75,88.5,514,576);
    }
    float Capsule(float2 P, float2 Size)
    {
        float Radius=Size.y*.5;
        float2 Q=abs(P-Size*.5)-float2(Size.x*.5-Radius,0);
        return length(max(Q,0))-Radius;
    }
};
PlayerFrameMapping Map;
float2 SourceSize=float2(1916,821);
float2 S;
float Alpha=1;
if(Layer>.5)
{
    S=float2(916+UV.x*393,(Layer<1.5?388:446)+UV.y*42);
    // 数值位置使用同一原稿的无字晶体窄段；实际数值由ResourceValueText事件更新。
    if(S.x>1035&&S.x<1187) S.x=1018+(S.x-1035)*.10;
    float D=Map.Capsule(UV*float2(119,12),float2(119,12));
    Alpha=saturate(.5-D/max(fwidth(D),.5));
}
else
{
    float2 P=UV*float2(193,77)+float2(19.5,11.5);
    S=float2(Map.SourceX(P.x),lerp(Map.PortraitY(P.y),Map.BarY(P.y),smoothstep(88,92,P.x)));
    Alpha=Texture2DSample(Matte,MatteSampler,S/SourceSize).a;
    // 清除Alpha=1的孤立残留；内部深色区域不以亮度抠图。
    Alpha*=step(1.5/255,Alpha);
    float PortraitD=length(P-float2(56,48))-31.8;
    Alpha*=saturate(.5+PortraitD/max(fwidth(PortraitD),.5));
    // 保留姓名牌右端纹样，只用原稿无字暗底覆盖“青岚”示意字。
    if(S.x>943&&S.x<1043&&S.y>317&&S.y<369) S.x=1167+(S.x-943)*.32;
    float D=min(Map.Capsule(P-float2(90,45),float2(119,12)),Map.Capsule(P-float2(90,58),float2(119,12)));
    if(D<.4)
    {
        S=float2(1182,331);
        Alpha=1;
    }
}
float3 RGB=Texture2DSample(Source,SourceSampler,S/SourceSize).rgb;
return float4(RGB,Alpha);
