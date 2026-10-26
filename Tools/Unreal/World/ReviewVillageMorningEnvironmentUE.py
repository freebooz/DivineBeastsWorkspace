"""SSLM 清晨光雾隔离审核：仅由直接 Monolith 在 UE5.8 Editor 调用。

正式地图只读；参数清单是编辑器作者输入，不冒充运行时 DataAsset。
不添加运行时环境管理器，不修改任何服务器天气/战斗合同。
"""
import hashlib
import json
import math
import os
from pathlib import Path
import statistics
import re
import unreal as u

ROOT = Path(u.Paths.project_dir()).resolve().parent
# 历史审核原样保留；P5只使用新的隔离目录、证据目录和参数版本。
LEGACY_OUT = ROOT / 'Saved/Validation/VillageMorningEnvironment20261025'
P5_OUT = ROOT / 'Saved/Validation/VillageMorningEnvironmentP5_20261026'
FORMAL = '/DBAWorldPack_Village/Maps/L_Village_Start'
LEGACY_REVIEW = '/Game/Development/Weather/VillageMorning'
P5_REVIEW = '/Game/Development/Weather/VillageMorningP5'
LEGACY_PRESETS = Path(__file__).with_name('VillageMorningPresets.json')
P5_PRESETS = Path(__file__).with_name('VillageMorningP5Presets.json')
OUT, REVIEW, PRESET_FILE = LEGACY_OUT, LEGACY_REVIEW, LEGACY_PRESETS

def select_review_scope(scope):
    """在编辑器调用每一阶段前显式选择审核范围；缺省保持历史V1不变。"""
    global OUT, REVIEW, PRESET_FILE
    assert scope in ('legacy', 'p5'), '非法审核范围'
    if scope == 'p5':
        OUT, REVIEW, PRESET_FILE = P5_OUT, P5_REVIEW, P5_PRESETS
    else:
        OUT, REVIEW, PRESET_FILE = LEGACY_OUT, LEGACY_REVIEW, LEGACY_PRESETS
    assert PRESET_FILE.is_file(), '所选审核范围参数清单不存在'
    return {'scope': scope, 'review_map_root': REVIEW, 'evidence_dir': str(OUT), 'presets_file': str(PRESET_FILE)}

ENV = ('DirectionalLight', 'SkyLight', 'SkyAtmosphere', 'VolumetricCloud', 'ExponentialHeightFog', 'LocalFogVolume', 'PostProcessVolume')
PROPS = {
 'DirectionalLightComponent': ['intensity','light_color','use_temperature','temperature','light_source_angle','cast_shadows','cast_volumetric_shadow','volumetric_scattering_intensity','atmosphere_sun_light','atmosphere_sun_light_index','mobility'],
 'SkyLightComponent': ['intensity','light_color','real_time_capture','source_type','cast_shadows','volumetric_scattering_intensity','mobility'],
 'ExponentialHeightFogComponent': ['fog_density','fog_height_falloff','fog_inscattering_luminance','start_distance','fog_cutoff_distance','fog_max_opacity','second_fog_data','enable_volumetric_fog','volumetric_fog_scattering_distribution','volumetric_fog_albedo','volumetric_fog_emissive','volumetric_fog_extinction_scale','volumetric_fog_distance','volumetric_fog_start_distance','volumetric_fog_near_fade_in_distance','volumetric_fog_static_lighting_scattering_intensity'],
 'LocalFogVolumeComponent': ['radial_fog_extinction','height_fog_extinction','height_fog_falloff','height_fog_offset','fog_phase_g','fog_albedo','fog_emissive','fog_start_distance'],
 'SkyAtmosphereComponent': ['rayleigh_scattering_scale','rayleigh_scattering','mie_scattering_scale','mie_scattering','mie_absorption_scale','mie_absorption','mie_anisotropy','aerial_pespective_view_distance_scale','height_fog_contribution','sky_luminance_factor'],
 'VolumetricCloudComponent': ['layer_bottom_altitude','layer_height','tracing_start_max_distance','tracing_max_distance','material'],
}

def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/name).write_text(json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True),encoding='utf-8')

def serial(v):
    if v is None or isinstance(v,(bool,int,float,str)): return v
    if isinstance(v,u.Object): return v.get_path_name()
    if hasattr(v,'export_text'): return v.export_text()
    return str(v)

def xyz(v): return [v.x,v.y,v.z]

def trans(t):
    return {'location':xyz(t.translation),'rotation':[t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w],'scale':xyz(t.scale3d)}

def actors(): return list(u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors())

