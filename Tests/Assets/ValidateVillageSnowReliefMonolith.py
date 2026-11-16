"""真实UE雪材质：完整PBR输出、近景视差和服务器中性引用边界。"""
import importlib.util,re
from pathlib import Path
spec=importlib.util.spec_from_file_location('native',Path(__file__).with_name('VerifySnowNativeGraphsMonolith.py'))
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)
p='/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
g=native.mcp_call(9316,'material','export_material_graph',{'asset_path':p,'include_properties':True,'include_positions':False})
nodes=g['nodes'];outputs={o['to_property'] for o in g['outputs']}
names={n.get('props',{}).get('ParameterName') for n in nodes}
assert {'VillageSnowORMTexture','VillageSnowHeightTexture'}<=names,'当前原生材质缺少配套ORM/高度'
assert any(n['class']=='BumpOffset' for n in nodes),'缺少受距离限制的近景高度视差'
assert {'SubsurfaceColor','Opacity','Roughness','Normal','AmbientOcclusion'}<=outputs,outputs
assert 'WorldPositionOffset' not in outputs,'雪层不能擅自改变用户地形/脚部高度'
# exporter的属性名表尚未覆盖隐藏MP_Displacement；完整原生连接查询使用引擎属性名。
full=native.mcp_call(9316,'material','get_full_connection_graph',{'asset_path':p})
out={o['property']:o['expression'] for o in full['material_outputs']}
assert 'Displacement' in out,'未连接真实Displacement输出'
index={n['id']:n for n in nodes}
parents={}
for e in g['connections']:parents.setdefault(e['to'],set()).add(e['from'])
def ancestors(key):
    found=set();pending=[key]
    while pending:
        current=pending.pop()
        if current in found:continue
        found.add(current);pending.extend(parents.get(current,()))
    return found
parameters={n.get('props',{}).get('ParameterName'):n for n in nodes if n['class'] in ('ScalarParameter','VectorParameter')}
displacement=parameters['Displacement Multiplier']
assert float(displacement['props']['DefaultValue'])==10.,'松雪层最大置换应为10cm'
depth=parameters['Snow Footprint Depth Cm']
assert float(depth['props']['DefaultValue'])==5.,'脚印压陷仍限制在5cm'
assert depth['id'] in ancestors(out['Displacement']),'压陷深度没有进入真实输出'
assert displacement['id'] in ancestors(out['Displacement']),'置换参数没有进入真实输出'
tint=parameters['VillageSnow Subsurface Tint']
rgb=[float(v) for v in re.findall(r'[RGB]=([0-9.]+)',tint['props']['DefaultValue'])]
assert rgb==[.547744,.710151,1.],rgb
assert tint['id'] in ancestors(out['SubsurfaceColor']),'浅蓝未连接次表面输出'
assert tint['id'] not in ancestors(out['BaseColor']),'次表面乘色不应污染基础色'
for name,value in [('Snow Noise Density',12.),('Snow Fine Noise Density',48.),('Snow Noise Strength',.30)]:
    node=parameters[name]
    assert abs(float(node['props']['DefaultValue'])-value)<1e-6,name
    assert node['id'] in ancestors(out['Displacement']),name+'未影响真实雪高度'
print('VILLAGE_SNOW_RELIEF_NATIVE_GRAPH_PASS PBR+SSS+noise Displacement=10cm FootprintDepth=5cm no-WPO')
