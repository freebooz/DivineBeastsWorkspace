"""项目层原生作者：既有PCG加密、真实水陆交界湿泥。只改装饰和地形材质，不改高度。

begin/survey/author/audit/camera由Monolith运行；plan只离线计算数值遮罩PNG，绝不伪造UE资产。
复用ShoreMud PBR、现有Bridge雪/路链及平台PCG；无新播放器、运行轮询或碰撞对象。
"""
import json,math,re,hashlib,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/VillageFoliageShore20261024'
ART=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/Shore'
MATERIAL='/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
BASE='/DBAWorldPack_Village/Terrain/Shore'
MASK=BASE+'/T_DBA_Village_ShoreWetnessMask'
GRAPH='/DBAWorldPack_Village/PCG/Ferns/PCG_DBA_Village_Ferns'
STEP=150.0
WIDTH=600.0


def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')


def context():
    import unreal as u
    assert u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name().split('.')[0]=='/DBAWorldPack_Village/Maps/L_Village_Start'
    f={'__file__':str(ROOT/'Tools/Unreal/PCG/AuthorVillageFerns.py')}
    exec(compile(Path(f['__file__']).read_text(encoding='utf-8'),f['__file__'],'exec'),f)
    return u,f


def begin():
    u,f=context()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'先保护人工地图修改'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(),'先保护人工资产修改'
    assert not (OUT/'Before.json').exists(),'禁止覆盖本次首次备份'
    OUT.mkdir(parents=True,exist_ok=True)
    files=['Maps/L_Village_Start.umap','PCG/Ferns/PCG_DBA_Village_Ferns.uasset','Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset']
    content=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content'
    for p in files:shutil.copy2(content/p,OUT/Path(p).name)
    actor=next(a for a in f['ACTORS'].get_all_level_actors() if a.get_actor_label()=='Village_Ferns_PCG')
    director=next(a for a in f['ACTORS'].get_all_level_actors() if isinstance(a,u.GamePlatformPCGWorldDirector))
    masks,error=director.collect_spatial_masks();assert not error
    m=u.load_asset(MATERIAL);lib=u.MaterialEditingLibrary;nodes={n.get_name():n for n in lib.get_material_expressions(m)}
    targets=['MaterialExpressionLinearInterpolate_1','MaterialExpressionLinearInterpolate_3','MaterialExpressionLinearInterpolate_4','MaterialExpressionLinearInterpolate_6']
    before={'scene':f['snapshot'](),'oldCount':sum(c.get_instance_count() for c in actor.get_components_by_class(u.InstancedStaticMeshComponent)),
        'terrain':[f['trace'](x,y) for x,y in [(-20000,-20000),(20000,20000),(-20000,18000),(18000,-18000),(0,1070),(240,0)]],
        'masks':[m.export_text() for m in masks],
        'snowOutputInputs':{k:[n.get_name() if n else None for n in lib.get_inputs_for_material_expression(m,nodes[k])][1:] for k in targets}}
    write('Before',before)
    g=u.load_asset(GRAPH)
    sampler=next(n.get_settings() for n in g.get_editor_property('nodes') if isinstance(n.get_settings(),u.PCGSurfaceSamplerSettings))
    sampler.set_editor_property('points_per_squared_meter',.054)
    assert u.EditorAssetLibrary.save_loaded_asset(g,False)
    assert actor.get_editor_property('pcg_component').seed==20261024
    actor.get_editor_property('pcg_component').generate(True)
    print('FERNS_DENSITY_3X_REQUESTED','before',before['oldCount'])


