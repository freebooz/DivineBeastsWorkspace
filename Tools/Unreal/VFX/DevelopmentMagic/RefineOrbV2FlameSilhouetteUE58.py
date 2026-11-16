# -*- coding: utf-8 -*-
"""SSLM V2材质原型轮廓修正：消除火焰审核薄片的平切顶边。
仅修改本轮新建V2材质，并重新拍摄材质/网格隔离图。不是Niagara集成或人工通过。
"""
import unreal,math
from pathlib import Path
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve().parent
BASE='/DBAPresentationPack_Core/VFX/OrbProjectile/Review/V2'
assert '-OrbV2ReworkOwned' in unreal.SystemLibrary.get_command_line()
m=unreal.load_asset(BASE+'/Materials/M_DBA_FireVeil_V2')
assert isinstance(m,unreal.Material)
for n in unreal.MaterialEditingLibrary.get_material_expressions(m):
    if isinstance(n,unreal.MaterialExpressionCustom):
        old=n.get_editor_property('code')
        boundary='float u=UV.x*2-1;float v=UV.y;'
        assert boundary in old,'V2薄片材质已变化，拒绝覆盖未知修改'
        prefix=old[:old.index(boundary)]
        new=r'''
float u=UV.x*2-1;float v=saturate(UV.y);
float bend=.18*sin(v*7-T*2.1)*sin(v*3.1415927);
float width=.68*pow(max(.0,sin(v*3.1415927)),.85);
float edge=1-smoothstep(width*.42,max(width,.001),abs(u+bend));
float vertical=smoothstep(.01,.16,v)*(1-smoothstep(.82,.99,v));
float n=nf.fbm(float3((u+bend)*3.4,v*6-T*1.65,T*.20));
float mask=edge*vertical*smoothstep(.18,.62,n+.12);
float3 c=lerp(float3(2.3,.26,.009),float3(6.0,2.65,.23),smoothstep(.25,.72,n));
return float4(c,saturate(mask*.78));
'''
        n.set_editor_property('code',prefix+new)
unreal.MaterialEditingLibrary.recompile_material(m)
assert unreal.EditorAssetLibrary.save_loaded_asset(m,False)
core=unreal.load_asset(BASE+'/Materials/M_DBA_FireCoreFlow_V2')
for n in unreal.MaterialEditingLibrary.get_material_expressions(core):
    if isinstance(n,unreal.MaterialExpressionCustom):
        old=n.get_editor_property('code')
        assert 'float3(1.5,.105,.009)' in old
        n.set_editor_property('code',old.replace('float3(1.5,.105,.009)','float3(1.7,.23,.014)').replace('float3(4.6,1.35,.07)','float3(4.6,1.8,.12)'))
unreal.MaterialEditingLibrary.recompile_material(core)
assert unreal.EditorAssetLibrary.save_loaded_asset(core,False)
# 保留相同真实材质审核场景，仅将输出路径分版，旧失败证据不覆盖。
source=ROOT/'Tools/Unreal/VFX/DevelopmentMagic/RefineOrbV2ArtProofUE58.py'
wrapper=source.read_text(encoding='utf8')
start=wrapper.index('material=unreal.load_asset')
end=wrapper.index('# 审核薄片仅验证')
wrapper=wrapper[:start]+wrapper[end:]
wrapper=wrapper.replace('OrbProjectileV2/ArtProofR1','OrbProjectileV2/ArtProofR2')
wrapper=wrapper.replace('.48+.04*(i%2)','.38+.04*(i%2)')
exec(compile(wrapper,str(source),'exec'),{'__name__':'__main__'})
