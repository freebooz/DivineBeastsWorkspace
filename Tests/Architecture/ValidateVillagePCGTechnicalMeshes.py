#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""神兽联盟PCG技术静态模型的源几何和发行隔离合同检查。

不会写项目资产，不调用UE。检验OBJ几何合法性、厘米范围、材质名称、
源SHA、全部六种平台领域，以及V1正式MeshSet映射不能指向技术原型。
"""
from __future__ import annotations
import json
import math
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ART = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
              "DBAWorldPack_Village/SourceArt/PCG/TechnicalPrototype_v1")
GEN = runpy.run_path(str(ROOT / "Tools/Unreal/PCG/GenerateVillagePrototypeMeshes.py"))
IMPORT = runpy.run_path(str(ROOT / "Tools/Unreal/PCG/ImportVillageTechnicalPrototypes.py"))
PRODUCTION = runpy.run_path(str(ROOT / "Tools/Unreal/PCG/VerifyVillagePCGProductionSources.py"))

def main() -> None:
    source_data, expected_manifest = GEN["make_sources"]()
    found = {p.name for p in ART.iterdir() if p.is_file()}
    assert found == set(source_data), "技术OBJ来源文件清单不完整或含陌生文件"
    for name, data in source_data.items():
        actual = GEN["canonical_repository_text"]((ART / name).read_bytes())
        assert actual == data, "规范化Git文本换行后，固定生成器与源内容仍不一致：" + name
        assert GEN["canonical_repository_text"](data.replace(b"\n", b"\r\n")) == data
        assert IMPORT["sha"](ART / name) == GEN["hashlib"].sha256(data).hexdigest()
    saved = json.loads((ART / "VillagePCGTechnicalSourceManifest_v1.json").read_text(encoding="utf-8"))
    assert saved == expected_manifest
    assert saved["sourceKind"] == "procedural_technical_prototype_not_approved"
    assert saved["humanArtApproval"] == "pending" and not saved["unrealAssetCreated"]
    assert len(saved["meshSources"]) == 6
    assert {r["role"] for r in saved["meshSources"]} == set(IMPORT["EXPECTED"])
    for row in saved["meshSources"]:
        lines = (ART / row["relativePath"]).read_text(encoding="utf-8").splitlines()
        vertices = [line.split() for line in lines if line.startswith("v ")]
        normals = [line.split() for line in lines if line.startswith("vn ")]
        faces = [line.split() for line in lines if line.startswith("f ")]
        assert len(vertices) == len(faces) * 3 == row["triangles"] * 3
        assert len(normals) == len(faces)
        assert all(len(vertex) == 4 and all(math.isfinite(float(x)) for x in vertex[1:])
                   for vertex in vertices)
        assert all(len(face) == 4 for face in faces)
        for i, face in enumerate(faces, 1):
            ids = [item.split("//") for item in face[1:]]
            assert [int(t[1]) for t in ids] == [i, i, i]
            assert [int(t[0]) for t in ids] == [3*i - 2, 3*i - 1, 3*i]
        assert row["triangles"] >= 20
        assert row["boundsCm"][2][0] >= -0.01
        tech_asset = "/DBAWorldPack_Village/Development/PCG/Meshes/SM_DBA_PCG_" + row["role"] + "_Tech"
        assert PRODUCTION["normalize"](tech_asset) is None, (
            "开发技术网格不能通过正式生产来源准入：" + tech_asset)
    packaging = (ROOT / "Game/Config/DefaultGame.ini").read_text(encoding="utf-8-sig")
    assert '+DirectoriesToNeverCook=(Path="/DBAWorldPack_Village/Development/PCG")' in packaging
    importer_text = (ROOT / "Tools/Unreal/PCG/ImportVillageTechnicalPrototypes.py").read_text(encoding="utf-8")
    review_text = (ROOT / "Tools/Unreal/PCG/AuthorVillagePrototypeReviewMap.py").read_text(encoding="utf-8")
    assert "unreal.StaticMeshEditorSubsystem" in importer_text
    assert "mesh_editor.get_simple_collision_count(mesh)" in importer_text
    assert "mesh_editor.remove_collisions(mesh)" in importer_text
    assert 'collision_count != 0' in importer_text
    assert "unreal.CollisionEnabled.NO_COLLISION" in review_text
    pipeline = (ROOT / "Tools/Unreal/PCG/RunVillagePrototypeImport.ps1").read_text(encoding="utf-8-sig")
    assert "'ReviewApply','ReviewVerify'" in pipeline
    assert "AuthorVillagePrototypeReviewMap.py" in pipeline
    assert "UE_PCG_SIX_REAL_STATIC_MESHES_MAP_REOPEN_OK" in pipeline
    assert "UE_PCG_SIX_TECH_MESHES_REAL_MAP_SAVED" in pipeline
    assert "-ExecutePythonScript=" in pipeline
    assert "unreal.StaticMeshActor" in review_text and "levels.save_current_level()" in review_text
    # 来源字段是项目目标语义，并非表示原型已完成Gameplay碰撞；正式地图不可复用临时网格碰撞。
    print("VILLAGE_PCG_SIX_TECH_MESH_SOURCE_AND_NEVER_COOK_GATE_PASS")
    print("TRIANGLES", sum(row["triangles"] for row in saved["meshSources"]))
    print("TECH_SOURCE_GOLD_ONLY_NO_GAEA_NO_UE_ASSET_NO_3A_ART_APPROVAL")


if __name__ == "__main__":
    main()