def survey():
    u,f=context()
    actors=f['ACTORS'].get_all_level_actors()
    lake=next(a for a in actors if a.get_actor_label()=='HillsideWater_Lake')
    comp=lake.get_component_by_class(u.WaterBodyComponent)
    assert not comp.get_editor_property('affects_landscape')
    spline=comp.get_water_spline();length=spline.get_spline_length()
    polygon=[]
    for i in range(256):
        p=spline.get_location_at_distance_along_spline(length*i/256,u.SplineCoordinateSpace.WORLD)
        polygon.append([p.x,p.y,p.z])
    water=sum(p[2] for p in polygon)/len(polygon)
    assert max(abs(p[2]-water) for p in polygon)<.01,'湖面必须为水平面'
    river=next(a for a in actors if a.get_actor_label()=='HillsideWater_River')
    rc=river.get_component_by_class(u.WaterBodyRiverComponent);rs=rc.get_water_spline()
    assert not rc.get_editor_property('affects_landscape')
    river_points=[];segments=max(1,math.ceil(rs.get_spline_length()/STEP))
    for i in range(segments+1):
        distance=rs.get_spline_length()*i/segments
        p=rs.get_location_at_distance_along_spline(distance,u.SplineCoordinateSpace.WORLD)
        key=rs.get_input_key_value_at_distance_along_spline(distance)
        river_points.append([p.x,p.y,p.z,rc.get_river_width_at_spline_input_key(key)])
    controls=[]
    for i in range(rs.get_number_of_spline_points()):
        p=rs.get_location_at_spline_point(i,u.SplineCoordinateSpace.WORLD)
        controls.append([p.x,p.y,p.z,rc.get_river_width_at_spline_input_key(float(i)),rc.get_water_velocity_at_spline_input_key(float(i))])
    margin=WIDTH+STEP*2
    bounds=polygon+[[p[0]+sign*p[3]/2,p[1]+sign*p[3]/2,p[2]] for p in river_points for sign in [-1,1]]
    ox=math.floor((min(p[0] for p in bounds)-margin)/STEP)*STEP
    oy=math.floor((min(p[1] for p in bounds)-margin)/STEP)*STEP
    nx=math.ceil((max(p[0] for p in bounds)+margin-ox)/STEP)+1
    ny=math.ceil((max(p[1] for p in bounds)+margin-oy)/STEP)+1
    # 只读Landscape碰撞；不把树、茅屋屋顶或水面当作地形。
    ignore=[a for a in actors if not isinstance(a,u.LandscapeProxy)]
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world();heights=[]
    for y in range(ny):
        row=[]
        for x in range(nx):
            h=u.SystemLibrary.line_trace_single(world,u.Vector(ox+x*STEP,oy+y*STEP,16000),u.Vector(ox+x*STEP,oy+y*STEP,-12000),
                u.TraceTypeQuery.ECC_VISIBILITY,False,ignore,u.DrawDebugTrace.NONE).export_text()
            assert 'LandscapeHeightfieldCollisionComponent' in h and 'bBlockingHit=True' in h
            z=re.search(r'ImpactPoint=\(X=[^,]+,Y=[^,]+,Z=([^\)]+)\)',h)
            row.append(float(z.group(1)))
        heights.append(row)
    write('Survey',{'origin':[ox,oy],'step':STEP,'nx':nx,'ny':ny,'waterZ':water,'polygon':polygon,'heights':heights,
        'waterTransform':f['stable'](lake.get_actor_transform()),'affectsLandscape':False,
        'riverPoints':river_points,'riverControls':controls,'riverTransform':f['stable'](river.get_actor_transform())})
    print('READ_ONLY_SHORE_SURVEY',nx*ny,'waterZ',water)


