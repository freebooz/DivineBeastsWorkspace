"""Monolith原生蕨类PCG作者。只追加装饰，不改地形/水域/导航/手工场景。

平台原生PCG负责采样、坡度、空间排除和实例化；脚本仅装配第三层内容。
setup后等待引擎异步生成，再audit/save；重载和再生成分别留证。
"""
import hashlib
import json
import math
import re
import shutil
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve().parent
OUT=ROOT/'Saved/Validation/VillageFerns20261024'
MAP='/DBAWorldPack_Village/Maps/L_Village_Start'
BASE='/DBAWorldPack_Village/PCG/Ferns'
LIB='/DBAFoliagePack_Core/Foliage'
GRAPH=BASE+'/PCG_DBA_Village_Ferns'
BLUEPRINT=BASE+'/BP_DBA_VillageFernScatter'
LABEL='Village_Ferns_PCG'
SEED=20261024
DENSITY=.054  # 2026-10-24 用户要求加密：原PCG .018/m²提高到3倍。
ACTORS=u.get_editor_subsystem(u.EditorActorSubsystem)
EDITOR=u.get_editor_subsystem(u.UnrealEditorSubsystem)


def stable(value):
    return re.sub(r'\(0x[0-9a-fA-F]+\)','(address)',str(value))


def guard():
    assert EDITOR.get_editor_world().get_path_name().split('.')[0]==MAP,'必须打开正式新手村'


def trace(x,y):
    h=u.SystemLibrary.line_trace_single(EDITOR.get_editor_world(),u.Vector(x,y,16000),
        u.Vector(x,y,-12000),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE)
    text=h.export_text()
    assert 'bBlockingHit=True' in text and 'LandscapeHeightfieldCollisionComponent' in text,text
    m=re.search(r'ImpactPoint=\(X=([^,]+),Y=([^,]+),Z=([^\)]+)\)',text)
    return [float(v) for v in m.groups()]


def snapshot():
    records=[]
    for a in ACTORS.get_all_level_actors():
        if a.get_actor_label()==LABEL or isinstance(a,u.CameraActor) and 'TransientCapture' in a.get_actor_label():
            continue
        instances=[]
        for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
            instances.append({'name':c.get_name(),'mesh':c.static_mesh.get_path_name() if c.static_mesh else None,
              'transforms':[stable(c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count())]})
        records.append({'name':a.get_name(),'label':a.get_actor_label(),
            'transform':stable(a.get_actor_transform()),'instances':instances})
    return sorted(records,key=lambda x:x['name'])


def save_json(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/(name+'.json')).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')


