"""SSLM 独立山坡森林审核：原生 PCG 组合、明确参与者、只读地表。

仅允许 Development 审核地图；不修改正式地图、原型网格、权重图或地形。
由 Monolith run_python 调用 build()/audit()/regenerate()。点生成和实例化均由
UE PCG 完成，Python 只装配资产与审核；道路/建筑测试夹具不代表正式场景。
"""
import hashlib
import json
import math
import re
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
OUT = ROOT / "Saved/Validation/VillageHillsideForest20261024"
MAP = "/DBAWorldPack_Village/Development/PCG/Maps/L_DBA_Village_HillsideForest_Review"
BASE = "/DBAWorldPack_Village/Development/PCG/HillsideForest"
GRAPH = BASE + "/PCG_DBA_HillsideForest_Review"
SOURCE = "/DBAWorldPack_Village/Terrain/PCG/Graphs/PCG_DBA_Terrain_ForestWeightFiltered"
MESH = "/DBAWorldPack_Village/Development/PCG/MeshesUV/SM_DBA_PCG_ForestCanopy_TechUV"
FORMAL = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap"
FORMAL_HASH = "e1bc67e7a410d057bf2827d831ca401a77fe8ce938b60608a0069e122947f4b0"
SEED = 20261024
MAX_SLOPE = 35.0
CROWN_AREA = 8.0
CROWN_FACTOR = math.sqrt(CROWN_AREA)
CROWN_GAP_CM = 100.0
CROWN_PRUNE_FACTOR = .25
MIN_TREES = 95
ROAD = [(-15000,13000),(-8000,15000),(0,15000),(6500,17000),(16000,18000)]
ACTORS = u.get_editor_subsystem(u.EditorActorSubsystem)


def guard():
    w = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    assert w.get_path_name().split('.')[0] == MAP, "只能修改独立审核地图"
    assert hashlib.sha256(FORMAL.read_bytes()).hexdigest() == FORMAL_HASH, "正式地图已变化，停止"
    return w


def trace(x, y):
    """只读真实 Landscape 碰撞。UE5.8 HitResult 保护字段通过原生文本导出读取。"""
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    h = u.SystemLibrary.line_trace_single(world, u.Vector(x,y,15000),
        u.Vector(x,y,-10000), u.TraceTypeQuery.ECC_VISIBILITY, False, [], u.DrawDebugTrace.NONE)
    assert h is not None
    text = h.export_text()
    assert 'bBlockingHit=True' in text and 'LandscapeHeightfieldCollisionComponent' in text
    def vec(name):
        m = re.search(name+r'=\(X=([^,]+),Y=([^,]+),Z=([^\)]+)\)', text)
        assert m, text
        return tuple(float(v) for v in m.groups())
    return vec('ImpactPoint'), vec('ImpactNormal')


def spawn(cls, label, xy=(0,0), z=None):
    if z is None:
        z = trace(*xy)[0][2]
    a = ACTORS.spawn_actor_from_class(cls,u.Vector(xy[0],xy[1],z))
    assert a
    a.set_actor_label(label)
    a.set_folder_path('HillsideForest_ReviewOnly')
    a.set_editor_property('tags',[u.Name('SSLM.HillsideForest.ReviewOnly')])
    return a


def box_bounds(a, extent):
    for key in ('bounds','generation_bounds'):
        a.get_editor_property(key).set_box_extent(u.Vector(*extent),False)


