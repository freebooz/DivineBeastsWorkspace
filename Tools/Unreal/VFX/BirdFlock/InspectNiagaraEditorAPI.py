#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8只读Niagara编辑器能力探测，决定是否能用Editor Python生成真实鸟群发射器。"""
import unreal  # type: ignore

names=("NiagaraSystem","NiagaraEmitter","NiagaraSystemFactoryNew","NiagaraEmitterFactoryNew",
       "NiagaraEditorLibrary","NiagaraEditorSubsystem","NiagaraScript",
       "NiagaraMeshRendererProperties","NiagaraSpriteRendererProperties",
       "NiagaraSystemConversionContext","NiagaraConversionLibrary")
for name in names:
    typ=getattr(unreal,name,None)
    print("BIRD_NIAGARA_API",name,typ)
    if typ and ("Library" in name or "Subsystem" in name):
        print("BIRD_NIAGARA_METHODS",name,
              [n for n in dir(typ) if not n.startswith("_") and any(
                  word in n.lower() for word in ("emitter","renderer","asset","system","script","module","stack"))][:65])
assets=unreal.EditorAssetLibrary
source="/GamePlatformVFX/Environment/FallingFoliage/Niagara/NS_GP_FallingFoliage_Base"
sys=assets.load_asset(source)
print("BIRD_NIAGARA_SOURCE",sys.get_path_name() if sys else None)
if sys:
    for prop in ("emitter_handles","exposed_parameters","system_spawn_script",
                 "system_update_script","fixed_bounds","effect_type"):
        try:
            value=sys.get_editor_property(prop)
            print("BIRD_NIAGARA_PROP",prop,str(value)[:600])
        except Exception as error:
            print("BIRD_NIAGARA_PROP_UNAVAILABLE",prop,str(error)[:100])
    print("BIRD_NIAGARA_OBJECT_METHODS",[v for v in dir(sys)
        if any(w in v.lower() for w in ("emitter","renderer","parameter","compile","system","script"))][:90])
print("BIRD_NIAGARA_PROBE_COMPLETE")
