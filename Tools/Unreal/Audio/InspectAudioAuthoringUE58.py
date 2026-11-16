# -*- coding: utf-8 -*-
"""只读：检测UE5.8音频资源工厂和属性；为三维衰减/并发正式配置选择正确的引擎接口。"""
import unreal
for name in ["SoundAttenuationFactory","SoundConcurrencyFactory","SoundAttenuation","SoundConcurrency","SoundAttenuationSettings","SoundConcurrencySettings","SoundClassFactory","SoundClass"]:
    obj = getattr(unreal,name,None)
    print("SSLM_AUDIO_CLASS", name, "PRESENT" if obj is not None else "MISSING")
    if obj is None:
        continue
    if name in ["SoundAttenuationSettings","SoundConcurrencySettings"]:
        try:
            instance=obj()
            props=[x for x in dir(instance) if not x.startswith("_") and any(k in x for k in ("attenuat","radius","falloff","distance","max_count","resolution","spatial","volume"))]
            print("SSLM_AUDIO_PROPERTIES",name,props[:65])
        except Exception as exc:
            print("SSLM_AUDIO_PROPERTIES_ERROR",name,str(exc)[:250])
for name in ["SoundAttenuation","SoundConcurrency"]:
    cls=getattr(unreal,name,None)
    if cls:
        try:
            obj=unreal.new_object(cls,getattr(unreal,"get_transient_package")())
            for field in ["attenuation","attenuation_settings","concurrency","concurrency_settings"]:
                try:
                    value=obj.get_editor_property(field)
                    print("SSLM_AUDIO_FIELD",name,field,str(value)[:350])
                except Exception:
                    pass
        except Exception as exc:
            print("SSLM_AUDIO_NEW_ERROR",name,str(exc)[:300])
