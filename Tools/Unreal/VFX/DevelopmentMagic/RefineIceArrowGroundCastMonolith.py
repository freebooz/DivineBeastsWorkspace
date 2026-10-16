# -*- coding: utf-8 -*-
"""SSLM冰箭2秒地面施法作者：仅经Monolith修改已存在资产，保留身份与原图RGB。

默认只读；--apply才在本轮独占编辑器中禁用空中元素、校正内圈朝向和施法寿命。
先备份真实包，保存并重载后编译。共享模块涉及其它阶段，仅改变原Mode1地面内圈朝向。
"""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[4]
EVIDENCE=ROOT/'Saved/Validation/VFXInitialization20261016'
BASE='/DBAPresentationPack_Core/VFX/IceArrow'
DEFINITION='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Definitions/DA_DBA_VFX_DevFrostboltCast'


def main():
    """受影响资产闭包限定为三个系统、共享运动模块和既有Attached定义；失败保留原生证据。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');args=parser.parse_args()
    spec=importlib.util.spec_from_file_location('ground_cast_protocol',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p);p.EVIDENCE=EVIDENCE/'IceCastCalls'
    owner=json.loads((EVIDENCE/'Editor.json').read_text(encoding='utf-8-sig'))
    p.editor_python(f"import unreal,os\nassert os.getpid()=={owner['PID']}\nassert '-VFXInitializationOwned' in unreal.SystemLibrary.get_command_line()")
    paths=[BASE+'/Niagara/NS_DBA_IceArrow_'+v for v in ['Cast','GroundRune','Flight']]
    if not args.apply:
        rows=[p.niagara('get_system_summary',asset_path=path) for path in paths]
        (EVIDENCE/'IceCastBefore.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
        print('ICE_CAST_BASELINE',[(v['system_name'],[e['name'] for e in v['emitters'] if e['enabled']]) for v in rows]);return
    module=BASE+'/Niagara/NM_DBA_IceArrowMotion'
    backup=EVIDENCE/'IceCastAssetsBefore'
    for path in paths+[module,DEFINITION]:
        relative=path.removeprefix('/DBAPresentationPack_Core/')+'.uasset'
        source=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'/relative
        target=backup/relative;target.parent.mkdir(parents=True,exist_ok=True)
        if not target.exists():target.write_bytes(source.read_bytes())
    spec=importlib.util.spec_from_file_location('ice_ground_recipe',ROOT/'Tools/Unreal/VFX/DevelopmentMagic/AuthorIceArrowNiagara.py')
    author=importlib.util.module_from_spec(spec);spec.loader.exec_module(author)
    p.niagara('set_custom_hlsl_text',script_path=module,hlsl=author.HLSL)
    for stage in ['Cast','GroundRune']:
        path=BASE+'/Niagara/NS_DBA_IceArrow_'+stage
        for emitter in p.niagara('get_system_summary',asset_path=path)['emitters']:
            name=emitter['name'];keep=name in (['GroundOuter','GroundInner'] if stage=='Cast' else ['Outer','Inner'])
            if not keep:
                p.niagara('remove_emitter',asset_path=path,emitter=name);continue
            p.niagara('set_emitter_enabled',asset_path=path,emitter=name,enabled=True)
            raw=p.niagara('get_ordered_modules',asset_path=path,emitter=name);modules=json.loads(raw['result'])
            for node in modules:
                if node.get('script_path','').split('.')[0]==module:
                    p.niagara('set_module_input_value',asset_path=path,emitter=name,module_node=node['node_guid'],input='Life',value='2.1')
            p.niagara('set_module_input_value',asset_path=path,emitter=name,module_node='InitializeParticle',input='Lifetime',value='2.1')
    if any(e['name']=='RuneRing' for e in p.niagara('get_system_summary',asset_path=paths[2])['emitters']):
        p.niagara('remove_emitter',asset_path=paths[2],emitter='RuneRing')
    p.editor_python("import unreal\nd=unreal.load_asset("+repr(DEFINITION)+")\nd.set_editor_property('max_lifetime_seconds',2.2)\nassert unreal.EditorAssetLibrary.save_loaded_asset(d,False)\nassert unreal.EditorAssetLibrary.save_asset("+repr(module)+",False)")
    for path in paths:p.niagara('save_system',asset_path=path,only_if_dirty=False)
    p.editor_python('import unreal\nprint(unreal.EditorLoadingAndSavingUtils.reload_packages([unreal.load_asset(v).get_outermost() for v in '+repr(paths+[module,DEFINITION])+'],unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE))')
    result=[]
    for path in paths:
        diagnostic=p.niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        result.append(dict(system=path,summary=p.niagara('get_system_summary',asset_path=path),diagnostics=diagnostic))
    (EVIDENCE/'IceCastAfter.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    assert all(r['diagnostics']['error_count']==0 and r['diagnostics']['warning_count']==0 for r in result)
    print('ICE_GROUND_CAST_SAVED_RELOADED',len(result),'errors=0 warnings=0',flush=True)


if __name__=='__main__':main()
