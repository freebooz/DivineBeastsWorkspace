#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""表面痕迹P0/P1结构与源图只读门禁；与真实UE编译/Shader/视觉/Cook严格区分。"""
import hashlib
import json
import re
import unittest
from pathlib import Path

from PIL import Image
import numpy as np

ROOT=Path(__file__).resolve().parents[2]
PLUG=ROOT/"Game/Plugins/GamePlatform/Presentation/GamePlatformSurface"
SOURCES=PLUG/"Source/GamePlatformSurfaceClient"
KIND_NAMES=("Bullet","Scorch","Slash","Fluid","Crack")
RECORDS=ROOT/"Tools/Unreal/Surface/SurfaceMarkSourceManifest.json"

class SurfaceMarkStaticContracts(unittest.TestCase):
    def test_client_module_and_server_isolation(self):
        plugin=json.loads((PLUG/"GamePlatformSurface.uplugin").read_text(encoding="utf8"))
        modules={x["Name"]:x for x in plugin["Modules"]}
        self.assertEqual(set(modules),{"GamePlatformSurfaceClient","GamePlatformSurfaceEditor"})
        self.assertEqual(modules["GamePlatformSurfaceClient"]["Type"],"ClientOnly")
        self.assertEqual(modules["GamePlatformSurfaceEditor"]["Type"],"Editor")
        self.assertNotIn("Server",modules["GamePlatformSurfaceClient"]["TargetAllowList"])
        self.assertIn("Game", modules["GamePlatformSurfaceClient"]["TargetAllowList"])
        self.assertIn("Editor", modules["GamePlatformSurfaceClient"]["TargetAllowList"])
        self.assertNotIn("Server", modules["GamePlatformSurfaceEditor"]["TargetAllowList"])
        build=(SOURCES/"GamePlatformSurfaceClient.Build.cs").read_text(encoding="utf8")
        self.assertNotIn('"MobaPresentation"',build)
        self.assertNotIn('"DivineBeasts"',build)
        self.assertNotIn('"Niagara"',build)

    def test_five_kinds_and_bounded_world(self):
        types=(SOURCES/"Public/Marks/GamePlatformSurfaceMarkTypes.h").read_text(encoding="utf8")
        for name in KIND_NAMES:self.assertRegex(types,r"\b"+name+r"\s+UMETA")
        impl=(SOURCES/"Private/Marks/GamePlatformSurfaceMarkWorldSubsystem.cpp").read_text(encoding="utf8")
        for token in ("AcquireResources","ReleaseResources","EGamePlatformDataLifetime::World",
                      "NM_DedicatedServer","PendingRequests","RecentEventIds",
                      "MaxActiveMarks","SpawnDecalAttached","SpawnDecalAtLocation",
                      "MarkSpawned.Broadcast","DestroyComponent"):
            self.assertIn(token,impl,token)
        self.assertNotRegex(impl,r"\bTick\s*\(")
        self.assertNotIn("LoadSynchronous",impl)
        self.assertNotIn("TryLoad",impl)
        self.assertNotIn("UnloadPrimaryAsset",impl)
        self.assertIn("SourceArt",str(PLUG/"SourceArt"))

    def test_project_third_layer_and_game_target_contract(self):
        project=ROOT/"Game/Plugins/DivineBeasts/DBAClient"
        descriptor=json.loads((project/"DBAClient.uplugin").read_text(encoding="utf8"))
        deps={item["Name"]:item for item in descriptor["Plugins"]}
        self.assertIn("GamePlatformSurface",deps)
        self.assertEqual(set(deps["GamePlatformSurface"]["TargetAllowList"]),{"Client","Editor","Game"})
        rules=(project/"Source/DivineBeastsPresentationClient/DivineBeastsPresentationClient.Build.cs").read_text(encoding="utf8")
        self.assertIn('"GamePlatformSurfaceClient"',rules)
        adapter=(project/"Source/DivineBeastsPresentationClient/Private/Impact/DivineBeastsSurfaceImpactLibrary.cpp").read_text(encoding="utf8")
        for name in ("Impact.Projectile","Impact.Explosion","Impact.Slash","Impact.Energy","Impact.Fracture"):
            self.assertIn(name,adapter)
        for guard in ("ConfirmedEventId.IsValid()","ConfirmedHit.bBlockingHit","ECC_WorldStatic","ECC_WorldDynamic"):
            self.assertIn(guard,adapter)
        self.assertNotIn("SpawnDecalAtLocation",adapter)

    def test_client_cook_and_all_server_exclusion(self):
        # 客户端显式烘焙软目录；三类专用服务器从唯一策略真源强制排除。
        package='/GamePlatformSurface/Marks'
        for profile in ('FrontEndClient','VillageDevelopmentClient','PluginDemoClient'):
            config=ROOT/'Game/Config/Custom'/profile/'DefaultGame.ini'
            cooked=config.read_text(encoding='utf8')
            self.assertEqual(cooked.count('+DirectoriesToAlwaysCook=(Path="'+package+'")'),1,profile)
        policy=json.loads((ROOT/'Build/Game/ServerPresentationExclusions.json').read_text(encoding='utf8'))
        matches=[x for x in policy['CookDirectories'] if x['PackagePath']==package]
        self.assertEqual(len(matches),1)
        self.assertEqual(matches[0]['Profiles'],'AllServers')
        self.assertEqual(matches[0]['Availability'],'Present')
        self.assertEqual(matches[0]['SourceDirectory'],
            'Game/Plugins/GamePlatform/Presentation/GamePlatformSurface/Content/Marks')
        for server in ('DedicatedServer','VillageServer','VillageDevelopmentServer'):
            config=(ROOT/'Game/Config/Custom'/server/'DefaultGame.ini').read_text(encoding='utf8')
            self.assertEqual(config.count('+DirectoriesToNeverCook=(Path="'+package+'")'),1,server)
        self.assertNotIn('+DirectoriesToAlwaysCook=(Path="'+package+'")',
            (ROOT/'Game/Config/DefaultGame.ini').read_text(encoding='utf8'))

    def test_unreal_asset_author_no_overwrite_and_two_stage(self):
        import importlib.util
        source=ROOT/"Tools/Unreal/Surface/AuthorSurfaceMarksUEAssets.py"
        spec=importlib.util.spec_from_file_location("surface_mark_asset_author",source)
        author=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(author)
        self.assertTrue(author.validate_sources()["records"])
        self.assertIn("author-materials",source.read_text(encoding="utf8"))
        self.assertIn("verify-materials",source.read_text(encoding="utf8"))
        self.assertIn("-scripterrorsarefatal",source.read_text(encoding="utf8"))
        from tempfile import TemporaryDirectory
        from unittest.mock import patch
        with TemporaryDirectory() as directory:
            import pathlib
            root=pathlib.Path(directory)
            with patch.object(author,"CONTENT_ROOT",root):
                author.assert_native_package_not_occupied("/GamePlatformSurface/Marks/TestAsset")
                target=root/"Marks/TestAsset.uasset"
                target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(b"native asset test fixture only")
                with self.assertRaisesRegex(RuntimeError,"禁止覆盖"):
                    author.assert_native_package_not_occupied("/GamePlatformSurface/Marks/TestAsset")

    def test_five_material_sets_and_hashes(self):
        doc=json.loads(RECORDS.read_text(encoding="utf8"))
        self.assertEqual(doc["schemaVersion"],1)
        self.assertFalse(doc["formalAAAApproved"])
        self.assertEqual(len(doc["records"]),15)
        seen=set()
        for entry in doc["records"]:
            pair=entry["kind"],entry["role"]
            self.assertIn(pair[0],KIND_NAMES)
            self.assertIn(pair[1],("Albedo","Normal","ORM"))
            self.assertNotIn(pair,seen)
            seen.add(pair)
            path=(ROOT/entry["path"]).resolve()
            self.assertTrue(path.is_relative_to(PLUG))
            self.assertTrue(path.is_file())
            self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(),entry["sha256"])
            with Image.open(path) as im:
                self.assertEqual(im.size,(1024,1024))
                pixels=np.array(im)
            if entry["role"]=="Albedo":
                self.assertEqual(pixels.shape[-1],4)
                occupied=(pixels[:,:,3]>16)
                self.assertGreater(int(occupied.sum()),4000)
                self.assertLess(int(occupied.sum()),900000)
            elif entry["role"]=="Normal":
                self.assertEqual(pixels.shape[-1],3)
                self.assertGreater(float(pixels[:,:,2].mean()),180)
            elif entry["role"]=="ORM":
                self.assertEqual(pixels.shape[-1],3)
                self.assertGreater(float(pixels[:,:,1].mean()),45)
                self.assertLess(float(pixels[:,:,1].mean()),254)
        self.assertEqual(seen,{(k,r) for k in KIND_NAMES for r in ("Albedo","Normal","ORM")})

    def test_blueprint_and_authoring_contract(self):
        bp=(SOURCES/"Public/Marks/GamePlatformSurfaceMarkBlueprintLibrary.h").read_text(encoding="utf8")
        self.assertIn("SubmitSurfaceMarkFromHit",bp)
        self.assertIn("WorldContextObject",bp)
        author=(ROOT/"Tools/Unreal/Surface/AuthorSurfaceMarksUEAssets.py").read_text(encoding="utf8")
        self.assertIn("MD_DEFERRED_DECAL",author)
        self.assertIn("MaterialExpressionTextureSampleParameter2D",author)
        self.assertIn("unreal.GamePlatformSurfaceMarkCatalog",author)
        self.assertIn("replace_existing\",False",author)
        self.assertIn("SURFACE_MARK_UE_ASSETS_PASSED",author)
        self.assertIn("SURFACE_MARK_UE_MATERIALS_PASSED",author)

if __name__=="__main__":
    unittest.main(verbosity=2)
