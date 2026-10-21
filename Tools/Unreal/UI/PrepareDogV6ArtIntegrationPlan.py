# -*- coding: utf-8 -*-
"""SSLM戌狗V6：基于实际生成的PNG/SHA建立唯一Profile映射、原图保护与导入清单。

前置：SourceArt/UI/SkillIcons/V6/48、64、Masters及DogV6_DerivedArtManifest存在。
仅创建项目已有工作流消费的JSON清单，不创建或写入uasset；不会授权任何GAS技能。
保持V5正式名称；V6改名只作为独立美术审核名称。
"""
from pathlib import Path
import hashlib,json,struct,argparse
ROOT=Path(__file__).resolve().parents[3]
PACK=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog"
FREEZE=PACK/"Docs/DogV6_ArtFreeze_20261019.json"
DERIVED=PACK/"Docs/DogV6_DerivedArtManifest_20261020.json"
STAGING=PACK/"Docs/DogV6_StagingManifest.json"
SAVED=ROOT/"Saved/Validation/DogV6Staging20261019"
PLAN=SAVED/"IntegrationPlan.json"
PROTECT=SAVED/"ProtectedFiles.json"
PROFILE="Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog/Content/UI/Abilities/DA_DBA_Dog_PlaceholderUIProfile.uasset"

def require(check,msg):
    if not check:raise RuntimeError("戌狗V6映射准备失败："+msg)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def srcpng(row,kind,size):
    obj=row["files"][str(size)]
    rel=obj["path"]
    require(rel.startswith("Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog/SourceArt/"),"源图不属于戌狗")
    path=(ROOT/rel).resolve()
    require(path.is_relative_to(ROOT.resolve()) and path.is_file(),"缺少本地真实图源"+rel)
    data=path.read_bytes()
    require(data.startswith(b"\x89PNG\r\n\x1a\n") and struct.unpack(">II",data[16:24])==(size,size),"PNG格式/尺寸不符"+rel)
    h=sha(path)
    require(h==obj["sha256"],"PNG证据哈希漂移"+rel)
    return {"relativePath":rel,"sha256":h}
def write_new_json(path,obj):
    txt=json.dumps(obj,ensure_ascii=False,indent=2)+"\n"
    if path.exists():
        require(path.read_text("utf-8-sig")==txt,"存在不同内容的同名清单，不能覆盖："+str(path))
        return "reused"
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(txt,"utf-8")
    return "created"
def main():
    ar=argparse.ArgumentParser(description=__doc__)
    ar.add_argument("--mode",choices=["inspect","apply"],default="inspect")
    args=ar.parse_args()
    frozen=json.loads(FREEZE.read_text("utf-8"))
    art=json.loads(DERIVED.read_text("utf-8"))
    require(frozen["heroId"]==art["heroId"]=="Hero.Zodiac.Dog", "英雄身份错误")
    require(len(frozen["skills"])==len(art["assets"])==8,"数量不等于八项")
    require([a["slotId"] for a in frozen["skills"]]==[
      "Primary","Slot1","Slot2","Slot4","Slot3","Slot5","Slot6","Slot7"],"特殊槽位顺序错误")
    stage={"schemaVersion":1,"heroId":"Hero.Zodiac.Dog","sourceKind":"RECOMPOSED_FROM_EXISTING_V1_1254",
           "humanArtApproved":False,"nativeUEAssetsCreated":False,"skills":[]}
    entries=[]
    for fr,row in zip(frozen["skills"],art["assets"]):
        require(fr["abilityId"]==row["abilityId"] and fr["texturePath"]==row["texturePath"],"技能ID或路径错误")
        a48=srcpng(row,"runtime48",48)
        a64=srcpng(row,"audit64",64)
        master=srcpng(row,"master1024",1024)
        stage["skills"].append({"abilityId":fr["abilityId"],"slotId":fr["slotId"],
                                "v6NameZh":fr["v6NameZh"],"files":{"runtime48":a48,"audit64":a64,"master1024":master}})
        entries.append({"order":fr["displayOrder"],"slotId":fr["slotId"],"abilityId":fr["abilityId"],
                        "displayNameZh":fr["productionV5NameZh"],
                        "v6ArtNameZh":fr["v6NameZh"],
                        "descriptionZh":"技能图标V6美术预览，禁止施放；机制与正式名称仍待批准。",
                        "texturePath":fr["texturePath"],
                        "png48":a48["relativePath"],"png64":a64["relativePath"],
                        "sha48":a48["sha256"],"sha64":a64["sha256"]})
    require(len({x["sha48"] for x in entries})==8,"存在完全重复的48像素正式图源")
    old=json.loads((PACK/"Docs/MonolithGenerationManifest.json").read_text("utf-8"))
    protected={}
    for f in old["SourceArtwork"]["Files"]:
        rel=f["SourceRelativePath"];path=ROOT/rel
        require(sha(path).lower()==f["Sha256"].lower(),"V1源文件变化："+rel)
        protected[rel]=sha(path)
    for f in old["Assets"]:
        rel=f["EngineFileRelativePath"];path=ROOT/rel
        require(sha(path).lower()==f["EngineFileSha256"].lower(),"V1原生资产变化："+rel)
        protected[rel]=sha(path)
    # 唯一占位Profile只允许真实编辑器进行增补；独立保存SHA回退快照。
    require((ROOT/PROFILE).is_file(),"正式英雄Profile原生文件缺失")
    plan={"schemaVersion":1,"projectCode":"SSLM","scope":"CLIENT_READ_ONLY_ART_PREVIEW",
          "humanArtApproved":False,"v6NameProductionApproved":False,
          "heroes":[{"hero":"Dog","heroId":"Hero.Zodiac.Dog",
                     "profilePath":"/DBAHeroPack_Dog/UI/Abilities/DA_DBA_Dog_PlaceholderUIProfile",
                     "profileFile":PROFILE,"entries":entries}]}
    if args.mode=="inspect":
        print(json.dumps({"status":"INPUTS_VALID","imageFiles":24,"v1FilesProtected":len(protected),
                          "plannedProfileEntries":len(entries),"willWrite":[str(STAGING),str(PLAN),str(PROTECT)]},ensure_ascii=False))
        return
    result={str(STAGING):write_new_json(STAGING,stage),str(PLAN):write_new_json(PLAN,plan),
            str(PROTECT):write_new_json(PROTECT,protected)}
    print(json.dumps({"status":"PREPARED","paths":result,"imagesVerified":24,"v1Protected":len(protected),
                      "uiAssetsWritten":0,"gasAbilitiesGranted":0},ensure_ascii=False))
if __name__=="__main__":
    main()