def build():
    guard()
    assert not any(isinstance(a,u.GamePlatformPCGActorBase) for a in ACTORS.get_all_level_actors())
    assert not u.EditorAssetLibrary.does_asset_exist(GRAPH), '禁止覆盖审核资产'
    original = {a.get_actor_label():a.get_actor_transform().export_text() for a in ACTORS.get_all_level_actors()}
    land = next(a for a in ACTORS.get_all_level_actors() if isinstance(a,u.Landscape))
    has_weights = 'LS_ForestSoil' in [str(n) for n in land.get_target_layer_names()]
    director = spawn(u.GamePlatformPCGWorldDirector,'HillsideForest_WorldDirector',z=0)
    participants=[]
    def register(a, domain):
        a.set_editor_property('domain_id',u.Name(domain))
        assert director.register_participant(a)
        participants.append(a)
        return a
    # 新增的测试道路只输出二维排除，不雕刻地形、不生成道路碰撞。
    road = register(spawn(u.GamePlatformPCGSplineActor,'ReviewFixture_Road_5mClearance',z=0),'Road.Main')
    road.set_editor_property('carve_half_width_cm',1000.0) # 5m半路宽 + 5m树干缓冲。
    spline=road.get_editor_property('spline')
    spline.clear_spline_points(False)
    for x,y in ROAD:
        spline.add_spline_point(u.Vector(x,y,trace(x,y)[0][2]+10),u.SplineCoordinateSpace.WORLD,False)
    for i in range(len(ROAD)):
        spline.set_spline_point_type(i,u.SplinePointType.LINEAR,False)
    spline.update_spline()
    exclusions=[('ReviewFixture_Building_20mClearance',(3500,19500),(2800,2600,15000),'Building.Parcel'),
                ('Protected_ManualVillageLake', (0,0),(10500,10500,15000),'ManualLock')]
    for label,xy,extent,domain in exclusions:
        a=register(spawn(u.GamePlatformPCGExclusionActor,label,xy,0),domain)
        a.set_editor_property('exclusion_source',u.Name(label))
        box_bounds(a,extent)
    for a in list(ACTORS.get_all_level_actors()):
        if isinstance(a,u.PlayerStart):
            p=a.get_actor_location()
            e=register(spawn(u.GamePlatformPCGExclusionActor,'Protected_'+a.get_actor_label(),(p.x,p.y),0),'Gameplay.Spawn')
            box_bounds(e,(2200,2200,15000))
    bp=u.EditorAssetLibrary.load_blueprint_class('/DBAWorldPack_Village/PCG/Blueprints/BP_PCG_Forest')
    forests=[]
    for label,xy,ext in [('North',(0,16500),(23500,7500,15000)),
                          ('West',(-17000,-3000),(6500,12000,15000)),
                          ('East',(17000,-3000),(6500,12000,15000))]:
        a=register(spawn(bp,'HillsideForest_'+label,xy,0),'Forest.Canopy')
        box_bounds(a,ext)
        a.set_editor_property('seed',SEED)
        forests.append(a)
    assert director.validate_participant_set()==''
    mask_result=director.collect_spatial_masks()
    assert mask_result is not None and not mask_result[1]
    masks=mask_result[0]
    u.EditorAssetLibrary.make_directory(BASE)
    g=u.AssetToolsHelpers.get_asset_tools().create_asset(GRAPH.rsplit('/',1)[1],BASE,u.PCGGraph,u.PCGGraphFactory())
    g.set_editor_property('landscape_uses_metadata',True)
    def node(cls,title,x,y=0):
        n,s=g.add_node_of_type(cls);n.set_editor_property('node_title',u.Name(title));n.set_node_position(x,y)
        return n,s
    def edge(a,p,b,q):
        assert g.add_edge(a,u.Name(p),b,u.Name(q)) == b, (p,q)
    branch,bs=node(u.PCGBranchSettings,'HasForestWeightLayer',-900)
    bs.set_editor_property('output_to_b',has_weights)
    sub,ss=node(u.PCGSubgraphSettings,'ExistingForestWeightFiltered',-600,-250)
    ss.set_subgraph(u.load_asset(SOURCE))
    landscape,ls=node(u.PCGGetLandscapeSettings,'ReviewOnly_UnpaintedTerrain',-600,250)
    ls.set_editor_property('unbounded',False)
    sampler,sp=node(u.PCGSurfaceSamplerSettings,'Bounded_Canopy_Sampling',-350,250)
    sp.set_editor_property('points_per_squared_meter',0.0008)
    sp.set_editor_property('point_extents',u.Vector(650,650,100))
    sp.set_editor_property('looseness',1.0)
    sp.set_editor_property('apply_density_to_points',False)
    merge,ms=node(u.PCGMergeSettings,'Selected_Terrain_Source',-100)
    normal,ns=node(u.PCGNormalToDensitySettings,'WorldUp_Slope',150)
    ns.set_editor_property('normal',u.Vector(0,0,1));ns.set_editor_property('strength',1.0)
    ns.set_editor_property('offset',0.0);ns.set_editor_property('density_mode',u.PCGNormalToDensityMode.SET)
    slope,ds=node(u.PCGDensityFilterSettings,'Maximum_35_Degrees',400)
    ds.set_editor_property('lower_bound',math.cos(math.radians(MAX_SLOPE)));ds.set_editor_property('upper_bound',1.0)
    carve,cs=node(u.GamePlatformPCGSpatialCarveSettings,'WorldDirector_Road_Building_ManualLocks',650)
    cs.set_editor_property('masks',masks);cs.set_editor_property('subject_priority',30)
    alive,als=node(u.PCGDensityFilterSettings,'Remove_Excluded_Points',900)
    als.set_editor_property('lower_bound',0.0001);als.set_editor_property('upper_bound',1.0)
    transform,ts=node(u.PCGTransformPointsSettings,'Upright_Seeded_Rotation_Scale',1150)
    ts.set_editor_property('absolute_rotation',True)
    ts.set_editor_property('rotation_min',u.Rotator(pitch=0,yaw=0,roll=0))
    ts.set_editor_property('rotation_max',u.Rotator(pitch=0,yaw=359.9,roll=0))
    ts.set_editor_property('uniform_scale',True);ts.set_editor_property('scale_min',u.Vector(6.4,6.4,6.4));ts.set_editor_property('scale_max',u.Vector(9.6,9.6,9.6))
    mesh,sm=node(u.PCGStaticMeshSpawnerSettings,'Existing_Prototype_Tree_HISM',1400)
    sm.set_editor_property('mesh_selector_type',u.PCGMeshSelectorWeighted)
    desc=u.PCGSoftISMComponentDescriptor()
    desc.set_editor_property('static_mesh',u.load_asset(MESH))
    desc.set_editor_property('can_ever_affect_navigation',False)
    desc.set_editor_property('generate_overlap_events',False)
    desc.set_editor_property('use_default_collision',False)
    body=desc.get_editor_property('body_instance');body.set_editor_property('collision_profile_name',u.Name('NoCollision'))
    body.set_editor_property('collision_enabled',u.CollisionEnabled.NO_COLLISION);desc.set_editor_property('body_instance',body)
    entry=u.PCGMeshSelectorWeightedEntry();entry.set_editor_property('descriptor',desc);entry.set_editor_property('weight',1)
    sm.get_editor_property('mesh_selector_parameters').set_editor_property('mesh_entries',[entry])
    edge(g.get_input_node(),'In',branch,'In')
    edge(branch,'Output B',sub,'In')
    edge(branch,'Output A',landscape,'BoundingShape');edge(branch,'Output A',sampler,'Bounding Shape')
    edge(landscape,'Out',sampler,'Surface');edge(sampler,'Out',merge,'In');edge(sub,'Out',merge,'In')
    for a,b in [(merge,normal),(normal,slope),(slope,carve),(carve,alive),(alive,transform),(transform,mesh)]:edge(a,'Out',b,'In')
    edge(mesh,'Out',g.get_output_node(),'Out')
    assert u.EditorAssetLibrary.save_loaded_asset(g,False)
    for a in forests:
        a.set_editor_property('graph',g)
        c=a.get_editor_property('pcg_component');c.set_editor_property('seed',SEED);c.set_graph(g)
        c.generate(True)
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'Setup.json').write_text(json.dumps({'seed':SEED,'maxSlopeDegrees':MAX_SLOPE,'graph':GRAPH,
      'map':MAP,'sourceGraph':SOURCE,'mesh':MESH,'hasForestWeightLayer':has_weights,'maskCount':len(masks),
      'originalActorTransforms':original,'formalSHA256':FORMAL_HASH,'prototypeOnly':True},indent=2),encoding='utf-8')
    bind_spatial_sources()
    revise_scatter()
    print('NATIVE_PCG_GENERATION_REQUESTED',len(forests),'masks',len(masks),'seed',SEED)


