"""SSLM审核林间小路：确定性离线路径权重数据 + Monolith原生材质/样条装配。

plan()只生成数值遮罩，不生成或伪造UE资产；author()/verify()必须在UE内调用。
复用Village RoadDirt扫描纹理与GamePlatformSurface PBR覆盖函数，保留原地形天气链。
"""
import hashlib
import json
import math
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/VillageHillsideForest20261024'
ART=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/PCG/HillsideForest/Paths'
BASE='/DBAWorldPack_Village/Development/PCG/HillsideForest/Paths'
SOURCE='/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
MASTER=BASE+'/M_DBA_Review_ForestPaths'
MASK=BASE+'/T_DBA_Review_ForestPathsMask'
HALF_WIDTH=220.0
FEATHER=120.0


def plan():
    """在真实碰撞采样与树干/水域约束上寻路；只生成离线数值数据。"""
    import heapq
    import numpy as np
    from PIL import Image
    survey=json.loads((OUT/'PathsSurvey.json').read_text())
    terrain=np.array(survey['heights']);n=terrain.shape[0];step=survey['step']
    oy,ox=survey['origin'][1],survey['origin'][0]
    yy,xx=np.mgrid[0:n,0:n];xx=xx*step+ox;yy=yy*step+oy
    slope=terrain[:,:,1]
    lake=((xx+5500)/6500)**2+((yy+1800)/10600)**2<1
    blocked=(slope>35)|lake|((abs(xx-3500)<3300)&(abs(yy-19500)<3100))
    trees=survey['trees'];river=json.loads((OUT/'WaterLayout.json').read_text())['waters'][1]['points']
    def segment_dist(x,y,a,b):
        dx=b[0]-a[0];dy=b[1]-a[1]
        t=np.clip(((x-a[0])*dx+(y-a[1])*dy)/max(dx*dx+dy*dy,1),0,1)
        return np.hypot(x-a[0]-t*dx,y-a[1]-t*dy)
    for a,b in zip(river,river[1:]):blocked|=segment_dist(xx,yy,a,b)<1150
    for x,y,z,r in trees:blocked|=np.hypot(xx-x,yy-y)<r+HALF_WIDTH+FEATHER+100
    def valid_point(p):
        x,y=p
        if ((x+5500)/6500)**2+((y+1800)/10600)**2<1:return False
        if abs(x-3500)<3300 and abs(y-19500)<3100:return False
        if any(math.hypot(x-tx,y-ty)<r+HALF_WIDTH+FEATHER+100 for tx,ty,tz,r in trees):return False
        if any(float(segment_dist(x,y,a,b))<1150 for a,b in zip(river,river[1:])):return False
        return True
    def nearest(p):
        d=(xx-p[0])**2+(yy-p[1])**2;d=np.where(blocked,np.inf,d)
        return tuple(np.unravel_index(np.argmin(d),d.shape))
    def world(ij):return [float(xx[ij]),float(yy[ij])]
    def search(start,end):
        start=nearest(start);end=nearest(end);heap=[(0.,start)];cost={start:0.};prev={}
        while heap:
            _,a=heapq.heappop(heap)
            if a==end:break
            for di,dj in [(-1,0),(1,0),(0,-1),(0,1),(-1,-1),(-1,1),(1,-1),(1,1)]:
                b=(a[0]+di,a[1]+dj)
                if not (0<=b[0]<n and 0<=b[1]<n) or blocked[b]:continue
                if not valid_point(((xx[a]+xx[b])/2,(yy[a]+yy[b])/2)):continue
                length=math.hypot(di,dj)
                grade=abs(terrain[b][0]-terrain[a][0])/(length*step)
                nc=cost[a]+length*(1+3*(slope[b]/32)**2+4*grade)
                if nc<cost.get(b,math.inf):
                    cost[b]=nc;prev[b]=a
                    heapq.heappush(heap,(nc+math.hypot(b[0]-end[0],b[1]-end[1]),b))
        assert end in cost,('无法在现有地形/树干间连接小路',world(start),world(end))
        path=[end]
        while path[-1]!=start:path.append(prev[path[-1]])
        return [world(p) for p in reversed(path)]
    junction=world(nearest((0,15000)))
    routes=[('NorthWoodland',[(-19000,14000),junction,(18000,15000)]),
            ('CentralWoodland',[(14500,-18500),(6500,-3000),junction]),
            ('LakeWestWoodland',[junction,(-14500,7000),(-17000,-10000)])]
    paths=[]
    for name,waypoints in routes:
        pts=[]
        for a,b in zip(waypoints,waypoints[1:]):pts.extend(search(a,b)[bool(pts):])
        # Chaikin只平滑二维路线；高度仍交给真实Landscape，不生成浮空路网格。
        for repeat in range(2):
            smooth=[pts[0]]
            for a,b in zip(pts,pts[1:]):
                smooth.extend([[.75*a[0]+.25*b[0],.75*a[1]+.25*b[1]],[.25*a[0]+.75*b[0],.25*a[1]+.75*b[1]]])
            smooth.append(pts[-1])
            if all(valid_point(p) for p in smooth):pts=smooth
        length=sum(math.dist(a,b) for a,b in zip(pts,pts[1:]))
        paths.append({'name':name,'points':pts,'lengthCm':length})
    # 单张1024线性R8权重图；像素中心对应世界坐标，边缘为平滑泥土/草地渐变。
    res=1024;extent=50400.;origin=-25200.
    y,x=np.mgrid[0:res,0:res];x=origin+(x+.5)*extent/res;y=origin+(y+.5)*extent/res
    distance=np.full((res,res),np.inf,dtype=np.float32)
    for path in paths:
        for a,b in zip(path['points'],path['points'][1:]):
            # 只计算线段局部包围盒，避免整图逐线段扫描。
            pad=HALF_WIDTH+FEATHER+60
            x0=max(0,int((min(a[0],b[0])-pad-origin)/extent*res));x1=min(res,int((max(a[0],b[0])+pad-origin)/extent*res)+2)
            y0=max(0,int((min(a[1],b[1])-pad-origin)/extent*res));y1=min(res,int((max(a[1],b[1])+pad-origin)/extent*res)+2)
            d=segment_dist(x[y0:y1,x0:x1],y[y0:y1,x0:x1],a,b)
            distance[y0:y1,x0:x1]=np.minimum(distance[y0:y1,x0:x1],d)
    edge=18*np.sin(x*.021+y*.013)+12*np.sin(x*.037-y*.019)
    t=np.clip((HALF_WIDTH+FEATHER+edge-distance)/FEATHER,0,1);mask=t*t*(3-2*t)
    ART.mkdir(parents=True,exist_ok=True)
    Image.fromarray(np.uint8(mask*255),'L').save(ART/'ForestPathsMask.png')
    result={'schemaVersion':1,'seed':20261024,'paths':paths,'fullCoreWidthCm':HALF_WIDTH*2,
            'featherCm':FEATHER,'worldOriginXY':[origin,origin],'worldExtentCm':extent,'resolution':res,
            'slopeSampleLimitDegrees':35,'existingTrees':len(trees),'preserveTrees':True,
            'sourceSurveySHA256':hashlib.sha256((OUT/'PathsSurvey.json').read_bytes()).hexdigest()}
    (ART/'ForestPaths.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({'routes':[(p['name'],round(p['lengthCm']/100,1)) for p in paths],'treesPreserved':len(trees),'maskResolution':res}))


def author():
    """通过Monolith调用：创建审核材质派生及原生PCG样条，不雕刻Landscape。"""
    import unreal as u
    import runpy
    f=runpy.run_path(str(Path(__file__).with_name('AuthorVillageHillsideForestReview.py')));f['guard']()
    lib=u.MaterialEditingLibrary;assets=u.AssetToolsHelpers.get_asset_tools()
    spec=json.loads((ART/'ForestPaths.json').read_text(encoding='utf-8'))
    actors={a.get_actor_label():a for a in f['ACTORS'].get_all_level_actors()}
    land=next(a for a in actors.values() if isinstance(a,u.Landscape))
    assert land.get_editor_property('landscape_material').get_path_name().split('.')[0] in (SOURCE,MASTER)
    baseline=[[x,y,f['trace'](x,y)[0][2]] for y in range(-22000,22001,2000) for x in range(-22000,22001,2000)]
    (OUT/'PathsTerrainBefore.json').write_text(json.dumps(baseline))
    task=u.AssetImportTask();task.set_editor_property('filename',str(ART/'ForestPathsMask.png'))
    task.set_editor_property('destination_path',BASE);task.set_editor_property('destination_name',MASK.rsplit('/',1)[1])
    task.set_editor_property('automated',True);task.set_editor_property('replace_existing',True);task.set_editor_property('save',True)
    assets.import_asset_tasks([task]);mask=u.load_asset(MASK);assert isinstance(mask,u.Texture2D)
    mask.set_editor_property('srgb',False);mask.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_GRAYSCALE)
    mask.set_editor_property('address_x',u.TextureAddress.TA_CLAMP);mask.set_editor_property('address_y',u.TextureAddress.TA_CLAMP)
    assert u.EditorAssetLibrary.save_loaded_asset(mask,False)
    if u.EditorAssetLibrary.does_asset_exist(MASTER):raise RuntimeError('审核路径材质已存在；拒绝重复追加节点')
    material=u.EditorAssetLibrary.duplicate_asset(SOURCE,MASTER);assert material
    old={n.get_name():n for n in lib.get_material_expressions(material)}
    def node(cls,name):
        n=lib.create_material_expression(material,cls,2000,0);n.set_editor_property('desc','ForestPaths.'+name);return n
    def link(a,output,b,input_):assert lib.connect_material_expressions(a,output,b,input_),(output,input_)
    def scalar(name,value):
        n=node(u.MaterialExpressionScalarParameter,name);n.set_editor_property('parameter_name',u.Name(name));n.set_editor_property('default_value',value);return n
    def custom(name,code,inputs,output_type):
        n=node(u.MaterialExpressionCustom,name);n.set_editor_property('code',code);n.set_editor_property('output_type',output_type)
        pins=[]
        for key in inputs:p=u.CustomInput();p.set_editor_property('input_name',u.Name(key));pins.append(p)
        n.set_editor_property('inputs',pins)
        for key,(source,output) in inputs.items():link(source,output,n,key)
        return n
    world=old['MaterialExpressionWorldPosition_0']
    mask_uv=custom('MaskWorldUV','return (P.xy+25200.0)/50400.0;',{'P':(world,'')},u.CustomMaterialOutputType.CMOT_FLOAT2)
    tile=scalar('ForestPathTileCm',350.)
    uv=custom('RoadWorldUV','return P.xy/Tile;',{'P':(world,''),'Tile':(tile,'')},u.CustomMaterialOutputType.CMOT_FLOAT2)
    def texture(name,path,coord,sampler):
        n=node(u.MaterialExpressionTextureSampleParameter2D,name);n.set_editor_property('parameter_name',u.Name(name))
        n.set_editor_property('texture',u.load_asset(path));n.set_editor_property('sampler_type',sampler)
        n.set_editor_property('sampler_source',u.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS if name=='ForestPathMask' else u.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
        link(coord,'',n,'UVs');return n
    coverage=texture('ForestPathMask',MASK,mask_uv,u.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    texroot='/DBAWorldPack_Village/Terrain/Textures/RoadDirt/T_DBA_Terrain_RoadDirt_'
    albedo=texture('ForestPathBaseColor',texroot+'BaseColor',uv,u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal=texture('ForestPathNormal',texroot+'Normal',uv,u.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm=texture('ForestPathORM',texroot+'ORM',uv,u.MaterialSamplerType.SAMPLERTYPE_MASKS)
    tint=node(u.MaterialExpressionVectorParameter,'Tint');tint.set_editor_property('parameter_name',u.Name('ForestPathTint'))
    def linear(v):return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    rgb=[linear(v/255) for v in (197,173,131)];tint.set_editor_property('default_value',u.LinearColor(*rgb,1))
    wet=old['MaterialExpressionScalarParameter_4']
    color=custom('YellowDirt','float v=saturate(dot(Albedo,float3(.2126,.7152,.0722))/.3);return Tint*(.82+.36*v)*lerp(1.0,.58,saturate(Wet));',
        {'Albedo':(albedo,'RGB'),'Tint':(tint,'RGB'),'Wet':(wet,'')},u.CustomMaterialOutputType.CMOT_FLOAT3)
    nrm=custom('SoftSoilNormal','return normalize(float3(N.xy*.4,lerp(1.0,N.z,.4)));',{'N':(normal,'RGB')},u.CustomMaterialOutputType.CMOT_FLOAT3)
    rough=custom('DirtWetRoughness','return lerp(.7+.25*R,.36,saturate(Wet));',{'R':(orm,'G'),'Wet':(wet,'')},u.CustomMaterialOutputType.CMOT_FLOAT1)
    blend=node(u.MaterialExpressionMaterialFunctionCall,'PluginPBRBlend')
    blend.set_material_function(u.load_asset('/GamePlatformSurface/MaterialFunctions/Layers/MF_GP_PBRLayerBlend'))
    color_out=old['MaterialExpressionLinearInterpolate_1'];rough_out=old['MaterialExpressionLinearInterpolate_3']
    normal_out=old['MaterialExpressionLinearInterpolate_4'];ao_out=old['MaterialExpressionLinearInterpolate_6']
    # 把泥土接入四条原有天气混合的基础输入；所有积雪/压实/WPO输出保持原链。
    inputs={'BaseColor':lib.get_inputs_for_material_expression(material,color_out)[0],
            'BaseRoughness':lib.get_inputs_for_material_expression(material,rough_out)[0],
            'BaseNormal':lib.get_inputs_for_material_expression(material,normal_out)[0],
            'BaseAO':lib.get_inputs_for_material_expression(material,ao_out)[0]}
    for name,source in inputs.items():
        output=lib.get_input_node_output_name_for_material_expression({'BaseColor':color_out,'BaseRoughness':rough_out,'BaseNormal':normal_out,'BaseAO':ao_out}[name],source)
        link(source,output or '',blend,name)
    for name,source,output in [('OverlayColor',color,''),('OverlayNormal',nrm,''),('OverlayRoughness',rough,''),('OverlayAO',orm,'R'),('Coverage',coverage,'R')]:link(source,output,blend,name)
    for name,dest in [('BaseColor',color_out),('Normal',normal_out),('Roughness',rough_out),('AO',ao_out)]:link(blend,name,dest,'A')
    errors=lib.recompile_material(material);assert not errors,list(errors)
    assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    land.set_editor_property('landscape_material',material)
    director=actors['HillsideForest_WorldDirector']
    for path in spec['paths']:
        label='ForestPath_'+path['name'];actor=actors.get(label) or f['spawn'](u.GamePlatformPCGSplineActor,label,(0,0),0)
        actor.set_editor_property('domain_id',u.Name('Road.ForestPath'))
        actor.set_editor_property('carve_half_width_cm',HALF_WIDTH+FEATHER+100)
        spline=actor.get_editor_property('spline');spline.clear_spline_points(False)
        for x,y in path['points']:spline.add_spline_point(u.Vector(x,y,f['trace'](x,y)[0][2]+5),u.SplineCoordinateSpace.WORLD,False)
        for i in range(len(path['points'])):spline.set_spline_point_type(i,u.SplinePointType.LINEAR,False)
        spline.update_spline()
        if label not in actors:assert director.register_participant(actor)
    f['bind_spatial_sources']()
    graph=u.load_asset(f['GRAPH']);masks,error=director.collect_spatial_masks();assert not error
    carve=next(n for n in graph.get_editor_property('nodes') if str(n.node_title)=='WorldDirector_Road_Building_ManualLocks')
    carve.get_settings().set_editor_property('masks',masks);assert u.EditorAssetLibrary.save_loaded_asset(graph,False)
    assert max(abs(f['trace'](x,y)[0][2]-z) for x,y,z in baseline)<.001
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    print('NATIVE_FOREST_PATHS_SAVED',len(spec['paths']),'terrainUnchanged',len(baseline))


if __name__=='__main__':plan()
