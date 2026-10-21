# -*- coding: utf-8 -*-
"""戌狗V6美术冻结及原有资产保护回归。

前置：工作空间保留V6审核源、V5名称批准表、戌狗冻结清单与V1创建记录。
输入：项目内JSON及文件SHA；输出：断言结果。缺少新导入美术时明确跳过
暂存图源检验，而不是把图源缺失报告为验收成功。不会生成/修改uasset。
"""
from pathlib import Path
import hashlib
import json
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
HERO_PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog"


def read_json(path):
    """只允许读取工作空间内既有证据文件。"""
    return json.loads(path.read_text(encoding="utf-8-sig"))


class DogV6ArtFreezeTests(unittest.TestCase):
    """美术身份冻结必须与原始V6和已批准V5严格一致。"""

    @classmethod
    def setUpClass(cls):
        cls.frozen = read_json(HERO_PACK / "Docs/DogV6_ArtFreeze_20261019.json")
        cls.design = read_json(ROOT / "Docs/Design/ZodiacHistoryFirstEightSkillsV6_Review.json")
        cls.v5 = read_json(ROOT / "Docs/Design/ApprovedSkillDisplayNames_V5.json")

    def test_eight_stable_ids_and_special_slot_order(self):
        """验证八技能完整、唯一且特别的第四Slot4第五Slot3不颠倒。"""
        hero = next(x for x in self.design["heroes"] if x["heroId"] == "Hero.Zodiac.Dog")
        expected = sorted(hero["skills"], key=lambda x: x["displayOrder"])
        frozen = self.frozen["skills"]
        self.assertEqual(len(frozen), 8)
        self.assertEqual([r["displayOrder"] for r in frozen], list(range(1,9)))
        self.assertEqual([r["slotId"] for r in frozen],
                         ["Primary","Slot1","Slot2","Slot4","Slot3","Slot5","Slot6","Slot7"])
        self.assertEqual(len({r["abilityId"] for r in frozen}), 8)
        for a,b in zip(frozen,expected):
            self.assertEqual(a["abilityId"],b["abilityId"])
            self.assertEqual(a["slotId"],b["slotId"])
            self.assertEqual(a["v6NameZh"],b["proposedNameZh"])
        self.assertEqual(self.frozen["passive"]["nameZh"], "守阙")
        self.assertFalse(self.frozen["passive"]["occupiesActiveSlot"])

    def test_v5_names_still_prod_and_no_gas_grant(self):
        """美术冻结不能重写现有V5显示名或伪造已授予的服务器技能。"""
        names = {x[0]:x[1] for x in self.v5["entries"]}
        self.assertEqual(self.frozen["status"],"USER_DIRECTED_V6_ART_ONLY_FREEZE")
        self.assertEqual(self.frozen["gasAuthorizationStatus"],"NOT_GRANTED")
        for r in self.frozen["skills"]:
            self.assertEqual(r["productionV5NameZh"], names[r["abilityId"]])
            self.assertTrue(r["texturePath"].startswith("/DBAHeroPack_Dog/UI/Abilities/"))
            self.assertTrue(r["textureAssetName"].startswith("T_DBA_Dog_S"))
        # V6反击/契约机制保留历史稳定ID，不能由旧治疗单词推测真实功能。
        self.assertEqual(self.frozen["skills"][7]["abilityId"],
                         "dba.ability.dog_eight_loyal_heal@1")

    def test_protected_v1_file_hashes(self):
        """原有五张源PNG及五张原生纹理的SHA与既有Monolith证据一致。"""
        history = read_json(HERO_PACK / "Docs/MonolithGenerationManifest.json")
        for item in history["SourceArtwork"]["Files"]:
            path = ROOT / item["SourceRelativePath"]
            self.assertTrue(path.is_file(), str(path))
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest().upper(),
                             item["Sha256"].upper(),str(path))
        for item in history["Assets"]:
            path = ROOT / item["EngineFileRelativePath"]
            self.assertTrue(path.is_file(),str(path))
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest().upper(),
                             item["EngineFileSha256"].upper(),str(path))

    def test_staging_png_when_present(self):
        """图源传入真实Windows工作空间后再检验尺寸、SHA、八项源所有权。"""
        stagefile = HERO_PACK / "Docs/DogV6_StagingManifest.json"
        planfile = ROOT / "Saved/Validation/DogV6Staging20261019/IntegrationPlan.json"
        if not stagefile.is_file() or not planfile.is_file():
            self.skipTest("真实暂存PNG尚未传入当前工作空间，跳过而非声称已导入")
        stage = read_json(stagefile)
        plan = read_json(planfile)
        self.assertEqual(stage["heroId"],"Hero.Zodiac.Dog")
        self.assertEqual(len(stage["skills"]),8)
        self.assertEqual(len(plan["heroes"][0]["entries"]),8)
        for x,entry in zip(stage["skills"],plan["heroes"][0]["entries"]):
            self.assertEqual(x["abilityId"],entry["abilityId"])
            for key,n in (("runtime48",48),("audit64",64)):
                f=x["files"][key]
                path=(ROOT / f["relativePath"]).resolve()
                self.assertTrue(path.is_relative_to(ROOT.resolve()))
                self.assertTrue(path.is_file(),str(path))
                data=path.read_bytes()
                self.assertTrue(data.startswith(b"\x89PNG\r\n\x1a\n"))
                self.assertEqual(struct.unpack(">II",data[16:24]),(n,n))
                self.assertEqual(hashlib.sha256(data).hexdigest(), f["sha256"])


if __name__=="__main__":
    unittest.main(verbosity=2)
