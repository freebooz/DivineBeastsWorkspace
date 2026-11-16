# -*- coding:utf-8 -*-
"""为《神兽联盟》OrbProjectile V1 自有 UE5.8 审核编辑器切换独占Monolith端口9317。
不修改Monolith配置文件、世界关卡或其它UE进程；用于原生Niagara作者接口。
"""
import unreal,os,json,traceback
from pathlib import Path
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
dest=root/'Saved/Validation/OrbProjectileV1/OwnedMonolithPort.json'
try:
  command=unreal.SystemLibrary.get_command_line()
  assert '-OrbProjectileV1Owned' in command
  assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
  assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
  cls=getattr(unreal,'MonolithSettings',None)
  if cls is None:cls=unreal.load_class(None,'/Script/MonolithCore.MonolithSettings')
  assert cls is not None
  settings=unreal.get_default_object(cls)
  previous=settings.get_editor_property('ServerPort')
  settings.set_editor_property('ServerPort',9317)
  assert settings.get_editor_property('ServerPort')==9317
  world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
  assert world is not None
  unreal.SystemLibrary.execute_console_command(world,'Monolith.Restart')
  result=dict(status='requested',pid=os.getpid(),oldPort=previous,port=9317,
              project=unreal.Paths.get_project_file_path(),configSaved=False,
              serverOrGameplayModified=False)
  print('ORBP_MONOLITH_OWNER',os.getpid(),'PORT',9317,flush=True)
except Exception:
  result=dict(status='failed',error=traceback.format_exc()[-2500:])
  print('ORBP_MONOLITH_START_ERROR',result['error'],flush=True)
finally:
  dest.parent.mkdir(parents=True,exist_ok=True)
  dest.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