def setup():
    guard()
    assert not any(a.get_actor_label()==LABEL for a in ACTORS.get_all_level_actors()),'禁止重复追加'
    assert not u.EditorAssetLibrary.does_asset_exist(GRAPH),'已有图不得覆盖，使用增量修改'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'先保存/保护人工地图修改'
    OUT.mkdir(parents=True,exist_ok=True)
    original=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
    backup=OUT/'BeforeFerns_L_Village_Start.umap'
    assert not backup.exists(),'保护首次备份'
    shutil.copy2(original,backup)
    save_json('ProtectedSceneBefore',snapshot())
    samples=[trace(x,y) for x,y in [(-20000,-20000),(20000,20000),(-20000,18000),(18000,-18000),(0,1070),(240,0)]]
    director=next(a for a in ACTORS.get_all_level_actors() if isinstance(a,u.GamePlatformPCGWorldDirector))
    masks,error=director.collect_spatial_masks();assert not error and len(masks)>=10,error
    save_json('Setup',{'map':MAP,'sourceMapSHA256':hashlib.sha256(original.read_bytes()).hexdigest(),
        'seed':SEED,'slopeLimitDegrees':35,'densityPerSquareMeter':DENSITY,'terrainSamples':samples,
        'maskSnapshot':[m.export_text() for m in masks],'runtimeRegeneration':False,
        'collision':False,'navigationContribution':False,'cullStartCm':5000,'cullEndCm':8000})
    if not u.EditorAssetLibrary.does_directory_exist(BASE):assert u.EditorAssetLibrary.make_directory(BASE)
    tools=u.AssetToolsHelpers.get_asset_tools()
    meshes=[]
    for species in ['BostonFern','WesternSwordFern']:
        mesh=u.load_asset(LIB+'/Grass/'+species+'/SM_DBA_Foliage_'+species+'_Low')
        assert isinstance(mesh,u.StaticMesh)
        meshes.append(mesh)
        path=LIB+'/FoliageTypes/FT_DBA_'+species
        ft=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(
            path.rsplit('/',1)[1],path.rsplit('/',1)[0],u.FoliageType_InstancedStaticMesh,u.FoliageType_InstancedStaticMeshFactory())
        ft.set_editor_property('mesh',mesh)
        ft.set_editor_property('density',.018)
        ft.set_editor_property('radius',100.)
        ft.set_editor_property('align_to_normal',False)
        ft.set_editor_property('scale_x',u.FloatInterval(.75,1.1))
        ft.set_editor_property('cull_distance',u.Int32Interval(5000,8000))
        body=ft.get_editor_property('body_instance');body.set_editor_property('collision_profile_name',u.Name('NoCollision'))
        body.set_editor_property('collision_enabled',u.CollisionEnabled.NO_COLLISION);ft.set_editor_property('body_instance',body)
        assert u.EditorAssetLibrary.save_loaded_asset(ft,False)
    g=tools.create_asset(GRAPH.rsplit('/',1)[1],BASE,u.PCGGraph,u.PCGGraphFactory());assert g
    def node(cls,title,x):
        n,s=g.add_node_of_type(cls);n.set_editor_property('node_title',u.Name(title));n.set_node_position(x,0);return n,s
    def edge(a,p,b,q):assert g.add_edge(a,u.Name(p),b,u.Name(q))==b,(p,q)
    landscape,ls=node(u.PCGGetLandscapeSettings,'Existing_Landscape_ReadOnly',-900)
    ls.set_editor_property('unbounded',False)
    sampler,ss=node(u.PCGSurfaceSamplerSettings,'Seeded_Fern_Surface_Samples',-650)
    ss.set_editor_property('points_per_squared_meter',DENSITY)
    ss.set_editor_property('point_extents',u.Vector(90,90,80))
    ss.set_editor_property('looseness',1.)
    ss.set_editor_property('apply_density_to_points',False)
    normal,ns=node(u.PCGNormalToDensitySettings,'WorldUp_Slope',-400)
    ns.set_editor_property('normal',u.Vector(0,0,1));ns.set_editor_property('density_mode',u.PCGNormalToDensityMode.SET)
    ns.set_editor_property('strength',1.);ns.set_editor_property('offset',0.)
    slope,sl=node(u.PCGDensityFilterSettings,'Exclude_Slopes_Above_35_Degrees',-150)
    sl.set_editor_property('lower_bound',math.cos(math.radians(35)));sl.set_editor_property('upper_bound',1.)
    carve,cs=node(u.GamePlatformPCGSpatialCarveSettings,'Existing_WorldDirector_Road_Building_Water_Spawn_Masks',100)
    cs.set_editor_property('masks',masks);cs.set_editor_property('subject_priority',30)
    alive,al=node(u.PCGDensityFilterSettings,'Remove_Excluded_Points',350)
    al.set_editor_property('lower_bound',.0001);al.set_editor_property('upper_bound',1.)
    transform,ts=node(u.PCGTransformPointsSettings,'Upright_Yaw_And_Uniform_Scale',600)
    ts.set_editor_property('absolute_rotation',True)
    ts.set_editor_property('rotation_min',u.Rotator(pitch=0,yaw=0,roll=0));ts.set_editor_property('rotation_max',u.Rotator(pitch=0,yaw=359.9,roll=0))
    ts.set_editor_property('uniform_scale',True)
    ts.set_editor_property('scale_min',u.Vector(.75,.75,.75));ts.set_editor_property('scale_max',u.Vector(1.1,1.1,1.1))
    spawn,sp=node(u.PCGStaticMeshSpawnerSettings,'Weighted_Two_Ferns_HISM_NoCollision',850)
    sp.set_editor_property('mesh_selector_type',u.PCGMeshSelectorWeighted)
    entries=[]
    for mesh in meshes:
        desc=u.PCGSoftISMComponentDescriptor()
        for key,value in {'static_mesh':mesh,'component_class':u.HierarchicalInstancedStaticMeshComponent,
            'can_ever_affect_navigation':False,'generate_overlap_events':False,'use_default_collision':False,
            'instance_start_cull_distance':5000,'instance_end_cull_distance':8000,
            'world_position_offset_disable_distance':3500,'enable_density_scaling':True,
            'cast_shadow':False,'affect_distance_field_lighting':False}.items():desc.set_editor_property(key,value)
        body=desc.get_editor_property('body_instance');body.set_editor_property('collision_profile_name',u.Name('NoCollision'))
        body.set_editor_property('collision_enabled',u.CollisionEnabled.NO_COLLISION);desc.set_editor_property('body_instance',body)
        entry=u.PCGMeshSelectorWeightedEntry();entry.set_editor_property('descriptor',desc);entry.set_editor_property('weight',1)
        entries.append(entry)
    sp.get_editor_property('mesh_selector_parameters').set_editor_property('mesh_entries',entries)
    edge(g.get_input_node(),'In',landscape,'BoundingShape');edge(g.get_input_node(),'In',sampler,'Bounding Shape')
    edge(landscape,'Out',sampler,'Surface')
    for a,b in [(sampler,normal),(normal,slope),(slope,carve),(carve,alive),(alive,transform),(transform,spawn)]:edge(a,'Out',b,'In')
    edge(spawn,'Out',g.get_output_node(),'Out');assert u.EditorAssetLibrary.save_loaded_asset(g,False)
    factory=u.BlueprintFactory();factory.set_editor_property('parent_class',u.EditorAssetLibrary.load_blueprint_class('/DBAWorldPack_Village/PCG/Blueprints/BP_PCG_Forest'))
    bp=tools.create_asset(BLUEPRINT.rsplit('/',1)[1],BASE,u.Blueprint,factory);assert bp
    u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    actor=ACTORS.spawn_actor_from_class(u.EditorAssetLibrary.load_blueprint_class(BLUEPRINT),u.Vector(0,0,0))
    actor.set_actor_label(LABEL);actor.set_folder_path('Village/Foliage');actor.tags=[u.Name('SSLM.Decorative.Ferns')]
    for key in ['bounds','generation_bounds']:actor.get_editor_property(key).set_box_extent(u.Vector(24500,24500,16000),False)
    actor.set_editor_property('seed',SEED);actor.set_editor_property('graph',g)
    component=actor.get_editor_property('pcg_component');component.set_editor_property('seed',SEED);component.set_graph(g)
    component.set_editor_property('generation_trigger',u.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)
    # 不登记为权威空间来源。只消费现有掩码，避免服务器剥离装饰后留下空参与者。
    component.generate(True)
    print('FERN_NATIVE_PCG_GENERATION_REQUESTED',SEED,len(masks))


