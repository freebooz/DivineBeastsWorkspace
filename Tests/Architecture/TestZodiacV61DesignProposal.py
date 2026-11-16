#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM十二生肖 V6.1 技能设计完整性与生产隔离门禁。

全部检查仅对设计方案与历史批准来源做只读比对，不调用UE、Monolith、资产作者，
不执行GAS授权或改变当前英雄默认技能集。通过不代表96项技能已实现。
"""
import copy
import hashlib
import importlib.util
import json
import unittest
from collections import Counter
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SOURCE=ROOT/"Tools/Unreal/Abilities/BuildZodiacV61DesignProposal.py"
spec=importlib.util.spec_from_file_location("sslm_zodiac_v61",SOURCE)
assert spec and spec.loader
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class ZodiacV61ProposalTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.v61=m.author()
  cls.skills=[s for h in cls.v61["heroes"] for s in h["skills"]]
  cls.v2=m.load(m.OLD_V2)
  cls.v5=m.load(m.V5)
  cls.old_v6=m.load(m.OLD_V6)
  cls.reg=m.load(m.REG)

 def test_01_12_hero_96_stable_ability_ids_and_12_passives(self):
  self.assertEqual(12,self.v61["heroCount"])
  self.assertEqual(96,self.v61["skillCount"])
  self.assertEqual(12,self.v61["innatePassives"])
  self.assertEqual(96,len({s["abilityId"] for s in self.skills}))
  self.assertTrue(all(len(h["skills"])==8 for h in self.v61["heroes"]))
  self.assertTrue(all(not h["passive"]["occupiesSlot"] for h in self.v61["heroes"]))
  self.assertTrue(all([s["slotId"] for s in h["skills"]]==m.SLOTS for h in self.v61["heroes"]))

 def test_02_names_v5_approved_stay_untouched_and_47_renames_pending(self):
  approved={r[0]:r[1] for r in self.v5["entries"]}
  self.assertEqual(47,self.v61["renamedCount"])
  self.assertEqual(47,len(self.v61["nameChangeReviewQueue"]))
  self.assertTrue(self.v61["V5NamesStillProductionAuthority"])
  for s in self.skills:
   self.assertEqual(approved[s["abilityId"]],s["approvedDisplayNameV5"])
   self.assertEqual(s["renameNeedsApproval"],s["approvedDisplayNameV5"]!=s["proposedNameV61"])
   self.assertEqual(s["nameChangeReviewed"],not s["renameNeedsApproval"])
  self.assertEqual({s["abilityId"] for s in self.skills},{r[0] for r in self.v5["entries"]})

 def test_03_v6_visual_and_runtime_scope_are_explicit_for_every_skill(self):
  for h in self.v61["heroes"]:
   self.assertTrue(h["passive"]["designZh"])
   self.assertTrue(h["visualDNAZh"])
   self.assertEqual(8,len(m.REVIEW[h["heroId"].split(".")[-1]]))
   for s in h["skills"]:
    with self.subTest(a=s["abilityId"]):
     self.assertGreater(len(s["authorityExecutionZh"]),8)
     self.assertGreater(len(s["runtimeBoundariesZh"]),10)
     self.assertTrue(s["criticalNativeTestsZh"])
     self.assertTrue(s["historicalV6EffectZh"])
     self.assertEqual(3,len(s["threeAngleVisualReviewZh"]))
     self.assertEqual(3,len(s["visualKeyframesFromV6ProposalZh"]))
     self.assertTrue(s["conceptArtPromptZh"])
     self.assertTrue(s["sfxPaletteProposalZh"])
     self.assertEqual(5,len(s["sfxRequiredCuesZh"]))
     self.assertIn("取消",s["criticalVFXSemanticsZh"])

 def test_04_52_primitive_candidates_are_not_falsely_reported_implemented(self):
  counts=self.v61["currentPrimitiveCandidateCounts"]
  self.assertEqual(44,counts["Unsupported"])
  self.assertEqual(96,sum(counts.values()))
  self.assertEqual(52,96-counts["Unsupported"])
  self.assertEqual(counts,dict(Counter(s["candidateNativePrimitiveSourceOnly"] for s in self.skills)))
  self.assertTrue(all(s["canGrantServer"] is False for s in self.skills))
  self.assertEqual(0,self.v61["serverAbilitiesApproved"])

 def test_05_all_unmade_native_assets_are_null_and_no_production_change(self):
  for s in self.skills:
   for field in ("nativeDefinitionPath","nativeGAPath","nativeGEPath",
                 "nativeNiagaraPath","nativeSFXPath","nativeMontagePath","nativeIcon48Path"):
    self.assertIsNone(s[field],f"未制作的UE资源不能伪造路径：{s['abilityId']}:{field}")
   self.assertFalse(s["gameplayReviewed"])
   self.assertFalse(s["balanceReviewed"])
   self.assertFalse(s["visualHumanApproved"])
   self.assertFalse(s["presentationCanAlterServerOutcome"])
  self.assertFalse(self.v61["newUEAssetsCreated"])
  self.assertTrue(self.v61["noGameplaySystemChange"])

 def test_06_unapproved_balance_keeps_energy_and_magic_momentum_zero(self):
  self.assertTrue(all(s["level1V2ReferenceNotApproved"]["energyCost"]==0 for s in self.skills))
  self.assertTrue(all(s["level1V2ReferenceNotApproved"]["momentumCost"]==0
      for s in self.skills if s["level1V2ReferenceNotApproved"]["damage"]==0
      or s["level1V2ReferenceNotApproved"]["damage"]>0 and
         next(x for h in self.v2["heroes"] for x in h["slots"]
              if x["abilityId"]==s["abilityId"])["damageType"]=="Magic"))
  self.assertEqual(7,sum(s["level1V2ReferenceNotApproved"]["momentumCost"]>0 for s in self.skills))
  self.assertTrue(all(0 < s["suggestedWindupSecondsNotApproved"]<=0.65 for s in self.skills))

 def test_07_conflicting_four_healer_roles_explicitly_unapproved(self):
  names={h["heroId"].split(".")[-1]:h for h in self.v61["heroes"]}
  for code,marker in (("Rabbit","自疗"),("Goat","治疗"),("Rooster","副治疗"),("Dog","护卫")):
   skill=names[code]["skills"][-1]
   self.assertIn(marker,self.v61["teamRoleDecisionCandidateZh"])
   self.assertFalse(skill["gameplayReviewed"])
  self.assertIn("残像",names["Rabbit"]["skills"][-1]["historicalV6EffectZh"])
  self.assertIn("自疗",names["Rabbit"]["skills"][5]["historicalV6EffectZh"])
  self.assertIn("万愿归铃",names["Goat"]["skills"][3]["proposedNameV61"])
  self.assertIn("副治疗",names["Rooster"]["skills"][-1]["runtimeBoundariesZh"])
  self.assertIn("绑定",names["Dog"]["skills"][-1]["authorityExecutionZh"])

 def test_08_source_hashes_and_generated_documents_are_reproducible(self):
  for path in (m.OLD_V6,m.OLD_V2,m.V5,m.REG):
   self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),self.v61["sources"][path.name])
  files=m.expected()
  self.assertEqual(set(files),{m.OUT_MD,m.OUT_JSON})
  for path,content in files.items():
   self.assertTrue(path.is_file(),path)
   self.assertEqual(content,path.read_text(encoding="utf-8"))
  self.assertGreater(len(files[m.OUT_MD]),24000)
  self.assertIn("十二生肖",files[m.OUT_MD])

 def test_09_no_old_five_element_damage_or_king_seal_reintroduced(self):
  text="\n".join(s["authorityExecutionZh"]+" "+s["runtimeBoundariesZh"] for s in self.skills)
  for forbidden in ("王印结算","三合六合","五行克制","破元伤害","共鸣增幅"):
   self.assertNotIn(forbidden,text)

 def test_10_no_silently_authorized_historical_version_assets(self):
  self.assertFalse(self.old_v6["productionChangesMade"])
  self.assertEqual(0,self.old_v6["nativeAssetsCreated"])
  self.assertEqual(0,self.reg["nativeUEAssetCount"])
  self.assertEqual(0,self.reg["serverApprovedAbilityCount"])
  self.assertTrue(all(s["candidateNativePrimitiveSourceOnly"] in self.reg["primitiveModesImplementedAsSourceOnly"]
                      or s["candidateNativePrimitiveSourceOnly"]=="Unsupported" for s in self.skills))

if __name__=="__main__":
 unittest.main(verbosity=2)