def bind_spatial_sources():
    """道路/排除放置器绑定原生空间输入图；只传递自身空间数据，不生成模型。"""
    guard()
    path=BASE+'/PCG_DBA_ReviewSpatialSource'
    g=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    if g is None:
        g=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],BASE,u.PCGGraph,u.PCGGraphFactory())
        n,s=g.add_node_of_type(u.PCGGatherSettings)
        n.set_editor_property('node_title',u.Name('Explicit_Spatial_Source_Only_NoSpawn'))
        assert g.add_edge(g.get_input_node(),'In',n,'In')==n
        assert g.add_edge(n,'Out',g.get_output_node(),'Out')==g.get_output_node()
        assert u.EditorAssetLibrary.save_loaded_asset(g,False)
    for a in ACTORS.get_all_level_actors():
        if isinstance(a,(u.GamePlatformPCGExclusionActor,u.GamePlatformPCGSplineActor)):
            assert 'SSLM.HillsideForest.ReviewOnly' in [str(t) for t in a.tags]
            a.set_editor_property('graph',g)
            c=a.get_editor_property('pcg_component');c.set_graph(g)
            c.set_editor_property('seed',a.get_editor_property('seed'))
    director=next(a for a in ACTORS.get_all_level_actors() if isinstance(a,u.GamePlatformPCGWorldDirector))
    plan=director.build_static_execution_plan()
    assert plan is not None and plan[1]=='', 'WorldDirector 静态计划未通过'
    print('WORLD_DIRECTOR_PLAN_OK',len(plan[0]))


def regenerate():
    guard()
    for a in ACTORS.get_all_level_actors():
        if a.get_actor_label().startswith('HillsideForest_') and isinstance(a,u.GamePlatformPCGActorBase):
            a.get_editor_property('pcg_component').generate(True)
    print('REGENERATE_REQUESTED')


def wide_crown_mesh():
    """只编辑独立派生网格：叶冠XY sqrt(8)倍，投影面积8倍，主干不变。"""
    guard()
    path=BASE+'/SM_DBA_Review_WideCrown'
    exists=u.EditorAssetLibrary.does_asset_exist(path)
    evidence=OUT/'CrownGeometry.json'
    if exists and evidence.exists() and json.loads(evidence.read_text())['projectedAreaFactor']==CROWN_AREA:
        return u.load_asset(path)
    source=u.load_asset(MESH)
    mesh=u.load_asset(path) if exists else u.EditorAssetLibrary.duplicate_asset(MESH,path)
    assert mesh is not None
    dm=u.DynamicMesh()
    result=u.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(source,dm,
        u.GeometryScriptCopyMeshFromAssetOptions(),u.GeometryScriptMeshReadLOD())
    assert result[1]==u.GeometryScriptOutcomePins.SUCCESS
    foliage=set();bark=set()
    for mat in range(4):
        selection=u.GeometryScript_MeshSelection.select_mesh_elements_by_material_id(dm,mat,u.GeometryScriptMeshSelectionType.VERTICES)[1]
        ids=u.GeometryScript_MeshSelection.convert_mesh_selection_to_index_array(dm,selection)[1]
        (bark if mat==0 else foliage).update(ids)
    assert foliage and bark
    before={i:u.GeometryScript_MeshQueries.get_vertex_position(dm,i)[0] for i in foliage|bark}
    # 同步伸长侧枝末端；半径35cm内的主干与侧枝连接点保持原位置。
    chosen=foliage|{i for i in bark if math.hypot(before[i].x,before[i].y)>35.0}
    selection=u.GeometryScript_MeshSelection.convert_index_array_to_mesh_selection(dm,sorted(chosen),u.GeometryScriptMeshSelectionType.VERTICES)[1]
    u.GeometryScript_MeshTransforms.scale_mesh_selection(dm,selection,u.Vector(CROWN_FACTOR,CROWN_FACTOR,1),u.Vector(0,0,0))
    for i in foliage:
        after=u.GeometryScript_MeshQueries.get_vertex_position(dm,i)[0];old=before[i]
        assert abs(after.x-old.x*CROWN_FACTOR)<.001 and abs(after.y-old.y*CROWN_FACTOR)<.001 and abs(after.z-old.z)<.001
    for i in bark-chosen:
        assert (u.GeometryScript_MeshQueries.get_vertex_position(dm,i)[0]-before[i]).length()<.001
    opt=u.GeometryScriptCopyMeshToAssetOptions()
    opt.set_editor_property('enable_recompute_normals',True);opt.set_editor_property('enable_recompute_tangents',True)
    result=u.GeometryScript_AssetUtils.copy_mesh_to_static_mesh(dm,mesh,opt,u.GeometryScriptMeshWriteLOD())
    assert result[1]==u.GeometryScriptOutcomePins.SUCCESS
    assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
    (OUT/'CrownGeometry.json').write_text(json.dumps({'source':MESH,'derived':path,'foliageVertices':len(foliage),
        'horizontalFactor':CROWN_FACTOR,'projectedAreaFactor':CROWN_AREA,'trunkVerticesUnchanged':len(bark-chosen),'heightUnchanged':True},indent=2),encoding='utf-8')
    print('NATIVE_WIDE_CROWN_SAVED',len(foliage),'areaFactor',CROWN_AREA)
    return mesh


