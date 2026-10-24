"""植物风离线作者/审计共有配方，不是运行时风状态真源。

运行时名称由GamePlatformSurface C++合同拥有；本文件固定待原生审核的作者输入、
材质HLSL和资产路径。导入无UE依赖、无磁盘或编辑器副作用。
"""
VERSION = '1'
PLATFORM_ROOT = '/GamePlatformSurface/MaterialFunctions/Vegetation'
ST9_ROOT = '/DBAFoliagePack_Core/Foliage/Materials/Wind'
GLOBAL_FUNCTION = PLATFORM_ROOT + '/MF_GP_GlobalWindState'
GENERIC_FUNCTION = PLATFORM_ROOT + '/MF_GP_DirectionalVegetationWind'
ST9_FUNCTION = ST9_ROOT + '/MF_DBA_Foliage_ST9Wind'
ST9_MASTER = ST9_ROOT + '/M_DBA_Foliage_ST9_Master'
REVIEW_MAP = '/DBAWorldPack_Village/Development/Wind/Maps/L_DBA_VegetationWind_Review'
WIND_VECTORS = ('GlobalWindFrom', 'GlobalWindTo', 'GlobalWindTiming')
WIND_SCALAR = 'GlobalWindSeed'

# 全部计算在GPU；Yaw沿最短弧，频率线性过渡的积分与Weather快照相同。
# 空间阵风沿统一主方向传播，种子只平移阵风，绝不随机主风向。
GLOBAL_STATE_HLSL = '''
float t=max(0.0,Time+Timing.w-Timing.x);
float T=max(Timing.y,0.0);
float a=T>0.0?saturate(t/T):1.0;
float delta=fmod(To.x-From.x+540.0,360.0)-180.0;
float yaw=radians(From.x+delta*a);
float2 direction=float2(cos(yaw),sin(yaw));
float integral=T<=0.0?To.w*t:(t<T?
    From.w*t+(To.w-From.w)*t*t/(2.0*T):
    (From.w+To.w)*T*0.5+To.w*(t-T));
float phase=frac(Timing.z+integral);
float wave=sin(6.28318530718*(phase-dot(PositionWS.xy,direction)/1200.0+Seed/65536.0));
float strength=lerp(From.y,To.y,a)*(1.0+0.35*lerp(From.z,To.z,a)*wave);
return float4(direction,max(0.0,strength),phase);
'''

# 此高度遮罩仅供根在模型底部的普通原型植物；绝不套用到下垂柳叶。
GENERIC_WPO_HLSL = '''
float height=saturate((PositionLS.z-RootZcm)/max(HeightCm,0.01));
float anchored=height*height*saturate(Weight);
float main=0.8+0.2*sin(Phase*6.28318530718);
float fade=MaxDistanceCm>0.0?1.0-smoothstep(MaxDistanceCm*0.8,MaxDistanceCm,DistanceCm):1.0;
return DirectionWS*max(0.0,AmplitudeCm)*anchored*Strength*main*fade;
'''

# 不处理PackedPosition/PackedDirection，它们必须保持原始位模式；只保护数值权重。
FINITE_WEIGHT_HLSL = 'return isfinite(Weight)?Weight:0.0;'
BOUNDED_ST9_WPO_HLSL = '''
if(!all(isfinite(Offset))) return float3(0,0,0);
float3 value=Offset*Strength;
return value*min(1.0,max(MaxCm,0.0)/max(length(value),0.0001));
'''

# 由原生ST9函数公开默认值缩小到克制的美术审核起点；不是恢复出的源风曲线。
TREE_RESPONSE = {
    'SharedBend': 12., 'SharedOscillation': 8., 'SharedSpeed': .05,
    'SharedTurbulence': .15, 'SharedFlexibility': .01, 'SharedIndependence': .001,
    'SharedStartHeight': .1, 'Branch1Bend': 25., 'Branch1Oscillation': 12.,
    'Branch1Speed': .1, 'Branch1Turbulence': .25, 'Branch1Flexibility': .05,
    'Branch1Independence': .75, 'Branch1StretchLimit': 1000.,
    'Branch2Bend': 15., 'Branch2Oscillation': 8., 'Branch2Speed': .1,
    'Branch2Turbulence': .25, 'Branch2Flexibility': .05,
    'Branch2Independence': .75, 'Branch2StretchLimit': 1000.,
    'RipplePlanar': 2.5, 'RippleDirectional': 2., 'RippleSpeed': .25,
    'RippleFlexibility': .1, 'RippleIndependence': .1, 'MaxWindDisplacementCm': 30.}
FERN_RESPONSE = dict(TREE_RESPONSE, SharedBend=3., SharedOscillation=2.,
                     Branch1Bend=6., Branch1Oscillation=3., Branch2Bend=3.,
                     Branch2Oscillation=2., RipplePlanar=.8, RippleDirectional=.6,
                     MaxWindDisplacementCm=6.)


def is_plant_mesh(path):
    """显式植物库/既有原型树范围；茅屋、Water和任意名为Grass的材质均不纳入。"""
    return (path.startswith('/DBAFoliagePack_Core/Foliage/') or
            path.startswith('/DBAWorldPack_Village/PCG/HillsideForest/SM_DBA_Review_WideCrown'))
