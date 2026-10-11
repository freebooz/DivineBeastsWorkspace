#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""制作正式蝶/鱼Niagara变体：中立平台图 + 项目物种资源 + 原生EffectType。"""
import json
from FaunaMonolithProtocol import ROOT,niagara,editor_python,assert_owned_editor
from AuthorFaunaNiagaraMonolith import GP,DBA,BASE,EMITTER,configure
EFFECT=GP+'/EffectTypes/NET_GP_Fauna_Ambient'
SPECIES=('ButterflyAzure','FishSilver')
def main():
    assert_owned_editor()
    check=editor_python(f"import unreal; print('FAUNA_EFFECT_EXISTS',unreal.EditorAssetLibrary.does_asset_exist({EFFECT!r}))")
    exists=any('FAUNA_EFFECT_EXISTS True' in o.get('output','') for o in check.get('output',[]))
    if not exists:
        niagara('create_effect_type',save_path=EFFECT,cull_reaction='PauseResume',update_frequency='Low',max_distance=8000.0)
    # 基础系统保持平台资源自包含，正式项目实例替换网格和材质。
    niagara('set_renderer_material',asset_path=BASE,emitter=EMITTER,renderer_index=0,material=GP+'/Materials/M_GP_Fauna_Master')
    niagara('set_effect_type',asset_path=BASE,effect_type=EFFECT)
    niagara('save_system',asset_path=BASE,only_if_dirty=False)
    report=[]
    for species in SPECIES:
        path=DBA+'/Niagara/NS_DBA_'+species
        print(editor_python(f"import unreal\na=unreal.EditorAssetLibrary\nx=a.load_asset({path!r}) if a.does_asset_exist({path!r}) else a.duplicate_asset({BASE!r},{path!r})\nassert isinstance(x,unreal.NiagaraSystem)"))
        configure(path)
        niagara('set_renderer_mesh',asset_path=path,emitter=EMITTER,renderer_index=0,mesh=DBA+'/Meshes/SM_DBA_'+species)
        niagara('set_renderer_material',asset_path=path,emitter=EMITTER,renderer_index=0,material=DBA+'/Materials/MI_DBA_'+species)
        bounds='(X=1.15,Y=1.15,Z=10.0)' if species=='ButterflyAzure' else '(X=1.1,Y=1.6,Z=1.3)'
        niagara('set_renderer_property',asset_path=path,emitter=EMITTER,renderer_index=0,property='MeshBoundsScale',value=bounds)
        niagara('set_parameter_default',asset_path=path,parameter='User.FaunaCount',value='32' if species=='ButterflyAzure' else '64')
        niagara('set_effect_type',asset_path=path,effect_type=EFFECT)
        diag=niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        if diag.get('error_count',1) or diag.get('warning_count',0): raise RuntimeError('物种图编译失败：'+species+' '+json.dumps(diag))
        niagara('save_system',asset_path=path,only_if_dirty=False)
        report.append({'species':species,'niagara':path,'errors':diag.get('error_count'),'warnings':diag.get('warning_count')})
    out=ROOT/'Saved/Validation/Fauna/NiagaraAssets.json';out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps({'status':'passed','effectType':EFFECT,'systems':report},ensure_ascii=False,indent=2),encoding='utf-8')
    print('FAUNA_SPECIES_COMPILED',json.dumps(report,ensure_ascii=False))
if __name__=='__main__': main()
