#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM Dragon V6 8槽 UIProfile（技能界面配置）仅评审草案自动装配。

不创建第二个UPrimaryDataAsset，不更改原DA_DBA_Dragon_PlaceholderUIProfile；
仅以真实V6.1稳定AbilityId+当前V5已批准显示名+客户端现有Widget槽位，
形成可由Monolith人工签批后精确回填唯一UIProfile的八条待办映射。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
V6=ROOT/"Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.json"
REVIEW=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/"
              "SourceArt/UI/SkillIcons/V6/Review")
SELECTION=REVIEW/"DragonV6_HumanReviewSelections_Draft.json"
PROFILE=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/"
              "Content/UI/Abilities/DA_DBA_Dragon_PlaceholderUIProfile.uasset")
ABILITIES=ROOT/("Game/Plugins/DivineBeasts/DBAGameplay/Content/Abilities/Placeholders/"
                "DA_DBA_Dragon_PlaceholderAbilitySet.uasset")
OLD=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/"
           "Content/UI/Abilities")
OUTPUT=ROOT/"Docs/Implementation/GamePlatformAbilitySystem/DragonV6UIProfileBinding_Draft.json"
SLOTS=(
 ("Platform.Ability.Input.DivineBeasts.Primary","PrimaryAbilitySlot"),
 ("Platform.Ability.Input.DivineBeasts.Slot1","ActiveAbilitySlot1"),
 ("Platform.Ability.Input.DivineBeasts.Slot2","ActiveAbilitySlot2"),
 ("Platform.Ability.Input.DivineBeasts.Slot4","UltimateAbilitySlot"),
 ("Platform.Ability.Input.DivineBeasts.Slot3","MovementAbilitySlot"),
 ("Platform.Ability.Input.DivineBeasts.Slot5","DefenseAbilitySlot"),
 ("Platform.Ability.Input.DivineBeasts.Slot6","ControlAbilitySlot"),
 ("Platform.Ability.Input.DivineBeasts.Slot7","SignatureAbilitySlot"),
)
V1=(
 "T_DBA_Dragon_BasicAttack_DragonClaw_V1.uasset",
 "T_DBA_Dragon_Active01_DragonSpiral_V1.uasset",
 "T_DBA_Dragon_Active02_CloudBeam_V1.uasset",
 "T_DBA_Dragon_Ultimate_DragonAscension_V1.uasset",
 "T_DBA_Dragon_Passive_CloudHeart_V1.uasset",
)

def sha(path:Path)->str:
 h=hashlib.sha256()
 with path.open("rb") as file:
  for b in iter(lambda:file.read(1024*1024),b""):h.update(b)
 return h.hexdigest()

def load()->dict:
 d=json.loads(V6.read_text(encoding="utf-8-sig"))
 selection=json.loads(SELECTION.read_text(encoding="utf-8-sig"))
 if d.get("V5NamesStillProductionAuthority") is not True:
  raise RuntimeError("V5现有批准名称不再有效，请重新确认授权")
 hero=next((x for x in d["heroes"] if x["heroId"]=="Hero.Zodiac.Dragon"),None)
 if hero is None or len(hero["skills"])!=8 or len(selection["selection"])!=8:
  raise RuntimeError("龙V6八项及人工选择合同不完整")
 if not PROFILE.is_file() or not ABILITIES.is_file():
  raise RuntimeError("现有正式Profile/AbilitySet不在原稳定路径；不创建重名替代")
 entries=[]
 for i,row in enumerate(hero["skills"]):
  selection_row=selection["selection"][i]
  if selection_row["choiceABC"] is not None or selection_row["approval"]!="PENDING":
   raise RuntimeError("人工选择结果已更新，禁止用空图标草案覆盖正在执行的人审")
  if row["abilityId"]!=selection_row["abilityId"]:
   raise RuntimeError("项目稳定GAS ID与审核条目不匹配")
  entries.append({
    "position":i+1,
    "displaySlotId":row["slotId"],
    "inputGameplayTag":SLOTS[i][0],
    "widgetName":SLOTS[i][1],
    "AbilityId":row["abilityId"],
    "DisplayNameZh_productionV5":row["approvedDisplayNameV5"],
    "DisplayNameZh_proposedV6":row["proposedNameV61"],
    "DescriptionZh_reviewOnly":row["historicalV6EffectZh"],
    "Icon":None,
    "IconStatus":"PENDING_HUMAN_ICON_APPROVAL_AND_UE_TEXTURE2D",
    "GrantStatus":"SERVER_EIGHT_SKILL_V6_NATIVE_GAS_NOT_CONFIRMED",
    "readyForRelease":False,
  })
 originals={}
 for name in V1:
  path=OLD/name
  if not path.is_file():raise RuntimeError("原5枚V1技能Texture2D资产丢失："+name)
  originals[name]=sha(path)
 return {
  "schemaVersion":1,
  "heroDefinitionId":"Hero.Zodiac.Dragon",
  "purposeZh":"龙V6八项技能界面软引用映射草案，非第二个主资产",
  "productionV5NameAuthority":True,
  "reviewedV6MechanicsApproved":False,
  "primaryAssetIdentity":"DivineBeastsAbilityUIProfile:Hero.Zodiac.Dragon",
  "existingProductionProfilePath":"/DBAHeroPack_Dragon/UI/Abilities/DA_DBA_Dragon_PlaceholderUIProfile",
  "existingProductionProfileUAssetSha256":sha(PROFILE),
  "existingProductionAbilitySetFileSha256":sha(ABILITIES),
  "preservedOldV1Textures":originals,
  "productionAssetsModified":False,
  "newUETexture2DCount":0,
  "humanIconSelectionApproved":False,
  "entries":entries,
 }

def main():
 import argparse
 parser=argparse.ArgumentParser()
 parser.add_argument("--verify-only",action="store_true")
 args=parser.parse_args()
 data=load()
 if args.verify_only:
  if not OUTPUT.is_file() or json.loads(OUTPUT.read_text(encoding="utf-8"))!=data:
   raise RuntimeError("审查Profile草案与官方JSON/现行主资产SHA不符")
  print("SSLM_DRAGON_V6_UI_PROFILE_DRAFT_8_GAS_IDS_AND_5_V1_SHA_VERIFIED")
  return
 if OUTPUT.exists():raise FileExistsError("已存在Profile评审草案，拒绝覆盖")
 OUTPUT.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
 print("SSLM_DRAGON_V6_UI_PROFILE_DRAFT_CREATED abilities=8 approved=0 ueImported=0")

if __name__=="__main__":main()
