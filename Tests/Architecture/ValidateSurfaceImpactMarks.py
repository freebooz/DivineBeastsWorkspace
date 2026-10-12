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

if __name__=="__main__":
    unittest.main(verbosity=2)
