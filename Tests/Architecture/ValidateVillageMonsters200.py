"""200怪物的源码合同门禁；原生NavMesh、生成数量及客户端效果须另行验证。"""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parents[2]
base=root/'Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private/Village'
policy=(base/'DivineBeastsVillageWolfSpawnPolicy.h').read_text(encoding='utf-8-sig')
source=(base/'DivineBeastsVillageWolfWorldSubsystem.cpp').read_text(encoding='utf-8-sig')
expected={'MaxAliveWolves':200,'MinimumSpawnSeparationCm':2400,'PlayerStartSafetyRadiusCm':2200,
          'MaxWolvesPerBatch':4,'SpawnBatchIntervalSeconds':.2,'CandidateGridStepCm':1300,'MaxCandidateCells':4096}
errors=[]
for name,value in expected.items():
 m=re.search(r'\b'+name+r'\s*=\s*([\d.]+)',policy)
 if not m or abs(float(m.group(1))-value)>1e-5:errors.append(name+' does not match '+str(value))
for contract in ['CollectSpatialMasks','PrepareMasks','EvaluatePrepared','CachedSpawnGrounds','BuildSpawnCandidates','VillageMonsterPopulation']:
 if contract not in source:errors.append('Missing '+contract)
print(json.dumps({'Passed':not errors,'Issues':errors,'NativeRuntimeVerified':False},ensure_ascii=False))
raise SystemExit(bool(errors))
