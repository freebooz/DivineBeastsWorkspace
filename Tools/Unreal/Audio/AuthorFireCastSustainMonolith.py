"""通过本机Monolith维护唯一火球前摇Cue；原音色只读，生命周期由既有Presentation/SFX持有。

默认只读；--apply仅创建缺失Cue和更新其Definition。必须指定编辑器PID/端口，所有备份在工程Saved。
"""
import argparse
import hashlib
import json
import shutil
from pathlib import Path
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[3]
CUE = '/DBASFXPack_Core/Audio/Spells/Cues/SC_DBA_Spell_FireCast'
DEFINITION = '/DBASFXPack_Core/Audio/Spells/Definitions/DA_DBA_SFX_Spell_FireCast'
WAVE = '/DBASFXPack_Core/Audio/Library/Spells/Waves/S_DBA_Spells_ArcaneCast'
TEMPLATE = '/DBASFXPack_Core/Audio/Library/Spells/Definitions/DA_DBA_SFX_Spells_ArcaneCast'
CONTENT = ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBASFXPack_Core/Content'

def disk(asset):
    return CONTENT/'Audio'/(asset.split('/Audio/', 1)[1]+'.uasset')

def call(port, namespace, action, params):
    body = {'jsonrpc':'2.0','id':1,'method':'tools/call','params':{
        'name':namespace+'_query','arguments':{'action':action,'params':params}}}
    with urlopen(Request(f'http://127.0.0.1:{port}/mcp',json.dumps(body).encode(),
                         {'Content-Type':'application/json'}),timeout=120) as response:
        envelope=json.load(response)
    if envelope.get('error'): raise RuntimeError(envelope['error'])
    result=envelope['result']
    if result.get('isError'): raise RuntimeError(result)
    value=json.loads(next(item['text'] for item in result['content'] if item['type']=='text'))
    if value.get('ok') is False or value.get('success') is False or value.get('error'):
        raise RuntimeError(value)
    return value

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',type=int,required=True)
    parser.add_argument('--pid',type=int,required=True)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--apply',action='store_true')
    args=parser.parse_args()
    output=args.output.resolve()
    assert output.is_relative_to(ROOT/'Saved'), '输出必须位于当前工程Saved'
    output.mkdir(parents=True,exist_ok=True)
    guard=f'''import unreal as u, os, json
from pathlib import Path
assert os.getpid()=={args.pid}
assert Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).parent.resolve()==Path({str(ROOT)!r}).resolve()
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
'''
    def python(source):
        return call(args.port,'editor','run_python',{'command':guard+source,'mode':'execute_file','unattended':True})
    python('assert u.load_asset('+repr(WAVE)+') is not None')
    source_hash=hashlib.sha256(disk(WAVE).read_bytes()).hexdigest()
    if args.apply:
        for asset in (CUE,DEFINITION):
            original=disk(asset)
            if original.exists():
                backup=output/'Backups'/original.relative_to(ROOT)
                backup.parent.mkdir(parents=True,exist_ok=True)
                if not backup.exists(): shutil.copy2(original,backup)
        if not disk(CUE).exists():
            call(args.port,'audio','build_sound_cue_from_spec',{'asset_path':CUE,'spec':{
                'nodes':[{'id':'FireCast','type':'WavePlayer','properties':{'SoundWave':WAVE,'bLooping':True}}],
                'connections':[],'first_node':'FireCast','properties':{'VolumeMultiplier':1,'PitchMultiplier':1}}})
    graph=call(args.port,'audio','get_sound_cue_graph',{'asset_path':CUE})
    # 审核原生节点，拒绝覆盖为不相符的另一个实现。
    payload=graph.get('data',graph)
    assert len(payload['nodes'])==1,graph
    props=payload['nodes'][0]['properties']
    assert props['bLooping'] is True and props['SoundWave'].split('.')[0]==WAVE,graph
    validation=call(args.port,'audio','validate_sound_cue',{'asset_path':CUE})
    assert validation.get('data',validation)['valid'] is True,validation
    # Cue创建动作会标脏；只允许保存自有Cue，绝不保存其它任务的内容。
    source=f'''cue=u.load_asset({CUE!r})
wave=u.load_asset({WAVE!r})
assert not wave.get_editor_property('looping'), '通用音色必须保持单次'
assert abs(cue.get_editor_property('pitch_multiplier')-1)<.0001
'''
    if args.apply:
        # 此阶段guard允许唯一自有Cue因原生创建标脏。
        guard=guard.replace('assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()',
            'assert all(p.get_path_name() == '+repr(CUE)+' for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages())')
        source+=f'''definition=u.load_asset({DEFINITION!r})
if definition is None: definition=u.EditorAssetLibrary.duplicate_asset({TEMPLATE!r},{DEFINITION!r})
assert isinstance(definition,u.GamePlatformSFXDefinition)
ident=u.GamePlatformId()
ident.set_editor_property('namespace','presentation.dba.audio.spell.fire')
ident.set_editor_property('name','cast')
ident.set_editor_property('logical_version',1)
definition.set_editor_property('logical_id',ident)
definition.set_editor_property('sound',cue)
definition.set_editor_property('fade_out_seconds',.03)
assert u.EditorAssetLibrary.save_loaded_asset(cue,False)
assert u.EditorAssetLibrary.save_loaded_asset(definition,False)
'''
    else:
        source+=f'definition=u.load_asset({DEFINITION!r})\nassert isinstance(definition,u.GamePlatformSFXDefinition)\n'
    source+=f'''assert str(definition.get_editor_property('sound')).split("'")[1].split('.')[0]=={CUE!r}
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages()
Path({str(output/'Native.json')!r}).write_text(json.dumps({{'PID':os.getpid(),'Cue':{CUE!r},'Definition':{DEFINITION!r},'SourceWaveUnlooped':True,'Passed':True}}),encoding='utf-8')
'''
    # 防止Monolith返回Python错误却未设置工具isError：落盘证据必须由本次编辑器实际产生。
    native=output/'Native.json'
    if native.exists(): native.unlink()
    result=python(source)
    assert native.exists(),result
    assert hashlib.sha256(disk(WAVE).read_bytes()).hexdigest()==source_hash, '原音色不得修改'
    (output/'Graph.json').write_text(json.dumps(graph,ensure_ascii=False,indent=2),encoding='utf-8')
    (output/'Validation.json').write_text(json.dumps(validation,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'Passed':True,'PID':args.pid,'Apply':args.apply,'SourceSHA256':source_hash}))

if __name__=='__main__': main()