def simple_materials(mesh):
    """项目审核内容：共享不透明PBR母材质、四个实例；颜色源于项目V3配色规范。"""
    guard()
    folder=BASE+'/Materials';path=folder+'/M_DBA_Review_TreeSimple'
    lib=u.MaterialEditingLibrary;assets=u.AssetToolsHelpers.get_asset_tools()
    if u.EditorAssetLibrary.does_asset_exist(path):
        parent=u.load_asset(path)
    else:
        parent=assets.create_asset(path.rsplit('/',1)[1],folder,u.Material,u.MaterialFactoryNew())
        parent.set_editor_property('blend_mode',u.BlendMode.BLEND_OPAQUE)
        parent.set_editor_property('two_sided',False)
        color=lib.create_material_expression(parent,u.MaterialExpressionVectorParameter,-420,0)
        color.set_editor_property('parameter_name',u.Name('BaseColor'))
        color.set_editor_property('default_value',u.LinearColor(.1,.2,.1,1))
        rough=lib.create_material_expression(parent,u.MaterialExpressionScalarParameter,-420,180)
        rough.set_editor_property('parameter_name',u.Name('Roughness'));rough.set_editor_property('default_value',.8)
        metal=lib.create_material_expression(parent,u.MaterialExpressionConstant,-420,340)
        metal.set_editor_property('r',0.0)
        assert lib.connect_material_property(color,'RGB',u.MaterialProperty.MP_BASE_COLOR)
        assert lib.connect_material_property(rough,'',u.MaterialProperty.MP_ROUGHNESS)
        assert lib.connect_material_property(metal,'',u.MaterialProperty.MP_METALLIC)
        lib.set_base_material_usage(parent,u.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES,True)
        errors=lib.recompile_material(parent)
        assert not errors, list(errors)
        assert u.EditorAssetLibrary.save_loaded_asset(parent,False)
    # 2026-10-24用户最新指定桃花色；沿用V3桃花色板，只调整本审核树冠材质。
    palette=[('Bark','685541',.9),('LeafHighlight','F1D5CF',.8),
             ('Blossom','D99EA9',.85),('Canopy','E8B6BD',.85)]
    evidence=[]
    for index,(name,hexcolor,roughness) in enumerate(palette):
        path=folder+'/MI_DBA_Review_'+name
        inst=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else assets.create_asset(
            path.rsplit('/',1)[1],folder,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
        lib.set_material_instance_parent(inst,parent)
        def linear(value):
            value=value/255.0
            return value/12.92 if value<=.04045 else ((value+.055)/1.055)**2.4
        rgb=[linear(int(hexcolor[j:j+2],16)) for j in (0,2,4)]
        # UE5.8该setter写入成功仍固定返回false；以实际读取的参数核验。
        lib.set_material_instance_vector_parameter_value(inst,'BaseColor',u.LinearColor(*rgb,1))
        lib.set_material_instance_scalar_parameter_value(inst,'Roughness',roughness)
        lib.update_material_instance(inst)
        actual=lib.get_material_instance_vector_parameter_value(inst,'BaseColor')
        assert max(abs(a-b) for a,b in zip((actual.r,actual.g,actual.b),rgb))<.00001
        assert abs(lib.get_material_instance_scalar_parameter_value(inst,'Roughness')-roughness)<.00001
        assert u.EditorAssetLibrary.save_loaded_asset(inst,False)
        mesh.set_material(index,inst)
        evidence.append({'slot':index,'material':path,'sRGB':'#'+hexcolor,'linearRGB':rgb,'roughness':roughness})
    assert u.EditorAssetLibrary.save_loaded_asset(mesh,False)
    (OUT/'SimpleMaterials.json').write_text(json.dumps({'parent':parent.get_path_name(),
        'blend':'Opaque','metallic':0,'authoredTextureSamples':0,'paletteSource':'SSLM全局美术配色策略V3.0桃花色板；2026-10-24用户最新指定审核树冠桃花色',
        'slots':evidence},ensure_ascii=False,indent=2),encoding='utf-8')
    print('NATIVE_SIMPLE_MATERIALS_SAVED',len(evidence))


def revise_scatter():
    """整体4倍、冠幅面积8倍；用户允许冠层交叠，保留树干间距且至少60棵。"""
    guard();g=u.load_asset(GRAPH);mesh=wide_crown_mesh()
    simple_materials(mesh)
    nodes={str(n.node_title):n for n in g.get_editor_property('nodes')}
    if 'Landscape_Collision_Projection' not in nodes:
        ray,rs=g.add_node_of_type(u.PCGWorldRayHitSettings)
        ray.set_editor_property('node_title',u.Name('Landscape_Collision_Surface'));ray.set_node_position(-100,450)
        query=rs.get_editor_property('query_params')
        query.set_editor_property('actor_class_filter',u.PCGWorldQueryFilter.INCLUDE)
        query.set_editor_property('actor_class',u.LandscapeProxy)
        query.set_editor_property('collision_channel',u.CollisionChannel.ECC_VISIBILITY)
        query.set_editor_property('trace_complex',False);query.set_editor_property('ignore_pcg_hits',True)
        query.set_editor_property('override_default_params',True)
        query.set_editor_property('ray_origin',u.Vector(0,0,15000))
        query.set_editor_property('ray_direction',u.Vector(0,0,-1));query.set_editor_property('ray_length',30000.0)
        rs.set_editor_property('query_params',query)
        project,prs=g.add_node_of_type(u.PCGProjectionSettings)
        project.set_editor_property('node_title',u.Name('Landscape_Collision_Projection'));project.set_node_position(100,450)
        params=prs.get_editor_property('projection_params')
        params.set_editor_property('project_positions',True);params.set_editor_property('project_rotations',True)
        params.set_editor_property('project_scales',False);prs.set_editor_property('projection_params',params)
        prs.set_editor_property('force_collapse_to_point',True)
        merge=nodes['Selected_Terrain_Source'];normal=nodes['WorldUp_Slope']
        assert g.remove_edge(merge,'Out',normal,'In')
        assert g.add_edge(merge,'Out',project,'In')==project
        assert g.add_edge(ray,'Out',project,'Projection Target')==project
        assert g.add_edge(project,'Out',normal,'In')==normal
    sampler=nodes['Bounded_Canopy_Sampling'].get_settings()
    sampler.set_editor_property('points_per_squared_meter',0.0013)
    sampler.set_editor_property('point_extents',u.Vector(150,150,100));sampler.set_editor_property('looseness',3.0)
    transform=nodes['Upright_Seeded_Rotation_Scale'];ts=transform.get_settings()
    ts.set_editor_property('rotation_min',u.Rotator(pitch=0,yaw=0,roll=0));ts.set_editor_property('rotation_max',u.Rotator(pitch=0,yaw=359.9,roll=0))
    ts.set_editor_property('scale_min',u.Vector(6.4,6.4,6.4));ts.set_editor_property('scale_max',u.Vector(9.6,9.6,9.6))
    if 'WideCrown_PhysicalBounds' not in nodes:
        bounds,bs=g.add_node_of_type(u.PCGBoundsModifierSettings)
        bounds.set_editor_property('node_title',u.Name('WideCrown_PhysicalBounds'));bounds.set_node_position(1050,220)
        b=mesh.get_bounds();bs.set_editor_property('mode',u.PCGBoundsModifierMode.SET)
        bs.set_editor_property('bounds_min',b.origin-b.box_extent);bs.set_editor_property('bounds_max',b.origin+b.box_extent)
        prune,ps=g.add_node_of_type(u.PCGSelfPruningSettings)
        prune.set_editor_property('node_title',u.Name('Randomized_Crown_OverlapPruning'));prune.set_node_position(1400,220)
        params=ps.get_editor_property('parameters');params.set_editor_property('pruning_type',u.PCGSelfPruningType.ALL_EQUAL)
        params.set_editor_property('randomized_pruning',True);ps.set_editor_property('parameters',params)
        alive=nodes['Remove_Excluded_Points'];spawner=nodes['Existing_Prototype_Tree_HISM']
        assert g.remove_edge(alive,'Out',transform,'In') and g.remove_edge(transform,'Out',spawner,'In')
        for a,b in [(alive,bounds),(bounds,transform),(transform,prune),(prune,spawner)]:assert g.add_edge(a,'Out',b,'In')==b
    # 高度轴贯穿整个审核世界，避免不同坡高使相交冠层逃过三维裁剪。
    # 用户允许冠层交叠；使用冠层25%包围盒维持树干和侧枝根部间距。
    nodes={str(n.node_title):n for n in g.get_editor_property('nodes')}
    bs=nodes['WideCrown_PhysicalBounds'].get_settings();b=mesh.get_bounds()
    minimum=b.origin-b.box_extent;maximum=b.origin+b.box_extent
    pad=CROWN_GAP_CM/(2*6.4)
    bs.set_editor_property('bounds_min',u.Vector(minimum.x*CROWN_PRUNE_FACTOR-pad,minimum.y*CROWN_PRUNE_FACTOR-pad,-100000))
    bs.set_editor_property('bounds_max',u.Vector(maximum.x*CROWN_PRUNE_FACTOR+pad,maximum.y*CROWN_PRUNE_FACTOR+pad,100000))
    bs.set_editor_property('description',u.Text('User approved crown overlap to retain >=60 full-size trees. Crown-quarter bounds protect trunk spacing.'))
    ss=nodes['Existing_Prototype_Tree_HISM'].get_settings()
    selector=ss.get_editor_property('mesh_selector_parameters');entries=selector.get_editor_property('mesh_entries')
    entry=entries[0];desc=entry.get_editor_property('descriptor');desc.set_editor_property('static_mesh',mesh)
    entry.set_editor_property('descriptor',desc);selector.set_editor_property('mesh_entries',[entry])
    director=next(a for a in ACTORS.get_all_level_actors() if isinstance(a,u.GamePlatformPCGWorldDirector))
    actors={a.get_actor_label():a for a in ACTORS.get_all_level_actors()}
    # 单一空间域做全局裁剪；仅清理由本脚本标记的旧分区生成器。
    for label in ('West','East','South'):
        a=actors.get('HillsideForest_'+label)
        if a:
            assert 'SSLM.HillsideForest.ReviewOnly' in [str(t) for t in a.tags]
            assert director.unregister_participant(a)
            a.get_editor_property('pcg_component').cleanup(True)
            assert ACTORS.destroy_actor(a)
    a=actors.get('HillsideForest_Canopy') or actors['HillsideForest_North']
    a.set_actor_label('HillsideForest_Canopy');a.set_actor_location(u.Vector(0,0,0),False,False)
    box_bounds(a,(23500,24500,15000))
    # 河岸可有自然冠层悬挑；根部仍遵守原道路、建筑、手工村庄与出生点缓冲。
    radius=0.0
    actors['ReviewFixture_Road_5mClearance'].set_editor_property('carve_half_width_cm',1000+radius)
    box_bounds(actors['ReviewFixture_Building_20mClearance'],(2800+radius,2600+radius,15000))
    # 用户新增中央树木：中央整块禁种改为实际手工对象保护；湖面和出生点各自独立排除。
    manual=actors['StaticMeshActor'];origin,extent=manual.get_actor_bounds(False)
    lock=actors['Protected_ManualVillageLake']
    lock.set_actor_location(u.Vector(origin.x,origin.y,0),False,False)
    box_bounds(lock,(extent.x+1500,extent.y+1500,15000))
    for label,actor in actors.items():
        if label in ('Protected_Village_PlayerStart','Protected_VillagePlayerStartSecondary'):
            box_bounds(actor,(2200+radius,2200+radius,15000))
    masks,error=director.collect_spatial_masks();assert not error
    nodes['WorldDirector_Road_Building_ManualLocks'].get_settings().set_editor_property('masks',masks)
    assert u.EditorAssetLibrary.save_loaded_asset(g,False)
    plan=director.build_static_execution_plan();assert plan and not plan[1]
    regenerate()


def audit(name='Generated'):
    guard()
    setup=json.loads((OUT/'Setup.json').read_text(encoding='utf-8'))
    by_label={a.get_actor_label():a for a in ACTORS.get_all_level_actors()}
    assert all(by_label[k].get_actor_transform().export_text()==v for k,v in setup['originalActorTransforms'].items())
    director=by_label['HillsideForest_WorldDirector']
    assert director.validate_participant_set()==''
    plan=director.build_static_execution_plan()
    assert plan is not None and plan[1]==''
    transforms=[];groups=[];violations=[];slopes=[];height_errors=[];crowns=[];materials=set();trunks=[];central=0
    masks,error=director.collect_spatial_masks();assert not error
    def excluded(mask,x,y):
        vertices=[(v.x,v.y) for v in mask.get_editor_property('vertices')]
        closed=mask.get_editor_property('closed');inside=False
        if closed and mask.get_editor_property('fill_interior'):
            for k,a in enumerate(vertices):
                b=vertices[k-1]
                if (a[1]>y)!=(b[1]>y) and x<(b[0]-a[0])*(y-a[1])/(b[1]-a[1])+a[0]:inside=not inside
        if inside:return True
        for a,b in zip(vertices,vertices[1:]+(vertices[:1] if closed else [])):
            dx=b[0]-a[0];dy=b[1]-a[1]
            t=max(0,min(1,((x-a[0])*dx+(y-a[1])*dy)/(dx*dx+dy*dy))) if dx or dy else 0
            if math.hypot(x-a[0]-t*dx,y-a[1]-t*dy)<=mask.get_editor_property('half_width_cm'):return True
        return False
    for a in ACTORS.get_all_level_actors():
        if not a.get_actor_label().startswith('HillsideForest_') or not isinstance(a,u.GamePlatformPCGActorBase):continue
        c=a.get_editor_property('pcg_component')
        assert c.get_editor_property('generated'), a.get_actor_label()
        count=0
        for ism in a.get_components_by_class(u.InstancedStaticMeshComponent):
            assert ism.get_collision_enabled()==u.CollisionEnabled.NO_COLLISION
            mesh=ism.get_editor_property('static_mesh');b=mesh.get_bounds()
            lo=b.origin-b.box_extent;hi=b.origin+b.box_extent
            for slot in range(4):
                mat=ism.get_material(slot);assert mat is not None
                materials.add(mat.get_path_name())
            for i in range(ism.get_instance_count()):
                t=ism.get_instance_transform(i,True);p=t.translation
                q=t.rotation
                if 2.0*(q.x*q.x+q.y*q.y)>0.00001:violations.append(['notUpright',i])
                if not 6.39999<=t.scale3d.x<=9.60001:violations.append(['scale',i,t.scale3d.x])
                pos,n=trace(p.x,p.y)
                slope=math.degrees(math.acos(max(-1.0,min(1.0,n[2]))))
                slopes.append(slope);height_errors.append(abs(p.z-pos[2]))
                # collision height/normal and PCG native landscape sampling may differ slightly at triangle edges.
                if slope>MAX_SLOPE+0.01:violations.append(['slope',p.x,p.y,slope])
                if abs(p.z-pos[2])>12:violations.append(['height',p.x,p.y,p.z-pos[2]])
                corners=[u.MathLibrary.transform_location(t,u.Vector(x,y,0)) for x in (lo.x,hi.x) for y in (lo.y,hi.y)]
                crown=(min(v.x for v in corners),min(v.y for v in corners),max(v.x for v in corners),max(v.y for v in corners))
                crowns.append(crown)
                trunks.append((p.x,p.y,32*t.scale3d.x))
                if abs(p.x)<10500 and abs(p.y)<10500:central+=1
                for mask in masks:
                    if excluded(mask,p.x,p.y):violations.append(['spatialMask',str(mask.domain_id),p.x,p.y])
                if abs(p.x-3500)<=2800 and abs(p.y-19500)<=2600:violations.append(['building',p.x,p.y])
                for start,end in zip(ROAD,ROAD[1:]):
                    vx=end[0]-start[0];vy=end[1]-start[1]
                    q=max(0,min(1,((p.x-start[0])*vx+(p.y-start[1])*vy)/(vx*vx+vy*vy)))
                    if math.hypot(p.x-start[0]-q*vx,p.y-start[1]-q*vy)<999:violations.append(['road',p.x,p.y])
                transforms.append(t.export_text());count+=1
        groups.append({'actor':a.get_actor_label(),'instances':count,'seed':c.get_editor_property('seed')})
    minimum_gap=float('inf');crown_overlaps=0;trunk_gap=float('inf')
    for i,first in enumerate(crowns):
        for j,second in enumerate(crowns[:i]):
            dx=max(first[0]-second[2],second[0]-first[2],0)
            dy=max(first[1]-second[3],second[1]-first[3],0)
            gap=math.hypot(dx,dy);minimum_gap=min(minimum_gap,gap)
            if gap<.1:crown_overlaps+=1
            a=trunks[i];b=trunks[j]
            clear=math.hypot(a[0]-b[0],a[1]-b[1])-a[2]-b[2]
            trunk_gap=min(trunk_gap,clear)
            if clear<100:violations.append(['trunkGap',i,j,clear])
    if len(transforms)<MIN_TREES:violations.append(['belowMinimumTreeCount',len(transforms),MIN_TREES])
    if not central:violations.append(['noCentralTrees'])
    fingerprint=hashlib.sha256('\n'.join(sorted(transforms)).encode()).hexdigest()
    report={'stage':name,'groups':groups,'instances':len(transforms),'fingerprint':fingerprint,
            'maxCollisionSlopeDegrees':max(slopes,default=0),'maxGroundErrorCm':max(height_errors,default=0),
            'directorPlanParticipants':len(plan[0]),'crownAreaFactor':CROWN_AREA,
            'minimumCrownAABBGapCm':minimum_gap,'crownAABBOverlapPairsAllowed':crown_overlaps,
            'minimumTrunkGapCm':trunk_gap,'centralTrees':central,'materials':sorted(materials),
            'violations':violations,'originalActorsUnchanged':True,'formalSHA256Unchanged':True}
    (OUT/(name+'.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report))
    assert transforms and not violations, '生成结果为空或几何规则未通过'
    return report


def review_water_material(kind):
    """原生Water静态水面分支：不依赖地形雕刻或WaterInfo顶点映射。"""
    path=BASE+'/Water/MI_DBA_Review'+kind+'_NoCarve'
    if u.EditorAssetLibrary.does_asset_exist(path):
        material=u.load_asset(path)
    else:
        material=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],BASE+'/Water',
            u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    lib=u.MaterialEditingLibrary
    lib.set_material_instance_parent(material,u.load_asset('/DBAWorldPack_Village/Water/Materials/MI_DBA_Village_'+kind))
    switches={'Enable Water VS Mapping':False,'UseWaterInfoTexture':False,'UseScalarWaterBodyIndex':True,
        'UseFixedZ':False,'UseFixedWaterDepth':True,'UseFixedVelocity':True,
        'DisableDynamicBoundsClip':True,'Enable River':kind=='River','Enable Waves':False}
    for name,value in switches.items():
        lib.set_material_instance_static_switch_parameter_value(material,name,value,update_material_instance=False)
    lib.update_material_instance(material)
    for name,value in switches.items():
        assert lib.get_material_instance_static_switch_parameter_value(material,name)==value
    assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    return material


def author_water():
    """复用项目河湖蓝图的NoCarve派生，只在现有洼地添加可审核水面。"""
    guard()
    # 与本次放置同一调用内取基线，不能用人工编辑前的历史采样判定水体雕刻。
    samples=[[x,y,trace(x,y)[0][2]] for y in range(-22000,22001,2000) for x in range(-22000,22001,2000)]
    (OUT/'TerrainBeforeWaterPlacement.json').write_text(json.dumps(samples),encoding='utf-8')
    actors={a.get_actor_label():a for a in ACTORS.get_all_level_actors()}
    zone=actors.get('HillsideWater_Zone')
    if zone is None:
        assert not any(isinstance(a,u.WaterZone) for a in actors.values())
        zone=spawn(u.WaterZone,'HillsideWater_Zone',(0,0),0)
    zone.set_editor_property('zone_extent',u.Vector2D(52000,52000))
    zone.set_editor_property('enable_local_only_tessellation',False)
    zone.set_editor_property('render_target_resolution',u.IntPoint(512,512))
    watermesh=zone.get_editor_property('water_mesh')
    watermesh.set_editor_property('tile_size',1500.0)
    watermesh.set_editor_property('tessellation_factor',4)
    watermesh.set_editor_property('far_distance_mesh_extent',0.0)
    lake_z=-80.0
    # 水面多边形略延入干岸，由真实地形遮挡；湖底和高程保持原样。
    lake_points=[(-5500+5700*math.cos(i*math.tau/24),-1800+9800*math.sin(i*math.tau/24),lake_z) for i in range(24)]
    # 南侧实测低谷向中央湖盆流下，尾段进入湖面，避免悬空的平直水带。
    river_anchors=[(-9750,-19500),(-9500,-18500),(-9000,-18000),(-9000,-17000),
              (-8750,-16000),(-9000,-15000),(-9250,-14000),(-9500,-13000),(-9500,-11500),(-9000,-9000)]
    # 每2米沿原曲线采样地面，避免长样条穿入现有坡面；末段实际伸入湖泊多边形。
    river_xy=[]
    for start,end in zip(river_anchors,river_anchors[1:]):
        steps=math.ceil(math.hypot(end[0]-start[0],end[1]-start[1])/200)
        for i in range(steps):
            t=i/steps;river_xy.append((start[0]+(end[0]-start[0])*t,start[1]+(end[1]-start[1])*t))
    river_xy.append(river_anchors[-1])
    river_points=[]
    for i,(x,y) in enumerate(river_xy):
        z=max(lake_z,trace(x,y)[0][2]+100)
        river_points.append((x,y,z))
    assert all(b[2]<=a[2]+1 for a,b in zip(river_points,river_points[1:])),river_points
    results=[]
    for kind,points in [('Lake',lake_points),('River',river_points)]:
        path=BASE+'/Water/BP_DBA_Review'+kind+'_NoCarve'
        if not u.EditorAssetLibrary.does_asset_exist(path):
            factory=u.BlueprintFactory()
            factory.set_editor_property('parent_class',u.EditorAssetLibrary.load_blueprint_class('/DBAWorldPack_Village/Water/Blueprints/BP_DBA_Village'+kind))
            bp=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],BASE+'/Water',u.Blueprint,factory)
            u.BlueprintEditorLibrary.compile_blueprint(bp)
            cls=u.EditorAssetLibrary.load_blueprint_class(path)
            default=u.get_default_object(cls).get_component_by_class(u.WaterBodyComponent)
            assert default.get_path_name().startswith(path)
            default.set_editor_property('affects_landscape',False)
            default.set_editor_property('can_ever_affect_navigation',False)
            u.BlueprintEditorLibrary.compile_blueprint(bp)
            assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
            reloaded,error=u.EditorLoadingAndSavingUtils.reload_packages([bp.get_outer()],u.ReloadPackagesInteractionMode.ASSUME_NEGATIVE)
            assert reloaded and not str(error)
        cls=u.EditorAssetLibrary.load_blueprint_class(path)
        assert cls
        cdo=u.get_default_object(cls).get_component_by_class(u.WaterBodyComponent)
        assert not cdo.get_editor_property('affects_landscape'),'必须先重载核实NoCarve蓝图'
        actor=actors.get('HillsideWater_'+kind) or spawn(cls,'HillsideWater_'+kind,(0,0),0)
        if kind=='Lake':actor.set_actor_location(u.Vector(0,0,lake_z),False,False)
        comp=actor.get_component_by_class(u.WaterBodyComponent)
        assert not comp.get_editor_property('affects_landscape')
        comp.set_water_zone_override(zone)
        material=review_water_material(kind)
        comp.set_water_material(material)
        comp.set_water_static_mesh_material(material)
        # UE5.8 WaterBody PostLoad会清空旧WaterMeshOverride；使用正式原生静态网格生成接口。
        comp.set_editor_property('water_mesh_override',None)
        settings=comp.get_editor_property('static_mesh_settings')
        settings.set_editor_property('enable_water_body_static_mesh',True)
        settings.set_editor_property('section_water_body_static_mesh',False)
        comp.set_editor_property('static_mesh_settings',settings)
        comp.set_editor_property('fixed_water_depth',1500.0 if kind=='Lake' else 150.0)
        comp.set_water_info_material(u.load_asset('/Water/Materials/WaterInfo/DrawWaterInfo'))
        comp.set_editor_property('can_ever_affect_navigation',False)
        comp.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property('generate_overlap_events',False)
        spline=comp.get_water_spline()
        spline.clear_spline_points(False)
        for p in points:spline.add_spline_point(u.Vector(*p),u.SplineCoordinateSpace.WORLD,False)
        spline.set_closed_loop(kind=='Lake',False)
        for i in range(len(points)):spline.set_spline_point_type(i,u.SplinePointType.CURVE_CLAMPED if kind=='Lake' else u.SplinePointType.LINEAR,False)
        spline.update_spline();spline.k2_synchronize_and_broadcast_data_change()
        if kind=='River':
            for i in range(len(points)):
                comp.set_river_width_at_spline_input_key(float(i),350.0 if i==0 else 1100.0)
                comp.set_river_depth_at_spline_input_key(float(i),150.0)
                comp.set_water_velocity_at_spline_input_key(float(i),80.0)
            spline.k2_synchronize_and_broadcast_data_change()
        # 样条通知更新WaterInfo；显式重建持久静态水面，避免仍显示旧样条网格。
        comp.set_water_body_static_mesh_enabled(False)
        comp.set_water_body_static_mesh_enabled(True)
        for primitive in actor.get_components_by_class(u.PrimitiveComponent):
            primitive.set_editor_property('can_ever_affect_navigation',False)
            primitive.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        results.append({'kind':kind,'actor':actor.get_path_name(),'sourceBlueprint':path,
            'material':comp.get_water_material().get_path_name(),'affectsLandscape':False,'points':points,
            'renderMode':'NativeWaterBodyStaticMesh','navigation':False,'collision':'NoCollision'})
    director=actors['HillsideForest_WorldDirector']
    lake_mask=actors.get('Protected_ReviewLake') or spawn(u.GamePlatformPCGExclusionActor,'Protected_ReviewLake',(-5500,-1800),0)
    lake_mask.set_editor_property('domain_id',u.Name('Water.Lake'))
    box_bounds(lake_mask,(6200,10300,15000))
    river_mask=actors.get('Protected_ReviewRiver') or spawn(u.GamePlatformPCGSplineActor,'Protected_ReviewRiver',(0,0),0)
    river_mask.set_editor_property('domain_id',u.Name('Water.River'))
    river_mask.set_editor_property('carve_half_width_cm',1500.0)
    spline=river_mask.get_editor_property('spline');spline.clear_spline_points(False)
    for p in river_points:spline.add_spline_point(u.Vector(*p),u.SplineCoordinateSpace.WORLD,False)
    for i in range(len(river_points)):spline.set_spline_point_type(i,u.SplinePointType.LINEAR,False)
    spline.update_spline()
    for mask in (lake_mask,river_mask):
        if mask.get_actor_label() not in actors:assert director.register_participant(mask)
    bind_spatial_sources()
    (OUT/'WaterLayout.json').write_text(json.dumps({'waterZone':zone.get_path_name(),'lakeSurfaceZCm':lake_z,
        'riverFlowsDownhill':True,'waters':results,'terrainCarving':False,'navigationChanges':False},indent=2),encoding='utf-8')
    # 实测原有地形样本与水体添加前保持一致，不把蓝图默认值当作高度证据。
    errors=[abs(trace(x,y)[0][2]-z) for x,y,z in samples]
    assert max(errors)<.001,max(errors)
    (OUT/'TerrainAfterWaterPlacement.json').write_text(json.dumps({'samples':len(samples),'maxDeltaCm':max(errors),'unchanged':True}),encoding='utf-8')
    print('NATIVE_WATER_CREATED',len(results),'terrainSamplesUnchanged',len(samples),'maxDeltaCm',max(errors))
