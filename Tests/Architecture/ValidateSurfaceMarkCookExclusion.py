#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SurfaceMarks（局部表面痕迹）专用服务器声明/配置门禁。
读单源配置与真实产物目录，三个专服配置必须覆盖Mark目录；
本门禁不能替代真实UAT Cook/Stage和Pak/IoStore检查。
"""
from __future__ import annotations

import json
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
PACKAGE="/GamePlatformSurface/Marks"
CONTENT=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/Marks"
POLICY=ROOT/"Build/Game/ServerPresentationExclusions.json"
CONFIGS={
    "DedicatedServer":ROOT/"Game/Config/Custom/DedicatedServer/DefaultGame.ini",
    "VillageServer":ROOT/"Game/Config/Custom/VillageServer/DefaultGame.ini",
    "VillageDevelopmentServer":ROOT/"Game/Config/Custom/VillageDevelopmentServer/DefaultGame.ini",
}
PAK=ROOT/"Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini"

class SurfaceMarkCookBoundaryTest(unittest.TestCase):
    def test_policy_finite_exact_rule(self):
        doc=json.loads(POLICY.read_text(encoding="utf-8"))
        matches=[rule for rule in doc["CookDirectories"] if rule["PackagePath"]==PACKAGE]
        self.assertEqual(len(matches),1,"单源必须且只能有一条Marks目录")
        rule=matches[0]
        self.assertEqual(rule["Profiles"],"AllServers")
        self.assertEqual(rule["Availability"],"Present")
        self.assertEqual((ROOT/rule["SourceDirectory"]).resolve(),CONTENT.resolve())
        self.assertTrue(CONTENT.is_dir())
        files=sorted(CONTENT.rglob("*.uasset"))
        self.assertGreaterEqual(len(files),21)
        self.assertEqual(len([f for f in files if f.parent.name=="Textures"]),15)
        for f in files:
            self.assertEqual(f.read_bytes()[:4],bytes.fromhex("c1832a9e"))

    def test_server_ini_and_stage_rules(self):
        for profile,config in CONFIGS.items():
            with self.subTest(profile=profile):
                text=config.read_text(encoding="utf-8-sig")
                self.assertIn('DirectoriesToNeverCook=(Path="'+PACKAGE+'")',text)
                self.assertNotIn('DirectoriesToAlwaysCook=(Path="'+PACKAGE+'")',text)
        pak=PAK.read_text(encoding="utf-8-sig")
        self.assertIn("GamePlatformSurface/Content/Marks/",pak)

    def test_server_module_remains_client_only(self):
        path=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/GamePlatformSurface.uplugin"
        modules={m["Name"]:m for m in json.loads(path.read_text(encoding="utf-8"))["Modules"]}
        self.assertEqual(modules["GamePlatformSurfaceClient"]["Type"],"ClientOnly")
        self.assertNotIn("Server",modules["GamePlatformSurfaceClient"].get("TargetAllowList",[]))
        self.assertEqual(modules["GamePlatformSurfaceEditor"]["Type"],"Editor")

if __name__=="__main__":
    unittest.main(verbosity=2)
