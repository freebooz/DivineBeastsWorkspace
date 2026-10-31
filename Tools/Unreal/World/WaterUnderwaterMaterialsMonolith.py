"""在原生Water后处理图中扩展有界焦散，保持单一WaterSubsystem后处理链。"""
import importlib.util
import json
import math
from pathlib import Path
import struct
import zlib
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
OUT = ROOT/'Saved/Validation/VillageWaterSystems20261031'
MASTER = '/GamePlatformSurface/Materials/Water/M_GP_Underwater'
BASE = '/Water/Materials/PostProcessing/M_UnderWater_PostProcess_Volume'

CAUSTICS = r'''
if (Quality <= 0.0) return Color;
float2 uv = (WorldPos.xy - MaskMin.xy) * MaskScale.xy;
float inBounds = step(0.0, uv.x)*step(uv.x,1.0)*step(0.0,uv.y)*step(uv.y,1.0);
float lake = Texture2DSample(LakeMask,LakeMaskSampler,saturate(uv)).r * inBounds;
float depth = max(WaterZ - WorldPos.z,0.0);
float receiver = smoothstep(8.0,45.0,depth) * exp2(-depth/700.0);
float3 g = cross(ddx(clamp(WorldPos,-1.e6,1.e6)),ddy(clamp(WorldPos,-1.e6,1.e6)));
float3 n = g * rsqrt(max(dot(g,g),1.e-8));
receiver *= pow(saturate(abs(n.z)),3.0);
float2 p = WorldPos.xy * 0.006;
float c1 = Texture2DSample(CausticTexture,CausticTextureSampler,p+Time*float2(0.018,0.012)).r;
float c2 = Texture2DSample(CausticTexture,CausticTextureSampler,p.yx*0.81-Time*float2(0.014,0.009)).r;
float light = pow(saturate(0.5*(c1+c2)),2.0);
float distanceFade = exp2(-length(WorldPos-CameraPos)/3000.0);
return Color * (1.0 + light*receiver*lake*distanceFade*Strength*Quality);
'''


def common():
    spec = importlib.util.spec_from_file_location('water_common',str(ROOT/'Tools/Unreal/World/VillageWaterSystemsMonolith.py'))
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    return module


def guard_clean_targets(paths):
    """修改前拒绝目标未保存内容；不把作者metadata作为覆盖人工编辑的许可。"""
    targets=set(paths)
    dirty=[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name() in targets]
    if dirty:
        raise RuntimeError('目标资产含未保存修改，拒绝覆盖：'+', '.join(dirty))


