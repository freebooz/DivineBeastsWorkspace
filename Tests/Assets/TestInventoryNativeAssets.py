"""SSLM背包UE5.8原生资产与三层资源边界静态验收。

只验证磁盘资产、索引和二进制格式；通过不代表UE运行时绑定、
PIE、Dedicated Server联机、真实PostgreSQL迁移或最终3A美术审核通过。
禁止此脚本生成/修复.uasset，所有UE资产由Monolith编辑器真实创建。
"""

from __future__ import annotations

import json
import struct
import unittest
from pathlib import Path


WORKSPACE = Path(__file__).resolve().parents[2]
UI_PACK = (
    WORKSPACE
    / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core"
)
ITEMS = {
    "PeachHerb": "item.herb.peach",
    "Mugwort": "item.herb.mugwort",
    "Lotus": "item.material.lotus",
    "GreenStone": "item.material.stone",
    "Peachwood": "item.material.peachwood",
    "HerbalPotion": "item.consumable.herbal_potion",
    "VillageBadge": "item.quest.village_badge",
    "SilkwormSilk": "item.material.silkworm_silk",
}


class InventoryNativeAssetsTest(unittest.TestCase):
    """背包资产存在性、路径一致性和客户端资源隔离。"""

    def assert_unreal_asset(self, path: Path) -> None:
        self.assertTrue(path.is_file(), f"缺少原生UE资产：{path}")
        self.assertGreater(path.stat().st_size, 1000, f"资产异常短：{path}")
        with path.open("rb") as binary:
            self.assertEqual(
                binary.read(4), bytes.fromhex("c1832a9e"),
                f"不是UE原生.uasset格式：{path}",
            )

    def test_eight_native_definitions_and_textures(self) -> None:
        for name in ITEMS:
            with self.subTest(item=name):
                self.assert_unreal_asset(
                    UI_PACK / f"Content/UI/Inventory/Definitions/DA_DBA_Item_{name}.uasset"
                )
                self.assert_unreal_asset(
                    UI_PACK / f"Content/UI/Inventory/Icons/T_DBA_Item_{name}.uasset"
                )

    def test_source_images_are_512_rgba(self) -> None:
        for name in ITEMS:
            with self.subTest(item=name):
                source = UI_PACK / f"SourceArt/UI/Inventory/T_DBA_Item_{name}.png"
                self.assertTrue(source.is_file(), f"缺少透明图标原图：{source}")
                with source.open("rb") as binary:
                    self.assertEqual(binary.read(8), b"\x89PNG\r\n\x1a\n")
                    length = struct.unpack(">I", binary.read(4))[0]
                    self.assertEqual(binary.read(4), b"IHDR")
                    self.assertEqual(length, 13)
                    width, height, depth, color = struct.unpack(
                        ">IIBB", binary.read(10)
                    )
                    self.assertEqual((width, height, depth, color), (512, 512, 8, 6))

    def test_eight_item_ids_match_backend(self) -> None:
        source = WORKSPACE / "Backend/configs/inventory_item_policies_v1.json"
        policy = json.loads(source.read_text(encoding="utf-8"))
        self.assertEqual(policy["gameId"], "divine-beasts")
        self.assertEqual(
            {item["itemDefinitionId"] for item in policy["items"]},
            set(ITEMS.values()),
        )

    def test_widget_and_manifest_evidence(self) -> None:
        self.assert_unreal_asset(
            UI_PACK / "Content/UI/Screens/WBP_DBA_UI_Inventory.uasset"
        )
        manifest = json.loads(
            (UI_PACK / "Docs/MonolithGenerationManifest.json").read_text(
                encoding="utf-8"
            )
        )
        inventories = [
            record for record in manifest["Assets"]
            if record.get("AssetPath")
            == "/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_Inventory"
        ]
        self.assertEqual(len(inventories), 1)
        snapshot = inventories[0]
        self.assertEqual(snapshot["WidgetCount"], 216)
        self.assertEqual(snapshot["NativeTexture2DCount"], 8)
        self.assertEqual(snapshot["NativeItemDefinitionDataAssetCount"], 8)
        self.assertEqual(snapshot["NativeSlotIconWidgets"], 32)
        self.assertEqual(snapshot["NativeQuickbarIconWidgets"], 12)
        self.assertEqual(snapshot["CompileErrors"], 0)
        visual = json.loads(
            (UI_PACK / "Docs/InventoryVisualAssets_20261013.json").read_text(
                encoding="utf-8"
            )
        )
        self.assertEqual(len(visual["items"]), 8)
        self.assertTrue(all(entry["资产已生成"] for entry in visual["items"]))

    def test_client_primary_asset_scan_does_not_force_server_cook(self) -> None:
        config = (WORKSPACE / "Game/Config/DefaultGame.ini").read_text(
            encoding="utf-8-sig"
        )
        self.assertIn(
            'PrimaryAssetType="GamePlatformInventoryItemDefinition"', config
        )
        self.assertIn(
            'Path="/DBAUIPack_Core/UI/Inventory/Definitions"', config
        )
        self.assertNotIn(
            'DirectoriesToAlwaysCook=(Path="/DBAUIPack_Core/UI/Inventory', config
        )
        server = (
            WORKSPACE / "Game/Source/DivineBeastsArenaServer.Target.cs"
        ).read_text(encoding="utf-8-sig")
        self.assertNotIn('EnablePlugins.Add("DBAClient")', server)
        self.assertNotIn('EnablePlugins.Add("DBAUIPack_Core")', server)


if __name__ == "__main__":
    unittest.main()
