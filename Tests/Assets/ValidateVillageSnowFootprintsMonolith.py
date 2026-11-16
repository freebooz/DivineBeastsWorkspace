"""只读校验真实雪材质：脚印需进入Displacement/Normal/Roughness，不能只放置未连接参数。"""
import importlib.util
from pathlib import Path
spec=importlib.util.spec_from_file_location('native',Path(__file__).with_name('VerifySnowNativeGraphsMonolith.py'))
native=importlib.util.module_from_spec(spec);spec.loader.exec_module(native)
p='/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge'
g=native.mcp_call(9316,'material','export_material_graph',{'asset_path':p,'include_properties':True,'include_positions':False})
full=native.mcp_call(9316,'material','get_full_connection_graph',{'asset_path':p})
params={n.get('props',{}).get('ParameterName'):n['id'] for n in g['nodes']}
assert 'SnowContactTexture' in params,'原生雪材质尚未实现压痕高度场'
parents={}
for edge in g['connections']:parents.setdefault(edge['to'],set()).add(edge['from'])
def ancestors(key):
    found=set();todo=[key]
    while todo:
        k=todo.pop()
        if k in found:continue
        found.add(k);todo.extend(parents.get(k,()))
    return found
outputs={o['property']:o['expression'] for o in full['material_outputs']}
for output in ['Displacement','Normal','Roughness']:
    assert params['SnowContactTexture'] in ancestors(outputs[output]),'脚印未进入'+output
assert 'SnowContactOriginExtent' in params,'缺少局部高度场的世界坐标合同'
# 通过真实连线验证原草地/雪覆盖混合仍在最终法线祖先中，不按节点创建顺序猜名称。
normal_nodes=ancestors(outputs['Normal'])
ground_normal=params['VillageGroundNormalTexture']
snow_normal=params['VillageSnowNormalTexture']
assert ground_normal in normal_nodes and snow_normal in normal_nodes,'脚印绕过原地面与雪法线混合'
coverage=params['VillageSnowAmount']
assert coverage in normal_nodes,'零雪量必须仍控制地面与雪法线混合'
print('VILLAGE_SNOW_FOOTPRINT_NATIVE_GRAPH_PASS displacement+normal+roughness')