def ensure_master():
    guard_clean_targets([MASTER])
    assets = u.EditorAssetLibrary; ed = u.MaterialEditingLibrary
    if assets.does_asset_exist(MASTER):
        obj = u.load_asset(MASTER)
        assert assets.get_metadata_tag(obj,'SSLM.WaterAuthor') == 'VillageWaterSystems20261031', '已有非本作者母材质，不覆盖'
        if assets.get_metadata_tag(obj,'SSLM.WaterGraphState') == 'Complete':
            return obj
        assert not assets.find_package_referencers_for_asset(MASTER,True), '未完成母材质已有外部引用，停止恢复'
        common().backup(MASTER)
        assert assets.delete_asset(MASTER), '无法撤销本轮未绑定的半成品'
    assert not [p for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages() if p.get_name()!=MASTER]
    m = assets.duplicate_asset(BASE,MASTER)
    assert isinstance(m,u.Material)
    assets.set_metadata_tag(m,'SSLM.WaterAuthor','VillageWaterSystems20261031')
    assets.set_metadata_tag(m,'SSLM.WaterGraphState','Partial')
    source = ed.get_material_property_input_node(m,u.MaterialProperty.MP_EMISSIVE_COLOR)
    source_output = ed.get_material_property_input_node_output_name(m,u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert source, '原生水下图没有有效输出，不能重建掩盖错误'
    def expression(cls,x=0,y=0): return ed.create_material_expression(m,cls,x,y)
    custom = expression(u.MaterialExpressionCustom,2400,0)
    names = ['Color','WorldPos','CameraPos','Time','LakeMask','MaskMin','MaskScale','WaterZ','CausticTexture','Strength','Quality']
    inputs=[]
    for name in names:
        item=u.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    custom.set_editor_property('code',CAUSTICS)
    custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property('description','有界水下焦散；保留原生散射/水线，只照亮水下接收面')
    assert ed.connect_material_expressions(source,source_output,custom,'Color')
    for name,cls in [('WorldPos',u.MaterialExpressionWorldPosition),('CameraPos',u.MaterialExpressionCameraPositionWS),('Time',u.MaterialExpressionTime)]:
        assert ed.connect_material_expressions(expression(cls), '', custom,name)
    def vector(name,value):
        e=expression(u.MaterialExpressionVectorParameter);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',u.LinearColor(*value));return e
    def scalar(name,value):
        e=expression(u.MaterialExpressionScalarParameter);e.set_editor_property('parameter_name',name);e.set_editor_property('default_value',value);return e
    for pin,node in [('MaskMin',vector('LakeMaskMin',(0,0,0,0))),('MaskScale',vector('LakeMaskScale',(1,1,0,0))),('WaterZ',scalar('LakeSurfaceZ',0)),('Strength',scalar('CausticsStrength',0.85))]:
        assert ed.connect_material_expressions(node,'',custom,pin)
    for pin,name,path in [('LakeMask','LakeMask','/Engine/EngineResources/WhiteSquareTexture'),('CausticTexture','CausticTexture','/GamePlatformSurface/Textures/Water/T_GP_Water_Caustics_Source')]:
        node=expression(u.MaterialExpressionTextureObjectParameter);node.set_editor_property('parameter_name',name);node.set_editor_property('texture',u.load_asset(path));assert node.get_editor_property('texture')
        assert ed.connect_material_expressions(node,'',custom,pin)
    quality=expression(u.MaterialExpressionQualitySwitch)
    for pin,value in [('Default',1.0),('Low',0.0),('Medium',0.35),('High',0.7),('Epic',1.0)]:
        node=expression(u.MaterialExpressionConstant);node.set_editor_property('r',value)
        assert ed.connect_material_expressions(node,'',quality,pin), pin
    assert ed.connect_material_expressions(quality,'',custom,'Quality')
    assert ed.connect_material_property(custom,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    assets.set_metadata_tag(m,'SSLM.WaterAuthor','VillageWaterSystems20261031')
    assets.set_metadata_tag(m,'SSLM.SourceMaterial',BASE)
    assets.set_metadata_tag(m,'SSLM.WaterGraphState','Complete')
    ed.recompile_material(m)
    assert assets.save_loaded_asset(m,False)
    print('UNDERWATER_MASTER_SAVED',MASTER)
    return m


def mask_png(points,minx,miny,sizex,sizey,path,n=1024):
    """湖岸几何生成线性灰度遮罩，不读取或调色任何现有图稿。"""
    # 每行四个垂直子样本，水平按精确像素覆盖积分，减少水岸锯齿。
    raw=bytearray()
    for row in range(n):
        coverage=[0.0]*n
        for sy in range(4):
            y=miny+(row+(sy+.5)/4)*sizey/n
            xs=sorted(a[0]+(b[0]-a[0])*(y-a[1])/(b[1]-a[1])
                for a,b in zip(points,points[1:]+points[:1]) if (a[1]>y)!=(b[1]>y))
            assert len(xs)%2==0
            for left,right in zip(xs[::2],xs[1::2]):
                left=(left-minx)*n/sizex;right=(right-minx)*n/sizex
                for x in range(max(0,math.floor(left)),min(n,math.ceil(right))):
                    coverage[x]+=max(0,min(x+1,right)-max(x,left))*.25
        raw.append(0);raw.extend(min(255,max(0,round(v*255))) for v in coverage)
    def chunk(name,data): return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',n,n,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(bytes(raw),9))+chunk(b'IEND',b''))


def author_instance():
    helper=common();world,actors=helper.world_actors();ed=u.MaterialEditingLibrary;assets=u.EditorAssetLibrary
    village=world.get_path_name().split('.')[0]==helper.VILLAGE
    root='/DBAWorldPack_Village/Water' if village else '/Game/Development/Water'
    name='T_DBA_Village_LakeMask' if village else 'T_GP_ReviewLakeMask'
    material_path=root+'/Materials/'+('MI_DBA_Village_Underwater' if village else 'MI_GP_ReviewUnderwater')
    texture_path=root+'/Textures/'+name
    guard_clean_targets([MASTER,material_path,texture_path])
    # 来源metadata不代表本次未被人工编辑；先保护脏包，再备份既有真实资产。
    for path in (material_path,texture_path):
        if assets.does_asset_exist(path): helper.backup(path)
    master=ensure_master()
    lake=next(a for a in actors if a.get_class().get_path_name()=='/Script/Water.WaterBodyLake')
    spline=lake.get_water_body_component().get_water_spline()
    points=[spline.get_location_at_distance_along_spline(spline.get_spline_length()*i/1024,u.SplineCoordinateSpace.WORLD).to_tuple() for i in range(1024)]
    minx=min(p[0] for p in points)-100;miny=min(p[1] for p in points)-100
    sizex=max(p[0] for p in points)+100-minx;sizey=max(p[1] for p in points)+100-miny
    source=(ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Water' if village else ROOT/'Game/SourceArt/Water')/(name+'.png')
    if assets.does_asset_exist(texture_path):
        texture=u.load_asset(texture_path)
        assert assets.get_metadata_tag(texture,'SSLM.WaterAuthor')=='VillageWaterSystems20261031','已有非本轮纹理，不覆盖'
    else:
        assert not source.exists(), '已有原稿，不覆盖'
        mask_png(points,minx,miny,sizex,sizey,source)
        task=u.AssetImportTask();task.set_editor_property('filename',str(source));task.set_editor_property('destination_path',root+'/Textures');task.set_editor_property('destination_name',name);task.set_editor_property('automated',True);task.set_editor_property('save',False)
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture=u.load_asset(texture_path)
        assets.set_metadata_tag(texture,'SSLM.WaterAuthor','VillageWaterSystems20261031')
    assert isinstance(texture,u.Texture2D)
    texture.set_editor_property('srgb',False);texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_GRAYSCALE)
    texture.set_editor_property('address_x',u.TextureAddress.TA_CLAMP);texture.set_editor_property('address_y',u.TextureAddress.TA_CLAMP)
    assert assets.save_loaded_asset(texture,False)
    if assets.does_asset_exist(material_path):
        mi=u.load_asset(material_path)
        assert assets.get_metadata_tag(mi,'SSLM.WaterAuthor')=='VillageWaterSystems20261031','已有非本轮实例，不覆盖'
    else:
        mi=u.AssetToolsHelpers.get_asset_tools().create_asset(material_path.rsplit('/',1)[1],root+'/Materials',u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
        assets.set_metadata_tag(mi,'SSLM.WaterAuthor','VillageWaterSystems20261031')
    ed.set_material_instance_parent(mi,master)
    # 锁定UE5.8的三个Set...ParameterValue实现固定返回false；按真实参数回读验证，不把返回值当成功证据。
    ed.set_material_instance_texture_parameter_value(mi,'LakeMask',texture)
    assert ed.get_material_instance_texture_parameter_value(mi,'LakeMask')==texture
    for parameter,value in [('LakeMaskMin',u.LinearColor(minx,miny,0,0)),('LakeMaskScale',u.LinearColor(1/sizex,1/sizey,0,0))]:
        ed.set_material_instance_vector_parameter_value(mi,parameter,value)
        got=ed.get_material_instance_vector_parameter_value(mi,parameter)
        assert max(abs(a-b) for a,b in zip((got.r,got.g,got.b,got.a),(value.r,value.g,value.b,value.a)))<1.e-4
    ed.set_material_instance_scalar_parameter_value(mi,'LakeSurfaceZ',points[0][2])
    assert abs(ed.get_material_instance_scalar_parameter_value(mi,'LakeSurfaceZ')-points[0][2])<.01
    ed.update_material_instance(mi);assert assets.save_loaded_asset(mi,False)
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/(('Village' if village else 'Review')+'.Underwater.json')).write_text(json.dumps({'material':material_path,'master':MASTER,'source':BASE,'mask':texture_path,'mask_min':[minx,miny],'mask_size':[sizex,sizey],'surface_z':points[0][2],'quality':{'Low':0,'Medium':.35,'High':.7,'Epic':1}},ensure_ascii=False,indent=2),encoding='utf-8')
    print('UNDERWATER_INSTANCE_SAVED',material_path)
    return material_path
