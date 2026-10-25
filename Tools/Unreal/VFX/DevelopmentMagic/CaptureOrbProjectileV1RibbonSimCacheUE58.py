# -*- coding: utf-8 -*-
"""SSLM OrbProjectile V1 历史Ribbon GPU SimCache诊断，沿用独立PIE的FireFlight审查地图/移动脚本。
只将缓存中每发射器粒子与RibbonWidth数据写入Saved报告，不修改任何正式Niagara。
"""
from pathlib import Path
import unreal
root=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
src=root/'Tools/Unreal/VFX/DevelopmentMagic/CaptureOrbProjectileV1FlightPIEUE58.py'
code=src.read_text(encoding='utf8')
old="exec(compile(source,str(src),'exec'),{'__name__':'__main__'})"
assert code.count(old)==1
injection="""
# 只在已有65帧生成之后测试内存NiagaraSimCache；不改变正式逻辑和发射器。
needle='if self.frames not in STAGES:return'
assert source.count(needle)==1
check='''if self.frames==65:
                    diag={'ageFrame':self.frames,'emitters':[]}
                    try:
                        cache=unreal.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(self.comp)
                        ok=unreal.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                            cache,unreal.NiagaraSimCacheCreateParameters(),self.comp,False,1.0/60.0)
                        diag['captureOK']=bool(ok)
                        diag['frames']=cache.get_num_frames()
                        diag['numEmitters']=cache.get_num_emitters()
                        for ei in range(cache.get_num_emitters()):
                            n=str(cache.get_emitter_name(ei))
                            e={'name':n}
                            try:e['colorCount']=len(cache.read_color_attribute('Color',n,0))
                            except Exception as ex:e['colorError']=str(ex)
                            try:e['positionCount']=len(cache.read_position_attribute('Position',n,0))
                            except Exception as ex:e['positionError']=str(ex)
                            try:e['ribbonWidthCount']=len(cache.read_float_attribute('RibbonWidth',n,0))
                            except Exception as ex:e['widthError']=str(ex)
                            try:
                                cols=list(cache.read_color_attribute('Color',n,0))
                                ws=list(cache.read_float_attribute('RibbonWidth',n,0))
                                ps=list(cache.read_position_attribute('Position',n,0))
                                if cols:e['alphaMinMax']=[min(float(c.a) for c in cols),max(float(c.a) for c in cols)]
                                if ws:e['widthMinMax']=[min(map(float,ws)),max(map(float,ws))]
                                if ps:
                                    xyz=[(round(float(p.x),2),round(float(p.y),2),round(float(p.z),2)) for p in ps]
                                    e['positionFirstLast']=[xyz[0],xyz[-1]]
                                    e['positionBounds']=[[min(p[k] for p in xyz),max(p[k] for p in xyz)] for k in range(3)]
                                    e['distinctPositionCount']=len(set(xyz))
                            except Exception as ex:e['statError']=str(ex)

                            diag['emitters'].append(e)
                    except Exception:
                        diag['error']=traceback.format_exc()[-1800:]
                    self.report['ribbonSimCache']=diag
                    self.persist()
                if self.frames not in STAGES:return'''
source=source.replace(needle,check)
source=source.replace('Saved/Validation/OrbProjectileV1/NativeFlightPIE',
                      'Saved/Validation/OrbProjectileV1/NativeFlightPIESimCache')
"""
code=code.replace(old,injection+'\n'+old)
exec(compile(code,str(src),'exec'),{'__name__':'__main__'})
