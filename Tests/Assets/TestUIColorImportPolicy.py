# -*- coding: utf-8 -*-
"""原稿请求回归：颜色和遮罩不能共用sRGB设置，损坏源图/非法角色必须中止。
临时字节夹具仅验证政策，不生成图片或UE资产，不代替实际GPU验收。
"""
import hashlib
import importlib.util
from pathlib import Path
import tempfile
import unittest
spec = importlib.util.spec_from_file_location("ui_import_policy", Path(__file__).resolve().parents[2] / "Tools/Unreal/UI/ImportDBAThemeTexturesMonolith.py")
policy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(policy)
TEMP_ROOT = Path(__file__).resolve().parents[2] / "Saved/Validation/UIColorPolicyFixtures"
TEMP_ROOT.mkdir(parents=True, exist_ok=True)

class ImportPolicyTests(unittest.TestCase):
    def test_color_and_mask_have_separate_srgb_contracts(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as directory:
            root = Path(directory)
            (root / "source.png").write_bytes(b"readonly-source-fixture")
            item = {"Source": "source.png", "SHA256": hashlib.sha256(b"readonly-source-fixture").hexdigest(), "Destination": "/DBAUIPack_Core/UI/Textures/T_Test", "Role": "Color"}
            color = policy.prepare({"Files": [item]}, root)
            self.assertTrue(color[0]["params"]["srgb"])
            self.assertIn("TC_EditorIcon", [r["params"].get("value") for r in color])
            item["Role"] = "AlphaMask"
            self.assertFalse(policy.prepare({"Files": [item]}, root)[0]["params"]["srgb"])
            item["Role"] = "GuessFromFilename"
            with self.assertRaises(ValueError): policy.prepare({"Files": [item]}, root)
            item["Role"] = "Color"
            item["SHA256"] = "00" * 32
            with self.assertRaises(ValueError): policy.prepare({"Files": [item]}, root)

    def test_project_resources_cannot_enter_platform(self):
        with tempfile.TemporaryDirectory(dir=TEMP_ROOT) as directory:
            root = Path(directory)
            (root / "source.png").write_bytes(b"fixture")
            item = {"Source": "source.png", "SHA256": hashlib.sha256(b"fixture").hexdigest(), "Destination": "/GamePlatformUI/T_ProjectLogo", "Role": "Color"}
            with self.assertRaises(ValueError): policy.prepare({"Files": [item]}, root)

if __name__ == "__main__": unittest.main()