def plan():
    import numpy as np
    from PIL import Image
    survey=json.loads((OUT/'Survey.json').read_text(encoding='utf-8'))
    assert len(survey.get('riverPoints',[]))>1,'先通过Monolith读取本次河道，禁止复用仅含湖泊的旧采样'
    nx,ny=survey['nx'],survey['ny'];step=survey['step'];ox,oy=survey['origin']
    yy,xx=np.mgrid[0:ny,0:nx];xx=xx*step+ox;yy=yy*step+oy
    inside=np.zeros((ny,nx),dtype=bool);points=survey['polygon']
    for a,b in zip(points,points[1:]+points[:1]):
        inside ^= ((a[1]>yy)!=(b[1]>yy)) & (xx < (b[0]-a[0])*(yy-a[1])/((b[1]-a[1]) or 1e-12)+a[0])
    heights=np.array(survey['heights']);water=inside & (heights<survey['waterZ'])
    surface=np.full(water.shape,survey['waterZ'],dtype=float)
    river_water=np.zeros(water.shape,dtype=bool)
    nearest_river=np.full(water.shape,np.inf)
    river_distance=np.full(water.shape,np.inf)
    for a,b in zip(survey.get('riverPoints',[]),survey.get('riverPoints',[])[1:]):
        dx,dy=b[0]-a[0],b[1]-a[1]
        t=np.clip(((xx-a[0])*dx+(yy-a[1])*dy)/max(dx*dx+dy*dy,1e-9),0,1)
        distance=np.hypot(xx-a[0]-t*dx,yy-a[1]-t*dy)
        river_distance=np.minimum(river_distance,distance)
        width=(a[3]+t*(b[3]-a[3]))/2
        level=a[2]+t*(b[2]-a[2])
        actual=(distance<=width)&(heights<level)
        use=actual&(distance<nearest_river)
        nearest_river=np.where(use,distance,nearest_river)
        surface=np.where(use,level,surface)
        river_water|=actual
    water|=river_water
    assert river_water.sum()>0,'河岸必须基于真实露出河面生成'
    assert water.sum()>100,'必须从真实露出水面开始生成岸带'
    distance=np.full(water.shape,WIDTH+step)
    nearest_level=np.zeros(water.shape)
    cells=math.ceil(WIDTH/step)
    # 有界精确偏移距离，避免引入新依赖；仅离线执行，不进入游戏逐帧路径。
    for dy in range(-cells,cells+1):
        for dx in range(-cells,cells+1):
            d=math.hypot(dx,dy)*step
            if d>WIDTH:continue
            ya,yb=max(0,dy),min(ny,ny+dy);xa,xb=max(0,dx),min(nx,nx+dx)
            hit=water[ya-dy:yb-dy,xa-dx:xb-dx]
            better=hit&(d<distance[ya:yb,xa:xb])
            distance[ya:yb,xa:xb]=np.where(better,d,distance[ya:yb,xa:xb])
            nearest_level[ya:yb,xa:xb]=np.where(better,surface[ya-dy:yb-dy,xa-dx:xb-dx],nearest_level[ya:yb,xa:xb])
    t=np.clip((distance-100)/(WIDTH-100),0,1);coverage=1-t*t*(3-2*t)
    def smooth(a,b,x):
        t=np.clip((x-a)/(b-a),0,1);return t*t*(3-2*t)
    # 河面沿程下降，必须使用对应水面高程；不能用湖面固定高度裁剪整条河。
    coverage*=smooth(nearest_level-140,nearest_level-15,heights)*(1-smooth(nearest_level+180,nearest_level+400,heights))
    ART.mkdir(parents=True,exist_ok=True)
    path=ART/'VillageShoreWetnessMask.png'
    Image.fromarray(np.rint(coverage*255).astype(np.uint8)).resize((1024,1024),Image.Resampling.BILINEAR).save(path)
    dry=~water;bank=dry&(coverage>.2)
    idx=np.argwhere(bank)
    selected=min(idx,key=lambda p:abs((ox+p[1]*step)+8200)+abs((oy+p[0]*step)-4200))
    cy,cx=[int(v) for v in selected]
    river_bank=np.argwhere(bank&(river_distance<WIDTH+600))
    river_target=None
    if len(river_bank):
        ry,rx=[int(v) for v in min(river_bank,key=lambda p:abs((ox+p[1]*step)+10000)+abs((oy+p[0]*step)+15000))]
        river_target=[ox+rx*step,oy+ry*step,float(heights[ry,rx])]
    spec={'waterZ':survey['waterZ'],'widthCm':WIDTH,'worldOrigin':[ox-step/2,oy-step/2],
        'worldSize':[nx*step,ny*step],'mask':str(path),'SHA256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'waterSamples':int(water.sum()),'riverWaterSamples':int(river_water.sum()),'dryBankAreaM2':float(bank.sum()*step*step/10000),
        'cameraTarget':[ox+cx*step,oy+cy*step,float(heights[cy,cx])],
        'riverCameraTarget':river_target,
        'sourcePBR':'/DBAWorldPack_Village/Terrain/Textures/ShoreMud'}
    (ART/'VillageShore.json').write_text(json.dumps(spec,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in spec.items() if k in ['widthCm','dryBankAreaM2','waterSamples','riverWaterSamples','riverCameraTarget']}))


