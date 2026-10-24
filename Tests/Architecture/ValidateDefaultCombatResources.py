"""源合同门禁；不代替GAS原生执行、Cook或联机人工验收。"""
from pathlib import Path
import json,re
r=Path(__file__).resolve().parents[2]
paths={
'combat':'Game/Plugins/GamePlatform/Gameplay/GamePlatformCombat/Source/GamePlatformCombat/Private/Components/GamePlatformCombatComponent.cpp',
'cost':'Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Abilities/DivineBeastsConfiguredGameplayAbility.cpp',
'loadout':'Game/Plugins/DivineBeasts/DBAGameplay/Source/DivineBeastsAbilitiesRuntime/Private/Components/DivineBeastsAbilityLoadoutComponent.cpp',
'spawn':'Game/Plugins/DivineBeasts/DBAServer/Source/DBAServer/Private/Village/DivineBeastsVillageWolfWorldSubsystem.cpp'}
text={k:(r/p).read_text(encoding='utf-8-sig') for k,p in paths.items()}
ini=(r/'Game/Config/DefaultGame.ini').read_text(encoding='utf-8-sig')
errors=[]
for entry in ['EnergyRegenerationPerSecond=5.0','DefaultAbilityEnergyCost=25.0']:
 if entry not in ini:errors.append('Missing '+entry)
if not re.search(r'AuthorityInitializeWolf\(100\.0f\)',text['spawn']):errors.append('Monster spawn health is not 100')
for key in ['HandleEnergyAttributeChanged','ApplyEnergyRecovery','EnergyRecoveryTimer']:
 if key not in text['combat']:errors.append('Missing authority recovery '+key)
if 'BuildDefaultEnergyCost' not in text['cost']:errors.append('Missing native GAS default energy cost')
if 'EnergyModifiers' not in text['loadout']:errors.append('Grant validator does not accept/check energy costs')
print(json.dumps({'Passed':not errors,'Issues':errors,'NativeRuntimeVerified':False},ensure_ascii=False))
raise SystemExit(bool(errors))
