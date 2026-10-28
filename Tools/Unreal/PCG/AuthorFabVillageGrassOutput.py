# -*- coding: utf-8 -*-
"""在正式地形材质添加中性 Landscape Grass 输出；由客户端租约覆盖具体草种。"""
import hashlib
import json
import shutil
import uuid
from pathlib import Path
import unreal as u

ROOT = Path(u.Paths.project_dir()).parent
MAP = '/DBAWorldPack_Village/Maps/L_Village_Start'
MASTER = '/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
DEST = '/DBAWorldPack_Village/Terrain/Grass'
TEX = DEST + '/T_DBA_VillageGrassExclusion'
EMPTY_GRASS = DEST + '/LGT_DBA_VillageGrassNeutral'
EMPTY_FLOWERS = DEST + '/LGT_DBA_VillageFlowersNeutral'
ART = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Terrain/FabGrass'
CONTENT = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content'


def need(ok, msg):
    if not ok: raise RuntimeError(msg)
    return ok


def run():
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    need(world and world.get_path_name().split('.')[0] == MAP, '必须安全交接并打开正式地图')
    need(not u.EditorLoadingAndSavingUtils.get_dirty_map_packages() and not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(), '存在未保存修改')
    meta = json.loads((ART / 'VillageGrassExclusion.json').read_text(encoding='utf-8'))
    mapfile = CONTENT / 'Maps/L_Village_Start.umap'
    source = ART / 'VillageGrassExclusion.png'
    need(hashlib.sha256(mapfile.read_bytes()).hexdigest() == meta['mapSha256'], '地图采样已过期')
    need(hashlib.sha256(source.read_bytes()).hexdigest() == meta['pngSha256'], '遮罩源文件发生变化')
    need(not any(u.EditorAssetLibrary.does_asset_exist(p) for p in (TEX, EMPTY_GRASS, EMPTY_FLOWERS)), '目标资产已存在，拒绝覆盖')
    actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    starts = {a.get_path_name(): a.get_actor_transform().export_text() for a in actors if isinstance(a, u.PlayerStart)}
    landscapes = [a for a in actors if isinstance(a, u.LandscapeProxy)]
    need(len(landscapes) == 1 and landscapes[0].get_editor_property('landscape_material').get_path_name().split('.')[0] == MASTER, '地形或材质不符')
    lib = u.MaterialEditingLibrary
    mat = need(u.load_asset(MASTER), '地形材质缺失')
    original = list(lib.get_material_expressions(mat))
    need(not any(isinstance(n, u.MaterialExpressionLandscapeGrassOutput) for n in original), '已有草输出，拒绝重复创建')
    backup = ROOT / 'Saved/Validation/FabVillageGrass' / ('before-author-' + str(uuid.uuid4()))
    backup.mkdir(parents=True, exist_ok=False)
    for rel in ('Maps/L_Village_Start.umap', 'Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset'):
        shutil.copy2(CONTENT / rel, backup / Path(rel).name)
    # 非客户端的中性草种没有网格；关闭密度缩放以保留可由客户端覆盖的已烘焙权重。
    tools = u.AssetToolsHelpers.get_asset_tools()
    neutral = []
    for path in (EMPTY_GRASS, EMPTY_FLOWERS):
        asset = need(tools.create_asset(path.rsplit('/',1)[1], DEST, u.LandscapeGrassType, u.LandscapeGrassTypeFactory()), '创建中性草种失败')
        asset.set_editor_property('grass_varieties', [])
        asset.set_editor_property('enable_density_scaling', False)
        need(u.EditorAssetLibrary.save_loaded_asset(asset), '保存中性草种失败')
        neutral.append(asset)
    task = u.AssetImportTask()
    for k,v in {'filename':str(source), 'destination_path':DEST, 'destination_name':TEX.rsplit('/',1)[1],
                'automated':True, 'replace_existing':False, 'save':False}.items(): task.set_editor_property(k,v)
    tools.import_asset_tasks([task])
    tex = need(u.load_asset(TEX), '遮罩导入失败')
    tex.set_editor_property('srgb', False)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_GRAYSCALE)
    tex.set_editor_property('address_x', u.TextureAddress.TA_CLAMP)
    tex.set_editor_property('address_y', u.TextureAddress.TA_CLAMP)
    need(u.EditorAssetLibrary.save_loaded_asset(tex), '遮罩保存失败')
    nodes=[]
    try:
        def node(cls, x, y):
            n=need(lib.create_material_expression(mat,cls,x,y), '节点创建失败')
            n.set_editor_property('desc','FabVillageGrass')
            nodes.append(n)
            return n
        def wire(src,out,dst,pin): need(lib.connect_material_expressions(src,out,dst,pin), '连接失败 '+pin)
        worldpos=node(u.MaterialExpressionWorldPosition, 3500, 2800)
        worldpos.set_editor_property('world_position_shader_offset',u.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
        custom=node(u.MaterialExpressionCustom, 3750, 2800)
        x,y,w,h=meta['bounds']
        custom.set_editor_property('code',f'return saturate((P.xy-float2({x:.6f},{y:.6f}))/float2({w:.6f},{h:.6f}));')
        custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT2)
        ci=u.CustomInput();ci.set_editor_property('input_name','P');custom.set_editor_property('inputs',[ci])
        wire(worldpos,'XYZ',custom,'P')
        sample=node(u.MaterialExpressionTextureSample,4000,2800)
        sample.set_editor_property('texture',tex)
        sample.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        sample.set_editor_property('sampler_source',u.SamplerSourceMode.SSM_CLAMP_WORLD_GROUP_SETTINGS)
        wire(custom,'',sample,'UVs')
        inverse=node(u.MaterialExpressionOneMinus,4250,2800);wire(sample,'R',inverse,'')
        output=node(u.MaterialExpressionLandscapeGrassOutput,4500,2800)
        entries=[]
        for name,asset in zip(('VillageGrass','VillageFlowers'),neutral):
            entry=u.GrassInput();entry.set_editor_property('name',name);entry.set_editor_property('grass_type',asset);entries.append(entry)
        output.set_editor_property('grass_types',entries)
        wire(inverse,'',output,'VillageGrass');wire(inverse,'',output,'VillageFlowers')
        need(not list(lib.recompile_material(mat)), '地形材质编译失败')
        need(u.EditorAssetLibrary.save_loaded_asset(mat), '地形材质保存失败')
        now={a.get_path_name(): a.get_actor_transform().export_text() for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors() if isinstance(a,u.PlayerStart)}
        need(starts == now, 'PlayerStart发生变化')
        print('FAB_VILLAGE_GRASS_OUTPUT_SAVED',str(backup),'playerstarts',len(starts),'neutral_types',2)
    except Exception:
        for n in reversed(nodes): lib.delete_material_expression(mat,n)
        lib.recompile_material(mat)
        print('FAB_VILLAGE_GRASS_OUTPUT_FAILED: 新节点已撤销，原始地图未保存；请审查脏包与备份',str(backup))
        raise


run()
