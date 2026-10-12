#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""服务器表现资源策略的轻量行为回归，不启动UE，也不把人工夹具计作Cook证据。

先直接验证三个真实INI的差异，随后覆盖真源校验、受管段生成、命令行数组、
路径边界、外置对象、权威资源许可及空/损坏清单的失败语义。
所有合成文件仅写入临时目录；真实工作区在这些测试中只读。
"""
from __future__ import annotations
import copy
import csv
import importlib.util
import json
import re
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TOOL = ROOT / "Build/Game/ServerPresentationPolicy.py"
POLICY = ROOT / "Build/Game/ServerPresentationExclusions.json"
PROFILES = ("DedicatedServer", "VillageServer", "VillageDevelopmentServer")


def exclusions(profile: str) -> set[str]:
    """从实际INI读取新增排除，直接观察历史漏项，不依赖被测生成器。"""
    text = (ROOT / f"Game/Config/Custom/{profile}/DefaultGame.ini").read_text(encoding="utf-8-sig")
    return set(re.findall(r'^\+DirectoriesToNeverCook=\(Path="([^"]+)"\)', text, re.M))


class ConfigCoverageTests(unittest.TestCase):
    """删除Village的UI或开发服务器的BirdFlock/Snow排除必须使本组失败。"""

    def test_three_profiles_cover_same_required_presentation(self):
        common = exclusions("DedicatedServer")
        self.assertIn("/DBAUIPack_Core", common)
        self.assertIn("/GamePlatformVFX/Environment/BirdFlock", common)
        self.assertIn("/GamePlatformSurface/Textures/Snow", common)
        for profile in PROFILES:
            with self.subTest(profile=profile):
                self.assertFalse(common - exclusions(profile),
                                 f"{profile} 缺少服务器表现排除: {sorted(common - exclusions(profile))}")

    def test_development_authority_and_village_map_remain_enabled(self):
        # 保护候选权威技能、Village地图与AlwaysCook；它们不能被统一规则误删。
        text = (ROOT / "Game/Config/Custom/VillageDevelopmentServer/DefaultGame.ini").read_text(encoding="utf-8-sig")
        for expected in ('bAllowDevelopmentAbilitySets=true',
                         '-DirectoriesToNeverCook=(Path="/Game/Development/DivineBeasts/Abilities")',
                         '+DirectoriesToAlwaysCook=(Path="/Game/Development/DivineBeasts/Abilities")',
                         '+DirectoriesToAlwaysCook=(Path="/DBAWorldPack_Village")',
                         '+MapsToCook=(FilePath="/DBAWorldPack_Village/Maps/L_Village_Start")'):
            self.assertIn(expected, text)


class PolicyBehaviorTests(unittest.TestCase):
    """真实生产API的行为夹具；修改匹配/解析/生成语义会改变断言结果。"""

    def setUp(self):
        self.assertTrue(TOOL.is_file(), "缺少服务器资源策略执行器")
        self.assertTrue(POLICY.is_file(), "缺少服务器资源策略真源")
        spec = importlib.util.spec_from_file_location("server_policy_under_test", TOOL)
        self.api = importlib.util.module_from_spec(spec)
        import sys
        sys.modules[spec.name] = self.api
        spec.loader.exec_module(self.api)
        self.raw = json.loads(POLICY.read_text(encoding="utf-8-sig"))
        self.policy = self.api.load_policy(POLICY, ROOT)

    def rejected_policy(self, mutate):
        """仅改变临时JSON；危险真源必须在任何配置写入之前被拒绝。"""
        raw = copy.deepcopy(self.raw)
        mutate(raw)
        with tempfile.TemporaryDirectory(prefix="server-policy-test-") as folder:
            path = Path(folder) / "policy.json"
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaises(self.api.PolicyError):
                self.api.load_policy(path, ROOT)

    def test_all_profiles_and_foundation_alias_are_finite(self):
        for profile in PROFILES:
            self.assertEqual(self.policy.resolve_profile(profile), profile)
        self.assertEqual(self.policy.resolve_profile("FoundationStandalone"), "DedicatedServer")
        with self.assertRaises(self.api.PolicyError):
            self.policy.resolve_profile("FutureArenaServer")
        self.rejected_policy(lambda raw: raw["CookConfigProfiles"].update(FutureArenaServer="DedicatedServer"))

    def test_unknown_schema_and_unknown_fields_are_rejected(self):
        self.rejected_policy(lambda raw: raw.update(SchemaVersion=99))
        self.rejected_policy(lambda raw: raw.update(UnreviewedWildcard=True))

    def test_dangerous_generic_or_authority_paths_are_rejected(self):
        for path in ("/Game", "/Game/Development", "/DBAGameplay", "/DBAWorldPack_Village",
                     "/GamePlatformWeather", "/Game/Development/../Foundation", "/DBAHeroPack_*/UI"):
            with self.subTest(path=path):
                self.rejected_policy(lambda raw: raw["CookDirectories"][0].update(PackagePath=path))

    def test_duplicate_and_inconsistent_source_paths_are_rejected(self):
        self.rejected_policy(lambda raw: raw["CookDirectories"].append(copy.deepcopy(raw["CookDirectories"][0])))
        self.rejected_policy(lambda raw: raw["CookDirectories"][0].update(SourceDirectory="Game/Plugins"))

    def test_generated_block_is_idempotent_and_preserves_unrelated_text(self):
        profile = "VillageDevelopmentServer"
        original = (ROOT / f"Game/Config/Custom/{profile}/DefaultGame.ini").read_text(encoding="utf-8-sig")
        sentinel = "\n[Unrelated.Test]\nKey=unchanged\n"
        rendered = self.policy.render_config(profile, original + sentinel)
        self.assertTrue(rendered.endswith(sentinel))
        self.assertIn("bAllowDevelopmentAbilitySets=true", rendered)
        self.assertIn('+DirectoriesToAlwaysCook=(Path="/DBAWorldPack_Village")', rendered)
        self.assertEqual(rendered, self.policy.render_config(profile, rendered))

    def test_drift_detected_and_repair_changes_only_managed_block(self):
        original = (ROOT / "Game/Config/Custom/VillageServer/DefaultGame.ini").read_text(encoding="utf-8-sig")
        broken = original.replace('+DirectoriesToNeverCook=(Path="/DBAUIPack_Core")\n', "", 1)
        self.assertNotEqual(original, broken)
        repaired = self.policy.render_config("VillageServer", broken)
        self.assertEqual(original, repaired)
        with tempfile.TemporaryDirectory(prefix="server-policy-test-") as folder:
            root = Path(folder)
            for relative in self.policy.config_paths():
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((ROOT / relative).read_bytes())
            target = root / "Game/Config/Custom/VillageServer/DefaultGame.ini"
            target.write_text(broken, encoding="utf-8")
            with self.assertRaisesRegex(self.api.PolicyError, "VillageServer"):
                self.policy.check_configs(root)

    def test_missing_or_duplicated_markers_refuse_automatic_rewrite(self):
        original = (ROOT / "Game/Config/Custom/VillageServer/DefaultGame.ini").read_text(encoding="utf-8-sig")
        for broken in (original.replace(self.api.BEGIN_COOK, ""),
                       original + "\n" + self.api.BEGIN_COOK + "\n"):
            with self.subTest(broken=broken[-80:]):
                with self.assertRaises(self.api.PolicyError):
                    self.policy.render_config("VillageServer", broken)

    def test_outside_reset_cannot_cancel_managed_exclusions(self):
        original = (ROOT / "Game/Config/Custom/VillageServer/DefaultGame.ini").read_text(encoding="utf-8-sig")
        for directive in ('!DirectoriesToNeverCook=ClearArray',
                          '-DirectoriesToNeverCook=(Path="/DBAUIPack_Core")',
                          '-DirectoriesToNeverCook = (Path = "/DBAUIPack_Core")'):
            with self.subTest(directive=directive):
                with self.assertRaises(self.api.PolicyError):
                    self.policy.render_config("VillageServer", original + "\n" + directive + "\n")

    def test_pak_outside_override_is_drift(self):
        # END标记不会结束INI节；其后同名开关仍能覆盖受管规则，必须先拒绝。
        original = (ROOT / self.api.PAK_CONFIG).read_text(encoding="utf-8-sig")
        for suffix in ("\nbExcludeFromPaks = false\n",
                       "\n[excludeprojectclientpresentationfromdedicatedserver]\nbOverrideChunkManifest=false\n"):
            with self.subTest(suffix=suffix), self.assertRaises(self.api.PolicyError):
                self.policy.render_pak(original + suffix)

    def test_unrelated_section_with_similar_key_is_preserved(self):
        original = (ROOT / "Game/Config/Custom/VillageServer/DefaultGame.ini").read_text(encoding="utf-8-sig")
        suffix = "\n[Unrelated.Test]\n!DirectoriesToNeverCook=UnrelatedValue\n"
        self.assertEqual(self.policy.render_config("VillageServer", original + suffix), original + suffix)

    def test_stage_receipt_forbids_development_ui_for_every_profile(self):
        # 三profile共用DedicatedServer的Pak收据，最终产物禁入集合必须一致。
        for profile in PROFILES:
            self.assertTrue(self.policy.is_forbidden("/Game/Development/DivineBeasts/Abilities/UI/Profile.uasset", profile))

    def test_foundation_cooker_uses_distinct_add_tokens_without_clear(self):
        args = self.policy.cooker_arguments("FoundationStandalone")
        paths = self.policy.cook_paths("DedicatedServer")
        self.assertEqual(len(args), len(paths))
        self.assertEqual(len(set(args)), len(args))
        prefix = "-ini:Game:[/Script/UnrealEd.ProjectPackagingSettings]:+DirectoriesToNeverCook=(Path="
        self.assertEqual({argument[len(prefix):-1] for argument in args}, set(paths))
        self.assertTrue(all(argument.startswith(prefix) and argument.endswith(")") for argument in args))
        self.assertTrue(all('"' not in argument and "," not in argument and " " not in argument for argument in args))
        self.assertEqual(self.policy.cooker_arguments("VillageServer"), [])

    def test_paths_respect_directory_boundary_case_and_slash(self):
        denied = "DivineBeastsArena/Content/Development/Snow/Maps/Review.umap"
        self.assertTrue(self.policy.is_forbidden(denied, "DedicatedServer"))
        self.assertTrue(self.policy.is_forbidden("../../../" + denied.upper().replace("/", "\\"), "DedicatedServer"))
        self.assertFalse(self.policy.is_forbidden(denied.replace("/Snow/", "/Snowfall/"), "DedicatedServer"))
        with self.assertRaises(self.api.PolicyError):
            self.policy.is_forbidden("DivineBeastsArena/Content/Development/Snow/../Foundation/A.umap", "DedicatedServer")

    def test_external_actor_and_object_paths_follow_excluded_map_directory(self):
        for kind in ("__ExternalActors__", "__ExternalObjects__"):
            self.assertTrue(self.policy.is_forbidden(f"/Game/{kind}/Development/Snow/Maps/Review/A/123.uasset", "DedicatedServer"))
            self.assertTrue(self.policy.is_forbidden(f"../../../DivineBeastsArena/Content/{kind}/Development/Snow/Maps/Review/A/123.uasset", "DedicatedServer"))
            self.assertFalse(self.policy.is_forbidden(f"/Game/{kind}/Development/Snowfall/Map/A.uasset", "DedicatedServer"))

    def test_authoritative_data_collision_navigation_and_minimum_animation_are_allowed(self):
        # 仅使用现有权威所有者路径；不声称这些合成资产已存在或曾被Cook。
        for path in ("/DBAGameplay/Definitions/DA_Hero_Zodiac_Rat.uasset",
                     "/DBAGameplay/Abilities/DA_Ability.uasset",
                     "/DBAGameplay/Animations/MinimumAuthority.uasset",
                     "/DBAWorldPack_Village/Navigation/NavData.uasset",
                     "/DBAWorldPack_Village/Collision/WorldCollision.uasset",
                     "/GamePlatformWeather/Definitions/Weather.uasset",
                     "/Game/Development/DivineBeasts/Abilities/DA_Dev.uasset"):
            for profile in PROFILES:
                with self.subTest(path=path, profile=profile):
                    self.assertFalse(self.policy.is_forbidden(path, profile))

    def write_csv(self, folder, rows, header=("Filename", "ChunkType", "Size", "Platform")):
        """只写明示的测试清单；删除字段/更改目标/零尺寸会破坏验证证据。"""
        path = Path(folder) / "fixture.csv"
        with path.open("w", encoding="utf-8-sig", newline="") as handle:
            writer = csv.writer(handle)
            writer.writerow(header)
            writer.writerows(rows)
        return path

    def test_empty_or_damaged_csv_and_wrong_target_are_rejected(self):
        cases = [
            ([], ("Filename", "ChunkType", "Size", "Platform")),
            ([["one"]], ("Filename",)),
            ([["/DBAGameplay/Definitions/A.uasset", "ExportBundleData", "0", "WindowsServer"]], ("Filename", "ChunkType", "Size", "Platform")),
            ([["/DBAGameplay/Definitions/A.uasset", "ExportBundleData", "bad", "WindowsServer"]], ("Filename", "ChunkType", "Size", "Platform")),
            ([["/DBAGameplay/Definitions/A.uasset", "ExportBundleData", "80", "WindowsClient"]], ("Filename", "ChunkType", "Size", "Platform")),
            ([["", "ExportBundleData", "80", "WindowsServer"]], ("Filename", "ChunkType", "Size", "Platform")),
        ]
        for rows, header in cases:
            with self.subTest(rows=rows), tempfile.TemporaryDirectory(prefix="server-policy-test-") as folder:
                with self.assertRaises(self.api.PolicyError):
                    self.api.read_iostore_csv(self.write_csv(folder, rows, header))

    def test_valid_csv_is_allowed_but_forbidden_asset_fails(self):
        with tempfile.TemporaryDirectory(prefix="server-policy-test-") as folder:
            good = ["/DBAGameplay/Definitions/A.uasset", "ExportBundleData", "80", "WindowsServer"]
            path = self.write_csv(folder, [good])
            entries = self.api.read_iostore_csv(path)
            result = self.policy.validate_entries(entries, "DedicatedServer")
            self.assertEqual(result["AssetEntries"], 1)
            path = self.write_csv(folder, [good, ["/DBAUIPack_Core/UI/Login.uasset", "ExportBundleData", "40", "WindowsServer"]])
            with self.assertRaises(self.api.PolicyError):
                self.policy.validate_entries(self.api.read_iostore_csv(path), "DedicatedServer")

    def test_pak_listing_requires_complete_count_and_valid_entries(self):
        with tempfile.TemporaryDirectory(prefix="server-policy-test-") as folder:
            path = Path(folder) / "fixture.log"
            entry = 'LogPakFile: Display: "../../../DivineBeastsArena/Plugins/DivineBeasts/DBAGameplay/Content/Definitions/A.uasset" offset: 0, size: 80 bytes, sha1: ' + "0" * 40 + ', compression: None.\n'
            complete = entry + 'LogPakFile: Display: 1 files (80 bytes), (80 filtered bytes).\n'
            path.write_text(complete, encoding="utf-8")
            self.assertEqual(len(self.api.read_pak_listing(path)), 1)
            for broken in ("", entry, complete.replace("1 files", "2 files"), "LogPakFile: Error: corrupt\n" + complete):
                path.write_text(broken, encoding="utf-8")
                with self.assertRaises(self.api.PolicyError):
                    self.api.read_pak_listing(path)

    def test_empty_stage_and_unlisted_container_evidence_fail_closed(self):
        with tempfile.TemporaryDirectory(prefix="server-policy-test-") as folder:
            root = Path(folder)
            with self.assertRaises(self.api.PolicyError):
                self.api.audit_stage(self.policy, "DedicatedServer", root, root / "missing-UnrealPak.exe", root / "evidence")


if __name__ == "__main__":
    unittest.main(verbosity=2)
