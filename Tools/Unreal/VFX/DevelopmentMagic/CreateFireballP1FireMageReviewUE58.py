# -*- coding: utf-8 -*-
"""创建《神兽联盟》V3 P1 参考火焰独立审核资源：复用项目现有 FireMage Charge，不覆盖正式资源。"""
import unreal,hashlib,json,traceback
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
SRC='/DBAPresentationPack_Core/VFX/Library/Game/FireMage/Niagara/NS_PY_Charge'
DST='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3RefCharge'
OUT=ROOT/'Saved/Validation/FireballV3P1/FireMageReferenceCopy.json'
result={'source':SRC,'destination':DST,'status':'not_started','humanAccepted':False,'formalAssetTouched':False,'gameplayTimingChanged':False}
try:
    assert unreal.Paths.get_project_file_path().replace('\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8')
    obj=unreal.load_asset(SRC)
    if not isinstance(obj,unreal.NiagaraSystem): raise RuntimeError('源火焰Niagara不存在')
    if unreal.EditorAssetLibrary.does_asset_exist(DST):
        raise RuntimeError('独立参考审核资产已有，拒绝覆盖')
    clone=unreal.EditorAssetLibrary.duplicate_asset(SRC,DST)
    if not isinstance(clone,unreal.NiagaraSystem): raise RuntimeError('UE5.8无法生成审核系统副本')
    if not unreal.EditorAssetLibrary.save_loaded_asset(clone,False): raise RuntimeError('火焰审核系统没有保存')
    binary=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Casting/Review/V3/Niagara/NS_DBA_FireballCast_V3RefCharge.uasset'
    assert binary.exists() and binary.stat().st_size>10000
    result.update(status='created',bytes=binary.stat().st_size,sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                  shaderCompiled=False,nativeVisualCaptured=False)
    print('SSLM_P1_FIREMAGE_REVIEW_CREATED',DST,flush=True)
except Exception as e:
    result.update(status='failed',error=str(e),traceback=traceback.format_exc()[-3000:])
    print('SSLM_P1_FIREMAGE_REVIEW_ERROR',str(e),flush=True)
    raise
finally:
    OUT.parent.mkdir(parents=True,exist_ok=True)
    OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