def audit(phase='Generated',save=False):
    guard()
    before=json.loads((OUT/'ProtectedSceneBefore.json').read_text(encoding='utf-8'))
    assert snapshot()==before,'原场景Actor/树实例发生变化，停止保存'
    setup=json.loads((OUT/'Setup.json').read_text(encoding='utf-8'))
    for p in setup['terrainSamples']:
        assert max(abs(a-b) for a,b in zip(trace(p[0],p[1]),p))<.01,'地形碰撞高度变化'
    a=next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label()==LABEL)
    components=[];transforms=[]
    for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
        count=c.get_instance_count();assert c.static_mesh.get_path_name().startswith(LIB+'/Grass/')
        assert c.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
        rows=[stable(c.get_instance_transform(i,world_space=True)) for i in range(count)]
        transforms.extend([c.static_mesh.get_name()+' '+t for t in rows])
        components.append({'mesh':c.static_mesh.get_path_name(),'class':c.get_class().get_name(),'count':count,
            'cullStart':c.get_editor_property('instance_start_cull_distance'),'cullEnd':c.get_editor_property('instance_end_cull_distance'),
            'navigation':c.get_editor_property('can_ever_affect_navigation')})
    assert len(components)==2 and sum(c['count'] for c in components)>500,components
    assert all(not c['navigation'] and c['cullEnd']==8000 for c in components)
    signature=hashlib.sha256('\n'.join(sorted(transforms)).encode()).hexdigest()
    first=OUT/'AuditGenerated.json'
    if phase!='Generated':assert signature==json.loads(first.read_text(encoding='utf-8'))['instanceSHA256'],'保存/重载/再生成结果不一致'
    report={'phase':phase,'map':MAP,'seed':SEED,'components':components,'total':len(transforms),
        'instanceSHA256':signature,'protectedActorsUnchanged':len(before),'terrainSamplesUnchanged':len(setup['terrainSamples']),
        'nativePCG':True,'runtimeTested':False,'gpuMeasured':False}
    if save:assert u.EditorLoadingAndSavingUtils.save_current_level()
    save_json('Audit'+phase,report);print(json.dumps(report))
    return report


def regenerate():
    guard()
    a=next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label()==LABEL)
    a.get_editor_property('pcg_component').generate(True)
    print('FERN_REGENERATION_REQUESTED')


def capture(name='FernsGround'):
    guard()
    a=next(a for a in ACTORS.get_all_level_actors() if a.get_actor_label()==LABEL)
    c=max(a.get_components_by_class(u.InstancedStaticMeshComponent),key=lambda c:c.get_instance_count())
    # 从实际已生成实例选一簇，不为截图摆放额外植被。
    candidates=[c.get_instance_transform(i,world_space=True).translation for i in range(c.get_instance_count())]
    p=min(candidates,key=lambda p:abs(p.x+12000)+abs(p.y+11000))
    location=u.Vector(p.x+350,p.y-550,p.z+280)
    target=u.Vector(p.x,p.y,p.z+50)
    rotation=u.MathLibrary.find_look_at_rotation(location,target)
    EDITOR.set_level_viewport_camera_info(location,rotation)
    viewport=u.get_editor_subsystem(u.LevelEditorSubsystem)
    viewport.editor_set_game_view(True)
    viewport.set_level_viewport_fov(75.,viewport.get_active_viewport_config_key())
    ACTORS.clear_actor_selection_set()
    # 调用者随后执行原生HighResShot并检查文件；本函数不创建会弄脏地图的截图Actor。
    print('FERN_GPU_CAMERA_READY',name,stable(location))