def world():
    ed=u.get_editor_subsystem(u.UnrealEditorSubsystem)
    assert not ed.get_game_world(), 'PIE运行中拒绝作者操作'
    assert u.SystemLibrary.get_engine_version().startswith('5.8.'), '只允许锁定UE5.8'
    assert Path(u.Paths.project_dir()).resolve() == ROOT/'Game'
    return ed.get_editor_world()

def idle():
    w=world()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(), '存在未保存地图；保留现场并停止'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '存在未保存内容；保留现场并停止'
    return w

def package_file(path):
    if path.startswith('/Game/'): return ROOT/'Game/Content'/(path[6:]+'.umap')
    assert path==FORMAL
    return ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'

def formal_hash(): return hashlib.sha256(package_file(FORMAL).read_bytes()).hexdigest()

def read_props(obj, names):
    good, missing={},{}
    for n in names:
        try: good[n]=serial(obj.get_editor_property(n))
        except Exception as e: missing[n]=str(e)
    return {'values':good,'unavailable':missing}

def geometry():
    rows=[]
    for a in actors():
        if a.get_class().get_name() in ENV or a.get_actor_label().startswith('MorningReview_'): continue
        row={'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_name(),'transform':trans(a.get_actor_transform()),'components':[]}
        for c in a.get_components_by_class(u.StaticMeshComponent):
            m=c.get_editor_property('static_mesh')
            if not m: continue
            r={'name':c.get_name(),'mesh':m.get_path_name(),'transform':trans(c.get_world_transform()),'materials':[c.get_material(i).get_path_name() if c.get_material(i) else None for i in range(c.get_num_materials())]}
            if isinstance(c,u.InstancedStaticMeshComponent):
                ts=[trans(c.get_instance_transform(i,world_space=True)) for i in range(c.get_instance_count())]
                r['instance_count']=len(ts)
                r['instances_sha256']=hashlib.sha256(json.dumps(ts,sort_keys=True).encode()).hexdigest()
            row['components'].append(r)
        rows.append(row)
    return sorted(rows,key=lambda x:x['name'])

def environment():
    rows=[]
    for a in actors():
        if a.get_class().get_name() not in ENV: continue
        r={'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_name(),'transform':trans(a.get_actor_transform()),'components':[]}
        for c in a.get_components_by_class(u.ActorComponent):
            cn=c.get_class().get_name()
            if cn in PROPS: r['components'].append({'name':c.get_name(),'class':cn,**read_props(c,PROPS[cn])})
        if isinstance(a,u.PostProcessVolume):
            r['properties']=read_props(a,['unbound','enabled','priority','blend_weight','blend_radius'])
            pp=a.get_editor_property('settings')
            r['post_process']=read_props(pp,['auto_exposure_method','auto_exposure_bias','auto_exposure_apply_physical_camera_exposure','override_auto_exposure_bias','white_temp','white_tint','color_saturation','color_contrast','color_gain','bloom_intensity','ambient_occlusion_intensity','dynamic_global_illumination_method','reflection_method','film_slope','film_toe','film_shoulder'])
            r['settings_export']=pp.export_text()
        rows.append(r)
    return sorted(rows,key=lambda x:x['name'])

def audit(name='P0_Baseline.json'):
    w=idle()
    if REVIEW == P5_REVIEW and name == 'P0_Baseline.json':
        # P5必须重新从当前正式地图读取基线；绝不能用旧SHA跳过保护。
        assert w.get_path_name().split('.')[0] == FORMAL, 'P5基线必须从正式地图只读采集'
        assert not (OUT/name).exists(), 'P5基线证据已存在；不得直接覆盖'
        pps=[a for a in actors() if isinstance(a,u.PostProcessVolume)]
        assert len(pps)==1, 'P5要求确认唯一的正式后期处理体积'
        pp=pps[0].get_editor_property('settings')
        expected=presets()['shared']['fixed_exposure_bias']
        assert bool(pp.get_editor_property('override_auto_exposure_bias')), '正式曝光补偿未启用，拒绝跨版本比较'
        assert abs(pp.get_editor_property('auto_exposure_bias')-expected)<.001, '正式曝光值不符合P5候选；拒绝复用旧基线'
    inventory=[]; plants=[]
    for a in actors():
        center,extent=a.get_actor_bounds(False)
        inventory.append({'name':a.get_name(),'label':a.get_actor_label(),'class':a.get_class().get_name(),'location':xyz(a.get_actor_location()),'bounds_center':xyz(center),'bounds_extent':xyz(extent)})
        for c in a.get_components_by_class(u.InstancedStaticMeshComponent):
            m=c.get_editor_property('static_mesh')
            if not m: continue
            for i in range(c.get_instance_count()):
                t=c.get_instance_transform(i,world_space=True)
                plants.append({'component':c.get_name(),'mesh':m.get_path_name(),'location':xyz(t.translation),'materials':[c.get_material(k).get_path_name() if c.get_material(k) else None for k in range(c.get_num_materials())]})
    cvars={n:u.SystemLibrary.get_console_variable_int_value(n) for n in ['r.DynamicGlobalIlluminationMethod','r.ReflectionMethod','r.GenerateMeshDistanceFields','r.Shadow.Virtual.Enable','r.VolumetricFog','r.VolumetricFog.GridPixelSize','r.VolumetricFog.GridSizeZ','r.LocalFogVolume','r.LocalFogVolume.RenderIntoVolumetricFog','sg.ShadowQuality','r.VSync']}
    ws=w.get_world_settings()
    report={'world':w.get_path_name(),'engine':u.SystemLibrary.get_engine_version(),'pid':os.getpid(),'formal_sha256':formal_hash(),'review_scope':'p5' if REVIEW==P5_REVIEW else 'legacy','environment':environment(),'geometry':geometry(),'inventory':inventory,'instanced_plants':plants,'cvars':cvars,'world_settings':read_props(ws,['default_game_mode','enable_world_partition','enable_world_composition']),'streaming':read_props(w,['streaming_levels','world_partition'])}
    write(name,report)
    print(json.dumps({'saved':str(OUT/name),'actors':len(inventory),'plants':len(plants),'environment':len(report['environment']),'cvars':cvars}))
    return report

def check_formal():
    baseline=json.loads((OUT/'P0_Baseline.json').read_text(encoding='utf-8'))
    if REVIEW == P5_REVIEW:
        assert baseline.get('review_scope') == 'p5', 'P5拒绝使用历史旧基线'
    assert formal_hash()==baseline['formal_sha256'], '正式关卡磁盘输入变化；停止并调查'
    return baseline

def geometry_equal(actual, expected):
    # Water生成的MID计数会随每次加载递增；只归一化这些已核实的瞬态名称。
    # 地图内生成网格随SaveAs更换外层World路径，仍严格核对组件/实例/变换/外部资产。
    def norm(v):
        if isinstance(v,list): return [norm(x) for x in v]
        if isinstance(v,dict): return {k:norm(x) for k,x in v.items()}
        if isinstance(v,str):
            if '.LandscapeNaniteMesh_' in v and (v.startswith(FORMAL+'.') or v.startswith(REVIEW+'/Maps/')):
                v='REVIEW_WORLD.'+v.split('.',1)[1]
            if ':PersistentLevel.' in v and (v.startswith(FORMAL) or v.startswith(REVIEW+'/Maps/')):
                v='REVIEW_WORLD:PersistentLevel.'+v.split(':PersistentLevel.',1)[1]
            return re.sub(r'(/Engine/Transient\.(?:WaterInfoMID|WaterMID|WaterLODMID))_\d+$',r'\1#',v)
        return v
    return norm(actual)==norm(expected)

def make_copy(name):
    w=idle(); baseline=check_formal()
    assert w.get_path_name().split('.')[0]==FORMAL, '必须从正式只读基线创建每一份副本'
    assert geometry_equal(geometry(),baseline['geometry']), '正式场景几何发生变化'
    dest=REVIEW+'/Maps/'+name
    assert not package_file(dest).exists(), '副本已经存在；拒绝覆盖'
    assert u.EditorLoadingAndSavingUtils.save_map(w,dest), '引擎SaveAs失败'
    assert formal_hash()==baseline['formal_sha256'], 'SaveAs不应写回正式地图'
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(dest), 'SaveAs后显式加载隔离副本失败'
    assert geometry_equal(geometry(),baseline['geometry']), '副本几何/出生点不等价'
    print(json.dumps({'copy_saved':dest,'formal_unchanged':True,'geometry_equal':True}))
    return dest

def load_formal_and_copy(name):
    idle(); check_formal()
    assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(FORMAL), '正式基线读取失败'
    return make_copy(name)

def save_review(name):
    w=world(); check_formal()
    path=w.get_path_name().split('.')[0]
    assert path.startswith(REVIEW+'/Maps/'), '禁止写入正式关卡或非审核地图'
    assert geometry_equal(geometry(),json.loads((OUT/'P0_Baseline.json').read_text(encoding='utf-8'))['geometry']), '非环境场景变化；拒绝保存'
    assert u.EditorLoadingAndSavingUtils.save_map(w,path)
    write(name,{'world':path,'environment':environment(),'formal_sha256':formal_hash(),'geometry_equal':True,'pid':os.getpid()})
    print('REVIEW_MAP_SAVED',path)

def srgb(rgb):
    def lin(c):
        c=c/255.0
        return c/12.92 if c<=.04045 else ((c+.055)/1.055)**2.4
    return u.LinearColor(*(lin(c) for c in rgb),1.0)

def presets():
    return json.loads(PRESET_FILE.read_text(encoding='utf-8'))

def spatial_layout():
    """使用真实水体边界、茅屋和桃花树实例确定区域，不将旧固定雾坐标当地形。"""
    b=check_formal()
    lake=next(a for a in b['inventory'] if a['label']=='HillsideWater_Lake')
    hut=next(a for a in b['inventory'] if a['label']=='Village_ThatchHut_Lakeside')
    trees=[p for p in b['instanced_plants'] if 'WideCrown' in p['mesh']]
    blossoms=[p for p in trees if any(m and 'Blossom' in m for m in p['materials'])]
    assert blossoms, '未找到真实桃花材质树实例'
    # 选择靠近可通行低地且不在湖内的真实桃树；位置直接来自已保存实例。
    peach=min(blossoms,key=lambda p:(p['location'][0]-2000)**2+(p['location'][1]-6500)**2)['location']
    low=min(trees,key=lambda p:p['location'][2])['location']
    lx,ly=lake['bounds_center'][:2]; water_z=lake['location'][2]
    hx,hy,hz=hut['location']
    start=next(a['location'] for a in b['inventory'] if a['label']=='Village_PlayerStart')
    layout={
      'local_fog':[
        {'actor':'LocalFogVolume_0','region':'actual_lake_surface','location':[lx,ly,water_z+100.0],'scale':min(lake['bounds_extent'][:2])/500.0},
        {'actor':'LocalFogVolume_1','region':'actual_peach_tree_base','location':[peach[0],peach[1],peach[2]+80.0],'scale':5.0},
        {'actor':'LocalFogVolume_2','region':'actual_low_woodland','location':[low[0],low[1],low[2]+100.0],'scale':7.0}],
      'cameras':{
        'Entry':{'location':[start[0]-1400,start[1]-1000,start[2]+900],'target':[peach[0],peach[1],peach[2]+550]},
        'PeachWoodland':{'location':[peach[0]-2400,peach[1]-1600,peach[2]+1800],'target':[peach[0]+1000,peach[1]+1000,peach[2]+500]},
        'LakeShore':{'location':[hx+2800,hy+900,hz+1700],'target':[lx,ly+3000,water_z+350]},
        'LakesideHut':{'location':[hx+2200,hy-2600,hz+1200],'target':[hx,hy,hz+350]},
        'LowWoodland':{'location':[low[0]+1600,low[1]-2200,low[2]+1700],'target':[low[0],low[1]+2400,low[2]+550]}
      },
      'missing':['confirmed dock asset','built village center beyond the existing lakeside hut','waterfall/Niagara waterfall'],
      'sources':{'lake':lake['name'],'hut':hut['name'],'peach_instance':peach,'low_instance':low},
      'fov':75.0,'resolution':[1280,720]
    }
    write('SpatialLayout.json',layout)
    return layout

def apply_preset(key):
    w=idle(); b=check_formal(); path=w.get_path_name().split('.')[0]
    assert path==REVIEW+'/Maps/L_'+key, '预设只允许写对应的受保护副本'
    cfg=presets(); p=cfg['presets'][key]; shared=cfg['shared']; layout=spatial_layout()
    all_actors=actors()
    sun=next(a for a in all_actors if isinstance(a,u.DirectionalLight))
    sky=next(a for a in all_actors if isinstance(a,u.SkyLight))
    fog=next(a for a in all_actors if isinstance(a,u.ExponentialHeightFog))
    assert sum(isinstance(a,u.DirectionalLight) for a in all_actors)==1
    c=sun.get_component_by_class(u.DirectionalLightComponent)
    c.set_intensity(shared['sun_intensity_lux'])
    c.set_editor_property('light_source_angle',shared['sun_source_angle_deg'])
    c.set_editor_property('light_color',u.Color(255,255,255,255))
    c.set_editor_property('use_temperature',True)
    c.set_editor_property('temperature',shared['sun_temperature_kelvin'])
    c.set_editor_property('volumetric_scattering_intensity',.7)
    q=next(a for a in b['environment'] if a['class']=='DirectionalLight')['transform']['rotation']
    original=u.Quat(x=q[0],y=q[1],z=q[2],w=q[3]).rotator()
    # UE Python Rotator的位置参数顺序不能视为C++的Pitch/Yaw/Roll；显式命名并真实回读。
    sun.set_actor_rotation(u.Rotator(pitch=-shared['sun_altitude_deg'],yaw=original.yaw,roll=original.roll),False)
    actual=sun.get_actor_rotation()
    assert abs(actual.pitch+shared['sun_altitude_deg'])<.001 and abs(actual.yaw-original.yaw)<.001 and abs(actual.roll-original.roll)<.001, '太阳高度角/朝向回读失败'
    sky.get_component_by_class(u.SkyLightComponent).set_intensity(shared['sky_intensity'])
    c=fog.get_component_by_class(u.ExponentialHeightFogComponent)
    rgb=shared['volumetric_albedo_srgb']
    albedo=u.Color(r=rgb[0],g=rgb[1],b=rgb[2],a=255)
    assert [albedo.r,albedo.g,albedo.b]==rgb, '颜色通道必须真实回读；禁止依赖FColor的BGRA位置顺序'
    settings={'fog_density':p['fog_density'],'fog_height_falloff':p['height_falloff'],'start_distance':p['start_cm'],'fog_max_opacity':p['max_opacity'],'fog_inscattering_luminance':srgb(shared['fog_srgb']),'enable_volumetric_fog':p['volumetric_enabled'],'volumetric_fog_albedo':albedo,'volumetric_fog_emissive':u.LinearColor(0,0,0,0),'volumetric_fog_distance':shared['volumetric_distance_cm'],'volumetric_fog_start_distance':600.0,'volumetric_fog_near_fade_in_distance':400.0,'volumetric_fog_scattering_distribution':shared['volumetric_scattering'],'volumetric_fog_extinction_scale':shared['volumetric_extinction_scale']}
    for prop,v in settings.items(): c.set_editor_property(prop,v)
    second=c.get_editor_property('second_fog_data')
    for prop,v in {'fog_density':p['second_density'],'fog_height_falloff':.5,'fog_height_offset':layout['local_fog'][0]['location'][2]}.items(): second.set_editor_property(prop,v)
    c.set_editor_property('second_fog_data',second)
    for placement in layout['local_fog']:
        a=next(x for x in all_actors if x.get_name()==placement['actor'])
        a.set_actor_location(u.Vector(*placement['location']),False,False)
        sc=placement['scale']; a.set_actor_scale3d(u.Vector(sc,sc,sc))
        comp=a.get_component_by_class(u.LocalFogVolumeComponent)
        for prop,v in {'radial_fog_extinction':p['local_radial'],'height_fog_extinction':p['local_height'],'height_fog_falloff':2000.0,'height_fog_offset':0.0,'fog_start_distance':shared['local_start_cm'],'fog_phase_g':.2,'fog_albedo':srgb(shared['volumetric_albedo_srgb']),'fog_emissive':u.LinearColor(0,0,0,0)}.items(): comp.set_editor_property(prop,v)
    for a in all_actors:
        if isinstance(a,u.PostProcessVolume):
            pp=a.get_editor_property('settings')
            assert abs(pp.get_editor_property('auto_exposure_bias')-shared['fixed_exposure_bias'])<.001
            # 保留所有原始色彩分级、白平衡、AO和泛光，不使用滤镜掩盖雾。
    assert geometry_equal(geometry(),b['geometry']), '环境作者不应改变场景或出生点'
    save_review(key+'_Saved.json')

def cold_audit():
    idle(); check_formal(); reports=[]
    # P5复用已通过的能力测试，不重复创建能力地图；只验证新基线及三个新候选。
    entries=[('L_Village_Baseline_Overcast','Baseline_Saved.json')]+[('L_'+k,k+'_Saved.json') for k in presets()['presets']]
    if REVIEW != P5_REVIEW:
        entries.insert(0,('L_Monolith_EnvironmentCapability','P1_Persisted.json'))
    for short,record in entries:
        path=REVIEW+'/Maps/'+short
        assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(path)
        saved=json.loads((OUT/record).read_text(encoding='utf-8'))
        assert environment()==saved['environment'], '独立进程重载环境值变化: '+short
        if short!='L_Monolith_EnvironmentCapability':
            assert os.getpid()!=saved['pid'], '必须是独立Editor进程'
            assert geometry_equal(geometry(),check_formal()['geometry']), '独立重载场景几何变化'
        reports.append({'map':path,'persisted_equal':True})
    write('ColdReload.json',{'passed':True,'pid':os.getpid(),'maps':reports,'formal_unchanged':True})
    print('FOUR_MAPS_P5_COLD_RELOAD_PASS' if REVIEW==P5_REVIEW else 'FIVE_MAPS_COLD_RELOAD_PASS')
