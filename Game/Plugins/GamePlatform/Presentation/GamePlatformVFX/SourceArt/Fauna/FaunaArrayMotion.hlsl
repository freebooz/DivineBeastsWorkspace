// 世界实例上传的局部厘米位置、四元数和动作状态；固定粒子槽位，无粒子重排/死亡。
#if GPU_SIMULATION
int n=0; Positions.Length(n);
int qn=0; Orientations.Length(qn);
int mn=0; MotionStates.Length(mn);
if (Index>=0 && Index<n && Index<qn && Index<mn)
{
    float3 p; float4 q; float4 m;
    Positions.Get(Index,p);
    Orientations.Get(Index,q);
    MotionStates.Get(Index,m);
    Particles.Position=p;
    Particles.MeshOrientation=q;
    Particles.DynamicMaterialParameter=m;
    Particles.Scale=float3(1.0,1.0,1.0);
    Particles.Color=float4(1.0,1.0,1.0,1.0);
}
else
{
    // 空数组/非法索引必须在Get之前拦截；允许预载和Count=0。
    Particles.Position=float3(0.0,0.0,0.0);
    Particles.Scale=float3(0.0,0.0,0.0);
}
#endif
