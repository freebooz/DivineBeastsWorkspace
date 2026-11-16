# -*- coding: utf-8 -*-
"""SSLM戌狗V6：只读检查真实HUD与可复用审核Widget，评估后续原生预览可行性。"""
import unreal,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/"Saved/Validation/DogV6Staging20261019"
paths={
  "combatHUD":"/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_CombatHUD",
  "oxReview":"/DBAHeroPack_Ox/UI/Review/WBP_DBA_Ox_SkillIconReviewV6",
  "dogReview":"/DBAHeroPack_Dog/UI/Review/WBP_DBA_Dog_SkillIconReviewV6",
  "dogProfile":"/DBAHeroPack_Dog/UI/Abilities/DA_DBA_Dog_PlaceholderUIProfile",
}
reg=unreal.AssetRegistryHelpers.get_asset_registry()
reg.scan_paths_synchronous([x.rsplit("/",1)[0] for x in paths.values()],force_rescan=True)
lib=unreal.EditorAssetLibrary
result={}
for name,path in paths.items():
  entry={"path":path,"exists":bool(lib.does_asset_exist(path))}
  asset=lib.load_asset(path) if entry["exists"] else None
  if asset is not None:
    entry["nativeClass"]=str(asset.get_class().get_name())
    entry["properties"]=[x for x in ("widget_tree","generated_class","hero_definition_id","art_preview_ability_ids")
                         if x in dir(asset)]
    try:
      tree=asset.get_editor_property("widget_tree")
      entry["widgetTreeAvailable"]=tree is not None
      entry["widgetTreeClass"]=tree.get_class().get_name() if tree else None
      if tree is not None: entry["treeMethods"]=[x for x in dir(tree) if "widget" in x][:18]
    except Exception as exc:entry["widgetTreeError"]=str(exc)[:500]
    if name=="dogProfile":
      entry["heroId"]=str(asset.get_editor_property("hero_definition_id"))
      entry["entryCount"]=len(asset.get_editor_property("entries"))
      entry["artPreviewOrder"]=list(map(str,asset.get_editor_property("art_preview_ability_ids")))
  result[name]=entry
OUT.mkdir(parents=True,exist_ok=True)
(OUT/"HUDAssetProbe.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
print(json.dumps(result,ensure_ascii=False))
