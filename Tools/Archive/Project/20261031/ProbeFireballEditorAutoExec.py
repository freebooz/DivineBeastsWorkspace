# -*- coding: utf-8 -*-
"""验证 UE5.8 Editor -ExecCmds=py 脚本入口，使用自身独立进程并自动退出。"""
raise RuntimeError('SCRIPT_RETIRED: 已退休归档，见Tools/Archive/Project/20261031/README.md。')
from pathlib import Path
import json,time,os,unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
dest=root/'Saved/Validation/FireballV3P1/EditorAutoExecProbe.json'
dest.parent.mkdir(parents=True,exist_ok=True)
result={"pid":os.getpid(),"project":unreal.Paths.get_project_file_path(),"engine":unreal.SystemLibrary.get_engine_version(),
 "argv":unreal.SystemLibrary.get_command_line(),"ticks":0,"completed":False}
assert '-FireballV3CaptureOwned' in result["argv"]
assert result["engine"].startswith("5.8")
def callback(dt):
 global result,handle
 result["ticks"]+=1
 if result["ticks"]>=8:
  result["completed"]=True
  dest.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
  unreal.unregister_slate_post_tick_callback(handle)
  unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(callback)
dest.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding="utf-8")
print('SSLM_FIREBALL_EDITOR_EXEC_PROBE_RUNNING',os.getpid(),flush=True)
