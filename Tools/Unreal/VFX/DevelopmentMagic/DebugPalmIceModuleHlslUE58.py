# -*- coding: utf-8 -*-
"""SSLM冰霜P1审核模块编译隔离：可逆仅改自身Review/NM HLSL，不修改任何平台/源模块。
-mode=original写入上游同签名合法HLSL，若PIE通过说明新运动HLSL引起；-mode=restore从审核作者源码恢复。
保存原始审核模块快照与SHA256；如资产在别的会话被修改则阻止操作。
"""
import unreal,sys,json,re,hashlib,shutil,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
B='/DBAPresentationPack_Core/VFX/PalmElementGather/Review/V1/Niagara/'
OWN=B+'NM_DBA_PalmGatherMotion_Review'
SRC='/DBAPresentationPack_Core/VFX/Library/00_Core/Compatibility/NM_PY_FireMotionFlipbook_c6faccf5'
path=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/PalmElementGather/Review/V1/Niagara/NM_DBA_PalmGatherMotion_Review.uasset'
bak=ROOT/'Saved/Validation/PalmIceV1/Backups'
manifest=bak/'RestoreManifest.json'
mode='original' if '-PalmIceHlsl=original' in unreal.SystemLibrary.get_command_line() else 'restore' if '-PalmIceHlsl=restore' in unreal.SystemLibrary.get_command_line() else None
if mode is None:raise RuntimeError('仅允许original或restore两种显式操作')
assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
assert path.exists()
own=unreal.load_asset(OWN);src=unreal.load_asset(SRC)
assert isinstance(own,unreal.NiagaraScript) and isinstance(src,unreal.NiagaraScript)
def hlsl_nodes(asset):
 return [n for n in unreal.ObjectIterator(unreal.NiagaraNodeCustomHlsl)
         if n.get_path_name().startswith(asset.get_path_name()+':')]
a=hlsl_nodes(own);b=hlsl_nodes(src)
assert len(a)==len(b)==1,'Niagara HLSL不是唯一节点'
old=str(a[0].get_editor_property('custom_hlsl'))
source=str(b[0].get_editor_property('custom_hlsl'))
def hashstr(s):return hashlib.sha256(s.encode('utf8')).hexdigest()
if mode=='original':
 if manifest.exists():raise RuntimeError('存在未处理的调试备份，不能二次覆盖')
 src_code=(ROOT/'Tools/Unreal/VFX/DevelopmentMagic/AuthorPalmIceV1ReviewUE58.py').read_text(encoding='utf8')
 assert 'Particles.SubImageIndex=16.0*frac' in src_code
 assert 'SSLM 双掌元素聚能公共审核运动函数' in old
 assert old!=source
 bak.mkdir(parents=True,exist_ok=True)
 shutil.copy2(path,bak/'OriginalPalmIceModuleBeforeProbe.uasset')
 info={'beforeSha256':hashstr(old),'originHlslSha256':hashstr(source),
       'moduleBeforeSha256':hashlib.sha256(path.read_bytes()).hexdigest(),'mode':'original','originalHlsl':old}
 manifest.write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 new=source
else:
 if not manifest.exists():raise RuntimeError('未找到审核备份，禁止盲目恢复')
 info=json.loads(manifest.read_text(encoding='utf8'))
 if hashstr(old)!=info['originHlslSha256']:raise RuntimeError('其他工作已修改临时模块，拒绝恢复')
 new=info['originalHlsl']
a[0].set_editor_property('custom_hlsl',new)
if str(a[0].get_editor_property('custom_hlsl'))!=new:raise RuntimeError('HLSL内存回读失败')
if not unreal.EditorAssetLibrary.save_loaded_asset(own,False):raise RuntimeError('Niagara脚本保存失败')
info['lastAction']=mode
info['afterSha256']=hashstr(str(a[0].get_editor_property('custom_hlsl')))
manifest.write_text(json.dumps(info,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
print('SSLM_PALM_ICE_HLSL_SWITCHED',mode,'BYTES',len(new),'FILE',path,flush=True)
