#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P5/P6 端侧启用与服务器权威设计约束（独立静态门禁，不伪称联机验证）。

只读校验插件依赖唯一性、主项目三目标启用、Production竞技未获批准前FailClosed、
开发审核图必须被明确Cook排除，并保留1v1~5v5固定2队的人数协议。
"""
from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TARGETS = {
    name: ROOT / f"Game/Source/DivineBeastsArena{name}.Target.cs"
    for name in ("Editor", "Client", "Server")
}
REVIEW = "/Game/Development/Foundation/PluginReviews"
GAMEPLAY = ROOT / "Game/Plugins/DivineBeasts/DBAGameplay/DBAGameplay.uplugin"
REQUIRED = (
    "GamePlatformAI",
    "GamePlatformNavigation",
    "GamePlatformInteraction",
    "GamePlatformQuest",
)


class P5P6AssemblyTests(unittest.TestCase):
    def test_gameplay_plugin_dependencies_have_unique_identity(self):
        """重复Name会改变UE插件装配语义，不能依靠数组第二份掩盖结构错误。"""
        config = json.loads(GAMEPLAY.read_text(encoding="utf-8-sig"))
        names = [x["Name"] for x in config["Plugins"]]
        self.assertEqual(len(names), len(set(names)))

    def test_p5_plugins_in_all_three_build_targets(self):
        """生产游戏实际使用AI/Navigation/Interaction/Quest，因此三个现有目标必须显式装配。"""
        for target, path in TARGETS.items():
            source = path.read_text(encoding="utf-8-sig")
            for name in REQUIRED:
                with self.subTest(target=target, plugin=name):
                    self.assertIn(f'EnablePlugins.Add("{name}")', source)

    def test_arena_gamemode_defaults_to_no_unverified_pawn(self):
        """实际出生必须等待合法比赛分配和项目可信英雄资格，不启用默认Pawn。"""
        gm = (ROOT / "Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformArena/Private/Framework/GamePlatformArenaGameMode.cpp").read_text(encoding="utf-8-sig")
        self.assertIn("DefaultPawnClass = nullptr", gm)
        self.assertIn("if (!bAssignmentApplied)", gm)
        self.assertIn("if (!VerifiedTicket.bConsumed)", gm)

    def test_project_arena_catalog_is_not_falsely_production_ready(self):
        """产品尚未签发生产ModeSpec前只能交付测试图及拒绝准入证据。"""
        src = (ROOT / "Game/Plugins/DivineBeasts/DBAArena/Source/DivineBeastsArenaRuntime/Private/Catalog/DivineBeastsArenaModeCatalog.cpp").read_text(encoding="utf-8-sig")
        self.assertIn("EDivineBeastsArenaConfigState::NotConfigured", src)
        self.assertIn("Spec.ValidateProduction(OutError)", src)

    def test_review_content_excluded_from_regular_cook(self):
        """开发测试图属于明确不Cook的Foundation子树，不能进入正式包。"""
        content = (ROOT / "Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
        self.assertIn('DirectoriesToNeverCook=(Path="/Game/Development/Foundation")', content)
        self.assertTrue(REVIEW.startswith("/Game/Development/Foundation/"))

    def test_builtin_arena_has_all_five_team_sizes(self):
        """五档容量来自正式ModeSpec，而非依据枚举整数推导。"""
        src = (ROOT / "Game/Plugins/MobaCommon/GamePlatformArena/Source/GamePlatformMobaData/Private/Definitions/GamePlatformArenaBuiltinModes.cpp").read_text(encoding="utf-8-sig")
        modes = re.findall(r'MakeMode\(TEXT\("Arena\.Mode\.(?:Duel1v1|Team[2345]v[2345])"\),\s*([1-5])\)', src)
        self.assertEqual(sorted(int(x) for x in modes), [1, 2, 3, 4, 5])


if __name__ == "__main__":
    unittest.main(verbosity=2)
