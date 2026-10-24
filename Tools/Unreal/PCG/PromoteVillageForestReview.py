"""用户2026-10-24明确批准：编辑器森林/地形同步正式Village，保留原雪链路。

仅通过Monolith内的原生UE资产API执行。先备份地图与母材质；运行资源原生迁移，
使用正式地图身份供既有登录/服务器准入链消费，不新增或绕过认证入口。
"""
import hashlib,json,math
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve().parent
OUT=ROOT/'Saved/Validation/VillageHillsideForest20261024'
REVIEW='/DBAWorldPack_Village/Development/PCG/Maps/L_DBA_Village_HillsideForest_Review'
FORMAL='/DBAWorldPack_Village/Maps/L_Village_Start'
OLD='/DBAWorldPack_Village/Development/PCG/HillsideForest'
BASE='/DBAWorldPack_Village/PCG/HillsideForest'
BRIDGE='/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'


def promote_assets():
    lib=u.MaterialEditingLibrary
    assert (OUT/'BeforeFormalPromotion/Hashes.json').exists(),'必须先备份'
    assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages(),'先保存人工编辑'
    les=u.get_editor_subsystem(u.LevelEditorSubsystem)
    assert les.load_level(REVIEW)
    if not u.EditorAssetLibrary.does_directory_exist(BASE):
        assert u.EditorAssetLibrary.rename_directory(OLD,BASE),'原生运行资源迁移失败'
    road=u.load_asset(BASE+'/Paths/M_DBA_Review_ForestPaths');bridge=u.load_asset(BRIDGE)
    core={n.get_name():n for n in lib.get_material_expressions(bridge)}
    assert not any(str(n.get_editor_property('desc')).startswith('ForestPaths.') for n in core.values()),'拒绝重复追加'
    source_nodes=lib.get_material_expressions(road)
    copies={}
    blend=None
    for n in source_nodes:
        desc=str(n.get_editor_property('desc'))
        if desc=='ForestPaths.PluginPBRBlend':blend=n
        elif desc.startswith('ForestPaths.'):
            copies[n.get_name()]=lib.duplicate_material_expression(bridge,None,n)
    assert blend and copies
    def mapped(n):return copies[n.get_name()] if n.get_name() in copies else core[n.get_name()]
    def link(a,output,b,pin):assert lib.connect_material_expressions(a,output or '',b,pin),(a,pin)
    for n in source_nodes:
        if n.get_name() not in copies:continue
        names=lib.get_material_expression_input_names(n)
        inputs=lib.get_inputs_for_material_expression(road,n)
        for name,source in zip(names,inputs):
            if source:link(mapped(source),lib.get_input_node_output_name_for_material_expression(n,source),copies[n.get_name()],name)
    inputs=dict(zip(lib.get_material_expression_input_names(blend),lib.get_inputs_for_material_expression(road,blend)))
    coverage=inputs['Coverage']
    # 共享地图的中性雪母材质不能硬引用ClientOnly Surface插件资产。
    # 使用原生材质基本运算接入同一四通道数据，保留现有雪/脚印/位移输出。
    for base,overlay,dest in [('BaseColor','OverlayColor','MaterialExpressionLinearInterpolate_1'),
            ('BaseNormal','OverlayNormal','MaterialExpressionLinearInterpolate_4'),
            ('BaseRoughness','OverlayRoughness','MaterialExpressionLinearInterpolate_3'),
            ('BaseAO','OverlayAO','MaterialExpressionLinearInterpolate_6')]:
        mix=lib.create_material_expression(bridge,u.MaterialExpressionLinearInterpolate,2300,0)
        mix.set_editor_property('desc','ForestPaths.NeutralBridge.'+base)
        for pin,source in [('A',inputs[base]),('B',inputs[overlay]),('Alpha',coverage)]:
            link(mapped(source),lib.get_input_node_output_name_for_material_expression(blend,source),mix,pin)
        final=mix
        if base=='BaseNormal':
            final=lib.create_material_expression(bridge,u.MaterialExpressionNormalize,2500,0);link(mix,'',final,'VectorInput')
        link(final,'',core[dest],'A')
    errors=lib.recompile_material(bridge);assert not errors,list(errors)
    assert u.EditorAssetLibrary.save_loaded_asset(bridge,False)
    # 纯内容子蓝图直接继承UE原生水体，不向Server带入已排除的旧视觉父蓝图。
    for kind,cls in [('Lake',u.WaterBodyLake),('River',u.WaterBodyRiver)]:
        p=BASE+'/Water/BP_DBA_Review'+kind+'_NoCarve';bp=u.load_asset(p)
        u.BlueprintEditorLibrary.reparent_blueprint(bp,cls)
        cdo=u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(p)).get_component_by_class(u.WaterBodyComponent)
        cdo.set_editor_property('affects_landscape',False);cdo.set_editor_property('can_ever_affect_navigation',False)
        u.BlueprintEditorLibrary.compile_blueprint(bp);assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    land=next(a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a,u.Landscape))
    land.set_editor_property('landscape_material',bridge)
    assert land.get_editor_property('use_dynamic_material_instance')
    assert les.save_current_level()
    print('RUNNING_FOREST_ASSETS_PROMOTED_SNOW_BRIDGE_PRESERVED')


def promote_map():
    # 一次性迁移；原生覆盖已加载World可能在写盘后报错，禁止误重试覆盖用户新编辑。
    original_hash='e1bc67e7a410d057bf2827d831ca401a77fe8ce938b60608a0069e122947f4b0'
    formal_file=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content/Maps/L_Village_Start.umap'
    if hashlib.sha256(formal_file.read_bytes()).hexdigest()!=original_hash:
        raise RuntimeError('正式地图已变更或已同步；禁止重跑一次性迁移，请先独立加载正式关卡核验')
    les=u.get_editor_subsystem(u.LevelEditorSubsystem)
    assert les.load_level(REVIEW)
    world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    actors=u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    # 沿用不可被蓝图覆盖的原生准入GameMode；降雪由既有权威天气服务驱动。
    cls=u.load_class(None,'/Script/DBAWorldsRuntime.DivineBeastsWorldGameMode')
    world.get_world_settings().set_editor_property('default_game_mode',cls)
    for actor in actors:
        if isinstance(actor,u.PlayerStart):
            p=actor.get_actor_location()
            hit=u.SystemLibrary.line_trace_single(world,u.Vector(p.x,p.y,15000),u.Vector(p.x,p.y,-10000),u.TraceTypeQuery.ECC_VISIBILITY,False,[],u.DrawDebugTrace.NONE)
            import re
            m=re.search(r'ImpactPoint=\(X=[^,]+,Y=[^,]+,Z=([^\)]+)\)',hit.export_text());assert m
            ground=float(m.group(1));actor.set_actor_location(u.Vector(p.x,p.y,max(p.z,ground+130)),False,False)
    assert les.save_current_level()
    # 真UE SaveMap原生另存为正式包；正式身份不变，登录/后端地址无需旁路。
    assert u.EditorLoadingAndSavingUtils.save_map(world,FORMAL),'正式地图原生SaveMap失败'
    assert les.load_level(FORMAL)
    print('FORMAL_WORLD_SAVED',u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name())
