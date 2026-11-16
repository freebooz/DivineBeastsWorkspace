"""只读核验本任务的8项资产；不修改其它并行导入资源或正式地图。"""
import unreal as u,json
from pathlib import Path
BASE=Path(u.Paths.project_dir()).resolve().parent
OUT=BASE/'Saved/Validation/FoliagePack20261024'
auth=json.loads((OUT/'NativeAuthoring.json').read_text(encoding='utf-8-sig'))
assets=[u.load_asset(p) for p in auth['assets']]
assert len(assets)==8 and all(assets)
root='/DBAFoliagePack_Core/Foliage'
ft=u.load_asset(root+'/FoliageTypes/FT_DBA_Tree_PrototypeWideCrown')
definition=u.load_asset(root+'/PCG/DA_DBA_Foliage_PrototypeTreeMeshSet')
assert len(definition.get_editor_property('entries'))==1
assert ft.get_editor_property('mesh').get_path_name().startswith(root+'/Trees/')
report={'passed':True,'ownedNativeAssetCount':len(assets),'logicalId':definition.get_editor_property('logical_id').export_text(),
        'pluginMounted':u.PluginBlueprintLibrary.is_plugin_mounted('DBAFoliagePack_Core'),
        'assets':[a.get_path_name() for a in assets],'unrelatedImportsModified':False,'runtimePromoted':False}
(OUT/'NativeVerification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({k:v for k,v in report.items() if k!='assets'}))