def author():
    u,f=context();lib=u.MaterialEditingLibrary
    spec=json.loads((ART/'VillageShore.json').read_text(encoding='utf-8'))
    material=u.load_asset(MATERIAL)
    old={n.get_name():n for n in lib.get_material_expressions(material)}
    assert not any(str(n.get_editor_property('desc')).startswith('VillageShore.') for n in old.values()),'禁止重复追加岸带'
    if not u.EditorAssetLibrary.does_asset_exist(MASK):
        task=u.AssetImportTask();task.filename=spec['mask'];task.destination_path=BASE;task.destination_name=MASK.rsplit('/',1)[1]
        task.automated=True;task.save=True;task.replace_existing=False
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mask=u.load_asset(MASK);assert isinstance(mask,u.Texture2D)
    mask.set_editor_property('srgb',False);mask.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_GRAYSCALE)
    mask.set_editor_property('address_x',u.TextureAddress.TA_CLAMP);mask.set_editor_property('address_y',u.TextureAddress.TA_CLAMP)
    assert u.EditorAssetLibrary.save_loaded_asset(mask,False)
    def node(cls,label):
        n=lib.create_material_expression(material,cls,2800,500);n.set_editor_property('desc','VillageShore.'+label);return n
    def link(a,o,b,i):assert lib.connect_material_expressions(a,o,b,i),(o,i)
    def scalar(name,value):
        n=node(u.MaterialExpressionScalarParameter,name);n.set_editor_property('parameter_name',u.Name(name));n.set_editor_property('default_value',value);return n
    def custom(label,code,inputs,kind):
        n=node(u.MaterialExpressionCustom,label);n.set_editor_property('code',code);n.set_editor_property('output_type',kind)
        pins=[]
        for k in inputs:
            pin=u.CustomInput();pin.set_editor_property('input_name',u.Name(k));pins.append(pin)
        n.set_editor_property('inputs',pins)
        for k,(a,o) in inputs.items():link(a,o,n,k)
        return n
    world=old['MaterialExpressionWorldPosition_0']
    origin=node(u.MaterialExpressionVectorParameter,'WorldRect');origin.set_editor_property('parameter_name',u.Name('VillageShoreWorldRect'))
    origin.set_editor_property('default_value',u.LinearColor(*spec['worldOrigin'],*spec['worldSize']))
    uv=custom('WorldMaskUV','return (P.xy-Rect.xy)/max(float2(Rect.z,Height),1.0);',{'P':(world,''),'Rect':(origin,'RGB'),'Height':(origin,'A')},u.CustomMaterialOutputType.CMOT_FLOAT2)
    tile=scalar('VillageShoreTileCm',250.)
    detail=custom('MudUV','return P.xy/max(Tile,1.0);',{'P':(world,''),'Tile':(tile,'')},u.CustomMaterialOutputType.CMOT_FLOAT2)
    def texture(name,path,coord,sampler,clamp=False):
        n=node(u.MaterialExpressionTextureSampleParameter2D,name);n.set_editor_property('parameter_name',u.Name(name))
        n.set_editor_property('texture',u.load_asset(path));n.set_editor_property('sampler_type',sampler)
        n.set_editor_property('sampler_source',u.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS if clamp else u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        link(coord,'',n,'UVs');return n
    coverage=texture('VillageShoreMask',MASK,uv,u.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,True)
    water=scalar('VillageShoreWaterZCm',spec['waterZ']);strength=scalar('VillageShoreStrength',1.)
    road=next(n for n in old.values() if isinstance(n,u.MaterialExpressionTextureSampleParameter2D) and str(n.get_editor_property('parameter_name'))=='ForestPathMask')
    alpha=custom('ShoreCoverage','return saturate(Mask*Strength)*(1-saturate(Road));',
        {'Mask':(coverage,'R'),'Strength':(strength,''),'Road':(road,'R')},u.CustomMaterialOutputType.CMOT_FLOAT1)
    root=spec['sourcePBR']+'/T_DBA_Terrain_ShoreMud_'
    color=texture('VillageShoreMudBaseColor',root+'BaseColor',detail,u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal=texture('VillageShoreMudNormal',root+'Normal',detail,u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm=texture('VillageShoreMudORM',root+'ORM',detail,u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    wetcolor=custom('WetMudColor','return Albedo*lerp(1.0,.82,saturate(Wet));',{'Albedo':(color,'RGB'),'Wet':(coverage,'R')},u.CustomMaterialOutputType.CMOT_FLOAT3)
    rough=custom('WetMudRoughness','return clamp(lerp(.72,.32,saturate(Wet))*lerp(.9,1.1,R),.25,.85);',{'Wet':(coverage,'R'),'R':(orm,'G')},u.CustomMaterialOutputType.CMOT_FLOAT1)
    nrm=custom('MudNormal','return normalize(float3(N.xy*.55,lerp(1.0,N.z,.55)));',{'N':(normal,'RGB')},u.CustomMaterialOutputType.CMOT_FLOAT3)
    # 在既有雪覆盖的基础端混合原ShoreMud；不碰雪/脚印/位移分支，不引入专服排除的客户端函数引用。
    for key,value,output in [('MaterialExpressionLinearInterpolate_1',wetcolor,''),('MaterialExpressionLinearInterpolate_3',rough,''),
        ('MaterialExpressionLinearInterpolate_4',nrm,''),('MaterialExpressionLinearInterpolate_6',orm,'R')]:
        target=old[key];base=lib.get_inputs_for_material_expression(material,target)[0]
        out=lib.get_input_node_output_name_for_material_expression(target,base) or ''
        blend=node(u.MaterialExpressionLinearInterpolate,key);link(base,out,blend,'A');link(value,output,blend,'B');link(alpha,'',blend,'Alpha')
        if key=='MaterialExpressionLinearInterpolate_4':
            normalized=node(u.MaterialExpressionNormalize,'FinalNormal');link(blend,'',normalized,'VectorInput');link(normalized,'',target,'A')
        else:link(blend,'',target,'A')
    errors=lib.recompile_material(material);assert not errors,list(errors)
    assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    print('NATIVE_SHORE_MUD_CONNECTED',WIDTH,'cm')


def update_combined_mask():
    """湖岸现有材质原位扩展河岸；共享一张遮罩、同一PBR组，不叠加新材质层。"""
    u,f=context();lib=u.MaterialEditingLibrary
    spec=json.loads((ART/'VillageShore.json').read_text(encoding='utf-8'))
    assert spec.get('riverWaterSamples',0)>0,'必须先完成真实河道采样'
    task=u.AssetImportTask();task.filename=spec['mask'];task.destination_path=BASE;task.destination_name=MASK.rsplit('/',1)[1]
    task.automated=True;task.save=True;task.replace_existing=True
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mask=u.load_asset(MASK);mask.set_editor_property('srgb',False);mask.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_GRAYSCALE)
    mask.set_editor_property('address_x',u.TextureAddress.TA_CLAMP);mask.set_editor_property('address_y',u.TextureAddress.TA_CLAMP)
    assert u.EditorAssetLibrary.save_loaded_asset(mask,False)
    material=u.load_asset(MATERIAL)
    for n in lib.get_material_expressions(material):
        desc=str(n.get_editor_property('desc'))
        if desc=='VillageShore.WorldRect':n.set_editor_property('default_value',u.LinearColor(*spec['worldOrigin'],*spec['worldSize']))
        if desc=='VillageShore.ShoreCoverage':n.set_editor_property('code','return saturate(Mask*Strength)*(1-saturate(Road));')
    errors=lib.recompile_material(material);assert not errors,list(errors)
    assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    print('LAKE_AND_RIVER_SHARED_SHORE_MASK_SAVED',spec['riverWaterSamples'])


def audit(phase='Generated',save=False):
    u,f=context();before=json.loads((OUT/'Before.json').read_text(encoding='utf-8'))
    assert f['snapshot']()==before['scene'],'人工场景/原有树实例变化，停止保存'
    for p in before['terrain']:assert max(abs(a-b) for a,b in zip(f['trace'](p[0],p[1]),p))<.01
    actors=f['ACTORS'].get_all_level_actors();a=next(a for a in actors if a.get_actor_label()=='Village_Ferns_PCG')
    rows=[];transforms=[]
    for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
        assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION and not c.get_editor_property('can_ever_affect_navigation')
        rows.append({'mesh':c.static_mesh.get_name(),'count':c.get_instance_count(),'cullEnd':c.get_editor_property('instance_end_cull_distance')})
        transforms.extend(c.static_mesh.get_name()+f['stable'](c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count()))
    count=sum(r['count'] for r in rows);assert count>before['oldCount']*2.5,rows
    director=next(a for a in actors if isinstance(a,u.GamePlatformPCGWorldDirector));masks,error=director.collect_spatial_masks()
    assert not error and [m.export_text() for m in masks]==before['masks']
    material=u.load_asset(MATERIAL);lib=u.MaterialEditingLibrary;nodes={n.get_name():n for n in lib.get_material_expressions(material)}
    for key,old in before['snowOutputInputs'].items():assert [n.get_name() if n else None for n in lib.get_inputs_for_material_expression(material,nodes[key])][1:]==old
    lake=next(a for a in actors if a.get_actor_label()=='HillsideWater_Lake');survey=json.loads((OUT/'Survey.json').read_text(encoding='utf-8'))
    assert f['stable'](lake.get_actor_transform())==survey['waterTransform']
    assert not lake.get_component_by_class(u.WaterBodyComponent).get_editor_property('affects_landscape')
    if 'riverControls' in survey:
        river=next(a for a in actors if a.get_actor_label()=='HillsideWater_River')
        rc=river.get_component_by_class(u.WaterBodyRiverComponent);rs=rc.get_water_spline()
        assert f['stable'](river.get_actor_transform())==survey['riverTransform']
        assert not rc.get_editor_property('affects_landscape')
        current=[]
        for i in range(rs.get_number_of_spline_points()):
            p=rs.get_location_at_spline_point(i,u.SplineCoordinateSpace.WORLD)
            current.append([p.x,p.y,p.z,rc.get_river_width_at_spline_input_key(float(i)),rc.get_water_velocity_at_spline_input_key(float(i))])
        assert current==survey['riverControls'],'河道形状/宽度/流速变化'
    digest=hashlib.sha256('\n'.join(sorted(transforms)).encode()).hexdigest()
    first=OUT/'AuditGenerated.json'
    if phase!='Generated':assert digest==json.loads(first.read_text(encoding='utf-8'))['instanceSHA256']
    if save:assert u.EditorLoadingAndSavingUtils.save_current_level()
    report={'phase':phase,'densityPerM2':.054,'before':before['oldCount'],'after':count,'ratio':count/before['oldCount'],
        'species':rows,'instanceSHA256':digest,'seed':20261024,'maskCount':len(masks),'protectedActorsUnchanged':len(before['scene']),
        'terrainUnchanged':True,'waterUnchanged':True,'snowChainUnchanged':True,'clientDeployed':False,'gpuMeasured':False}
    write('Audit'+phase,report);print(json.dumps(report))


def camera(shore=True):
    u,f=context()
    if shore:
        spec=json.loads((ART/'VillageShore.json').read_text(encoding='utf-8'))
        p=spec['riverCameraTarget'] if shore=='river' else [-10200,-6900,-51.589621]
        target=u.Vector(p[0]+300,p[1]-100,p[2]-60);location=u.Vector(p[0]-1000,p[1]-1600,p[2]+900)
    else:
        location=u.Vector(-11152.943569,-11163.959116,770.861567);target=u.Vector(-12002.943569,-9913.959116,180.861567)
    f['EDITOR'].set_level_viewport_camera_info(location,u.MathLibrary.find_look_at_rotation(location,target))
    viewport=u.get_editor_subsystem(u.LevelEditorSubsystem);viewport.editor_set_game_view(True);viewport.set_level_viewport_fov(75.,viewport.get_active_viewport_config_key())
    f['ACTORS'].clear_actor_selection_set();print('NATIVE_CAMERA_READY','Shore' if shore else 'Ferns')


if __name__=='__main__':plan()
