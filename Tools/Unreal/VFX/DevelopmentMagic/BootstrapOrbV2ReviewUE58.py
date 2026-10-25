# -*- coding: utf-8 -*-
"""SSLM V2返工自有编辑器启动与只读检查。只改本进程Monolith端口，不保存配置。
复核原图集与材质接口，将原生纹理导出为证据；不修改V1或正式玩法资源。
"""
import unreal, json, os, traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
OUT=ROOT/'Saved/Validation/OrbProjectileV2'
OUT.mkdir(parents=True,exist_ok=True)
result={'status':'starting','pid':os.getpid(),'engine':unreal.SystemLibrary.get_engine_version(),'v1Verdict':'REJECTED_BY_HUMAN','assetsChanged':False}
try:
    assert '-OrbV2ReworkOwned' in unreal.SystemLibrary.get_command_line()
    assert result['engine'].startswith('5.8')
    cls=unreal.load_class(None,'/Script/MonolithCore.MonolithSettings')
    settings=unreal.get_default_object(cls)
    settings.set_editor_property('ServerPort',9318)
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'Monolith.Restart')
    result['port']=9318
    base='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V1'
    tex=unreal.load_asset(base+'/Textures/T_DBA_FireOrb_FlameFlip_Review')
    result['legacyTextureSize']=[tex.blueprint_get_size_x(),tex.blueprint_get_size_y()]
    task=unreal.AssetExportTask()
    task.set_editor_property('object',tex)
    task.set_editor_property('filename',str(OUT/'V1_FlameSourceAudit.png'))
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_identical',True)
    task.set_editor_property('prompt',False)
    result['textureExported']=bool(unreal.Exporter.run_asset_export_task(task))
    result['materialApi']=[v for v in dir(unreal.MaterialEditingLibrary) if any(x in v for x in ('expression','connect','recompile','usage'))]
    result['customClassAvailable']=hasattr(unreal,'MaterialExpressionCustom')
    result['status']='ready'
except Exception:
    result['status']='failed';result['error']=traceback.format_exc()
finally:
    (OUT/'Bootstrap.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print('ORBV2_BOOTSTRAP',json.dumps(result,ensure_ascii=False),flush=True)
