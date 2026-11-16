#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 丑牛八图标数据、质量与导入门禁的纯文本回归（不创建或修改UE资产）。"""
import json
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MANIFEST=ROOT/"Docs/Implementation/GamePlatformAbilitySystem/OxV6SkillIconImportManifest_20261018.json"
DESIGN=ROOT/"Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json"
REGISTRY=ROOT/"Docs/Implementation/GamePlatformAbilitySystem/V6SkillImplementationRegistry_20261018.json"
AUTHOR=ROOT/"Tools/Unreal/UI/ImportOxV6SkillIconsUE58.py"
PROFILE_H=ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Public/Definitions/DivineBeastsAbilityUIProfile.h"
VM=ROOT/"Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient/Private/ViewModels/Combat/DivineBeastsAbilityBarViewModel.cpp"

class OxV6SkillIconsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m=json.loads(MANIFEST.read_text(encoding="utf-8-sig"))
        cls.v=json.loads(DESIGN.read_text(encoding="utf-8-sig"))
        cls.r=json.loads(REGISTRY.read_text(encoding="utf-8-sig"))
        cls.author=AUTHOR.read_text(encoding="utf-8-sig")
    def test_identity_and_slot_order(self):
        ox=next(h for h in self.v["heroes"] if h["heroId"]=="Hero.Zodiac.Ox")
        correct=sorted(ox["skills"],key=lambda a:a["displayOrder"])
        m=self.m["items"]
        self.assertEqual(len(m),8)
        self.assertEqual([i["displayNameZh"] for i in m],
                         ["玄角撞","裂角","裂地踏","不动玄门","沉步","镇山护壁","顶回","立阵"])
        self.assertEqual([i["slotId"] for i in m],
                         ["Primary","Slot1","Slot2","Slot4","Slot3","Slot5","Slot6","Slot7"])
        self.assertEqual([i["abilityId"] for i in m],[a["abilityId"] for a in correct])
        self.assertEqual(len({i["abilityId"] for i in m}),8)
        self.assertEqual(len({i["targetTexturePath"] for i in m}),8)
        self.assertTrue(all(i["displayNameZh"]==c["proposedNameZh"] for i,c in zip(m,correct)))
    def test_v6_review_does_not_grant_unapproved_abilities(self):
        ox=[x for x in self.r["skills"] if x["heroId"]=="Hero.Zodiac.Ox"]
        self.assertEqual(len(ox),8)
        self.assertTrue(all(not x["canGrantToServer"] for x in ox))
        self.assertTrue(self.m["duplicatePrimaryAssetProfileForbidden"])
        self.assertEqual(self.m["productionAbilityGrantCountChanged"],0)
        self.assertEqual(self.m["currentOxPlaceholderEntriesShouldRemain"],6)
        self.assertEqual(self.m["manualReviewStatus"],"PENDING")
    def test_art_24_candidates_and_48_preview_contract(self):
        m=self.m["items"]
        self.assertEqual(sum(len(x["candidateFiles"]) for x in m),24)
        for row in m:
            self.assertTrue(row["preview48"].endswith("_Runtime48.png"))
            self.assertTrue(row["preview256"].endswith("_UI256.png"))
            self.assertTrue(row["preview512"].endswith("_Master512.png"))
            self.assertTrue(row["targetTexturePath"].startswith("/DBAHeroPack_Ox/UI/Abilities/V6/Textures/"))
            self.assertEqual(row["ueAssetState"],"NOT_IMPORTED")
        self.assertEqual(self.m["releaseGate"],"PENDING_ART_AND_GAMEPLAY_APPROVAL")
    def test_existing_vm_requires_current_granted_id(self):
        text=VM.read_text(encoding="utf-8-sig")
        for snippet in ["FindEntry(Grant.AbilityId)","if (bIconsReady && UI->Icon.IsValid())",
                        "AcquireResources(", "Snapshot.HeroDefinitionId", "ProfilePath = Assets->GetPrimaryAssetPath"]:
            self.assertIn(snippet,text)
        self.assertIn("GetPrimaryAssetId",PROFILE_H.read_text(encoding="utf-8-sig"))
        self.assertIn("profile.set_editor_property(\"entries\",old)",self.author)
        self.assertIn("len(placeholder)>=6",self.author)
        self.assertIn("manualReviewStatus",self.author)
        self.assertNotIn("ServerGrantAbility(",self.author)
    def test_author_does_not_claim_art_imported(self):
        archive=ROOT/self.m["pendingImageArchive"]
        if not archive.exists():
            self.assertTrue(all(i["ueAssetState"]=="NOT_IMPORTED" for i in self.m["items"]))
        self.assertIn("SSLM_OX_V6_IMPORT_WAITING_FOR_TRUSTED_ART_UPLOAD",self.author)

if __name__=="__main__":
    unittest.main(verbosity=2)
