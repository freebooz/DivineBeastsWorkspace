# -*- coding: utf-8 -*-
"""为两个项目物种建立真实网格LOD，并让Niagara按每个粒子的屏幕大小选择LOD。"""
import json
from pathlib import Path
import unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
BASE='/DBAPresentationPack_Core/VFX/Environment/Fauna'
def main():
    assert '-FaunaOwnedAuthoring' in unreal.SystemLibrary.get_command_line()
    api=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assets=unreal.EditorAssetLibrary
    report=[]
    for species in ('ButterflyAzure','FishSilver'):
        mesh=unreal.load_asset(BASE+'/Meshes/SM_DBA_'+species)
        system=unreal.load_asset(BASE+'/Niagara/NS_DBA_'+species)
        assert isinstance(mesh,unreal.StaticMesh) and isinstance(system,unreal.NiagaraSystem)
        settings=[]
        for percent,size in ((1.0,1.0),(0.5,0.10),(0.18,0.025)):
            setting=unreal.StaticMeshReductionSettings()
            setting.set_editor_property('percent_triangles',percent)
            setting.set_editor_property('screen_size',size)
            settings.append(setting)
        options=unreal.StaticMeshReductionOptions()
        options.set_editor_property('auto_compute_lod_screen_size',False)
        options.set_editor_property('reduction_settings',settings)
        assert api.set_lods(mesh,options)==3
        assert api.get_lod_count(mesh)==3
        verts=[api.get_number_verts(mesh,i) for i in range(3)]
        assert verts[0]>verts[1]>verts[2]>0
        # 只修改该系统所拥有的MeshRenderer；不触碰引擎模板或其他系统。
        renderers=[r for r in unreal.ObjectIterator(unreal.NiagaraMeshRendererProperties)
                   if r.get_path_name().startswith(system.get_path_name()+':')]
        assert len(renderers)==1,repr([r.get_path_name() for r in renderers])
        renderer=renderers[0];meshes=list(renderer.get_editor_property('meshes'))
        assert len(meshes)==1
        meshes[0].set_editor_property('lod_mode',unreal.NiagaraMeshLODMode.PER_PARTICLE)
        meshes[0].set_editor_property('lod_distance_factor',1.0)
        renderer.set_editor_property('meshes',meshes)
        assert assets.save_loaded_asset(mesh,only_if_is_dirty=False)
        assert assets.save_loaded_asset(system,only_if_is_dirty=False)
        report.append({'species':species,'lodVertexCounts':verts,'niagaraLODMode':'PerParticle',
                       'screenSizes':[1.0,0.10,0.025]})
    out=ROOT/'Saved/Validation/Fauna/MeshLODs.json';out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps({'status':'passed','systems':report},ensure_ascii=False,indent=2),encoding='utf-8')
    print('FAUNA_MESH_LODS',json.dumps(report))
main()
