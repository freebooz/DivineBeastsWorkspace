# -*- coding: utf-8 -*-
"""启动独立冰霜P1 UnrealEditor并仅在该进程内重设Monolith端口9317。
不保存Config、不触动另一进程9316；禁止修改地图/正式内容包。
"""
import unreal,os,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
OUT=ROOT/'Saved/Validation/PalmIceV1/OwnedMonolithPort.json'
def main():
 cmd=unreal.SystemLibrary.get_command_line()
 assert '-PalmIceOwnedMonolith' in cmd and 'DivineBeastsArena.uproject' in unreal.Paths.get_project_file_path()
 assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
 klass=getattr(unreal,'MonolithSettings',None)
 if klass is None:klass=unreal.load_class(None,'/Script/MonolithCore.MonolithSettings')
 assert klass,'MonolithSettings is unavailable'
 settings=unreal.get_default_object(klass)
 old=settings.get_editor_property('server_port')
 settings.set_editor_property('server_port',9317)
 assert settings.get_editor_property('server_port')==9317
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 assert world,'EditorWorld unavailable'
 unreal.SystemLibrary.execute_console_command(world,'Monolith.Restart')
 result={'editorPid':os.getpid(),'oldPort':old,'newPort':settings.get_editor_property('server_port'),
  'command':cmd,'status':'restart_issued','configSaved':False,'projectAssetChanged':False}
 OUT.parent.mkdir(parents=True,exist_ok=True)
 OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
 print('SSLM_PALM_ICE_MONOLITH_PORT',9317,'PID',os.getpid(),flush=True)
try:main()
except Exception:
 OUT.parent.mkdir(parents=True,exist_ok=True)
 OUT.write_text(json.dumps({'status':'failed','error':traceback.format_exc()[-3500:]},ensure_ascii=False,indent=2),encoding='utf8')
 print('PALM_ICE_MONOLITH_SETUP_FAILED',traceback.format_exc()[-1800:],flush=True)
