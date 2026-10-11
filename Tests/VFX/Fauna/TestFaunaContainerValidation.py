#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""客户端/服务器容器审计的测试夹具；只验证解析规则，不宣称真实Cook通过。"""
import csv
from pathlib import Path
import tempfile
import unittest

from ValidateFaunaContainer import (
    required_client_assets, validate, read_manifest, is_fauna_asset,
)


class FaunaContainerValidationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def write_csv(self, name, platform, files):
        path = self.root / name
        with path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.writer(stream)
            writer.writerow(["Filename", "Platform"])
            writer.writerows((file, platform) for file in files)
        return path

    def test_required_assets_exactly_twenty(self):
        self.assertEqual(len(required_client_assets()), 20)
        self.assertEqual(len(set(required_client_assets())), 20)

    def test_client_success_across_two_containers(self):
        required = required_client_assets()
        one = self.write_csv("client1.csv", "WindowsClient", required[:10])
        two = self.write_csv("client2.csv", "WindowsClient", required[10:])
        result = validate([one, two], "Client")
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["requiredCount"], 20)
        self.assertEqual(result["containerCount"], 2)

    def test_missing_required_fails(self):
        files = required_client_assets()[:-1]
        result = validate([self.write_csv("missing.csv", "WindowsClient", files)], "Client")
        self.assertEqual(result["status"], "failed")
        self.assertEqual(len(result["missing"]), 1)

    def test_client_rejects_review_and_source(self):
        files = required_client_assets() + [
            "../../../Project/Content/Development/Fauna/Maps/L_DBA_FaunaReview.uasset",
            "../../../Project/Plugins/DivineBeasts/ContentPacks/Presentation/"
            "DBAPresentationPack_Core/SourceArt/Fauna/Meshes/Fish.fbx",
        ]
        result = validate([self.write_csv("badclient.csv", "WindowsClient", files)], "Client")
        self.assertEqual(result["status"], "failed")
        self.assertEqual(len(result["forbidden"]), 2)

    def test_server_excludes_all_fauna(self):
        server_map = "../../../Project/Plugins/DivineBeasts/ContentPacks/Worlds/"
        server_map += "DBAWorldPack_Village/Content/Maps/L_Village_Start.uasset"
        result = validate([self.write_csv("server.csv", "WindowsServer", [server_map])], "Server")
        self.assertEqual(result["status"], "passed")
        self.assertEqual(result["faunaFiles"], 0)

    def test_server_forbids_fauna_and_external_actor(self):
        files = [
            "../../../Project/Plugins/GamePlatform/Presentation/GamePlatformVFX/"
            "Content/Environment/Fauna/Niagara/NS_GP_Fauna_Base.uasset",
            "../../../Project/Content/__ExternalActors__/Development/Fauna/Maps/L_DBA_FaunaReview/ABC.uasset",
        ]
        result = validate([self.write_csv("badserver.csv", "WindowsServer", files)], "Server")
        self.assertEqual(len(result["forbidden"]), 2)

    def test_mixed_platform_rejected(self):
        p = self.write_csv("wrong.csv", "WindowsServer", ["a.uasset"])
        with self.assertRaises(ValueError):
            validate([p], "Client")

    def test_empty_manifest_rejected(self):
        p = self.write_csv("empty.csv", "WindowsServer", [])
        with self.assertRaises(ValueError):
            read_manifest(p, "WindowsServer")

    def test_bad_header_rejected(self):
        p = self.root / "header.csv"
        p.write_text("Filename,Filename\nhello,hello\n", encoding="utf-8")
        with self.assertRaises(ValueError):
            read_manifest(p, "WindowsClient")

    def test_no_manifest_rejected(self):
        with self.assertRaises(ValueError):
            validate([], "Server")

    def test_marker_recognition(self):
        self.assertTrue(is_fauna_asset("/Game/Development/Fauna/Maps/X"))
        self.assertFalse(is_fauna_asset("/Game/Maps/L_Village_Start"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
