# -*- coding: utf-8 -*-
"""Monolith只读真实新手村PIE回归：读取全部Landscape MID与默认本地鸟群，不修改地图。
前提：已通过Monolith启动正式Village PIE且异步预热已结束；缺世界/MID/贴图/鸟群均失败。
56是意图数量；本脚本另读Niagara实际活跃组件，GPU粒子数须独立回读，不能用意图冒充。
"""
import json
from pathlib import Path
import unreal


world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
assert world and world.get_outermost().get_name().startswith('/DBAWorldPack_Village/Maps/'), '没有正式Village PIE'
landscapes = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.LandscapeProxy)
assert len(landscapes) > 0, '无真实地形'
expected = {'VillageGroundAlbedoTexture': '/DBAPresentationPack_Core/Textures/Terrain/Grass/T_DBA_Village_Grass_BaseColor',
            'VillageGroundNormalTexture': '/DBAPresentationPack_Core/Textures/Terrain/Grass/T_DBA_Village_Grass_Normal',
            'VillageGroundORMTexture': '/DBAPresentationPack_Core/Textures/Terrain/Grass/T_DBA_Village_Grass_ORM',
            'VillageSnowCoverageTexture': '/GamePlatformSurface/Textures/Snow/T_GP_Snow_CoverageNoise',
            'VillageSnowAlbedoTexture': '/GamePlatformSurface/Textures/Weather/T_GP_Weather_SnowAlbedo',
            'VillageSnowNormalTexture': '/GamePlatformSurface/Textures/Weather/T_GP_Weather_SnowNormal'}
mids = []
for landscape in landscapes:
    for component in landscape.get_components_by_class(unreal.LandscapeComponent):
        for index in range(component.get_num_materials()):
            mid = component.get_material_instance_dynamic(index)
            assert mid, '真实地形MID缺失'
            actual = {name: mid.get_texture_parameter_value(name).get_path_name().split('.')[0] for name in expected}
            assert actual == expected, f'真实MID草地/雪贴图未绑定或错绑: {actual}'
            dry = mid.get_vector_parameter_value('VillageDryColor')
            assert abs(dry.r-1.) < .002 and abs(dry.g-1.) < .002 and abs(dry.b-1.) < .002, '草地发生额外滤色'
            mids.append({'Path': mid.get_path_name(), 'Snow': mid.get_scalar_parameter_value('VillageSnowAmount'),
                         'Wetness': mid.get_scalar_parameter_value('VillageWetness')})
assert len(mids) >= 64, '未核实整张正式Landscape'
services = [s for s in unreal.ObjectIterator(unreal.DivineBeastsPresentationClientSubsystem)
            if ':LocalPlayer_' in s.get_path_name()]
assert len(services) == 1, '本次单玩家PIE须恰好一个本地项目表现协调器'
service = services[0]
counts = [service.get_requested_bird_count('DBA.Environment.WorldBirds'),
          service.get_requested_bird_count('DBA.Environment.VillageSwallows')]
assert counts == [32, 24], f'近空双鸟群缺失: {counts}'
birds = [o for o in unreal.ObjectIterator(unreal.NiagaraComponent)
         if o.get_world() == world and o.get_asset() and 'BirdFlock' in o.get_asset().get_path_name() and o.is_active()]
assert len(birds) == 2, f'实际活跃鸟群Niagara组件不是两组: {len(birds)}'
result = {'Passed': True, 'World': world.get_path_name(), 'MIDCount': len(mids), 'MIDs': mids,
          'BirdIntentCounts': counts, 'ActiveBirdNiagaraSystems': [b.get_asset().get_path_name() for b in birds],
          'GPUCountVerified': False, 'CookVerified': False, 'ManualVisualAccepted': False}
output = Path(unreal.Paths.project_dir()).parent / 'Saved/Validation/VillageAmbient20261013/PIE.json'
output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print('VILLAGE_AMBIENT_PIE_OK ' + json.dumps({k: v for k, v in result.items() if k != 'MIDs'}))
