#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM Village PCG技术模型UV修复的不可变中间源（仅开发验证）。

真实原始OBJ由GenerateVillagePrototypeMeshes.py及Git管理，绝不改动或覆盖。
UE5.8的Interchange OBJ导入器不接受f v//vn缺少UV索引的源结构；为测试导入，
只在工作空间Saved/Validation目录生成具备vt且面引用v/vt/vn的派生版本。
每顶点有独立三角面UV，非正式贴图展开；生成与检验均拒绝未授权来源和写入冲突。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import runpy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
GEN = runpy.run_path(str(ROOT / "Tools/Unreal/PCG/GenerateVillagePrototypeMeshes.py"))
SOURCE_FILES, SOURCE_MANIFEST = GEN["make_sources"]()
STAGE = ROOT / "Saved/Validation/GamePlatformPCG/TechnicalUVStageV1"
MANIFEST = "TechnicalUVStageManifest.json"


def digest(data: bytes) -> str:
    """原始和生成字节SHA-256均参与溯源，禁止只核对文件名。"""
    return hashlib.sha256(data).hexdigest()


def with_triangle_uv(obj_bytes: bytes) -> bytes:
    """不改变任何几何、材质或法线，只补UE OBJ翻译器需要的UV及面索引。"""
    text = obj_bytes.decode("utf-8")
    lines = text.splitlines()
    vertices = [line for line in lines if line.startswith("v ")]
    faces = [line for line in lines if line.startswith("f ")]
    if len(vertices) != len(faces) * 3 or not vertices:
        raise ValueError("技术OBJ必须保持独立且数量匹配的三角面顶点")
    if any(line.startswith("vt ") for line in lines):
        raise ValueError("仅允许对尚未含UV的初版技术源执行升级")
    # 每个三角形具有独立顶点：给每个顶点添加不会退化的局部UV。
    # 这是非发行技术审核所需的UV占位，不当作正式资产贴图展开。
    uv = ["vt 0.000000 0.000000", "vt 1.000000 0.000000", "vt 0.000000 1.000000"]
    uvs = [uv[index % 3] for index in range(len(vertices))]
    first_normal = next((i for i, line in enumerate(lines) if line.startswith("vn ")), -1)
    if first_normal < 0:
        raise ValueError("源OBJ缺少面法线")
    lines[first_normal:first_normal] = uvs
    repaired_faces = 0
    for i, line in enumerate(lines):
        if not line.startswith("f "):
            continue
        ids = []
        for token in line.split()[1:]:
            match = token.split("//")
            if len(match) != 2:
                raise ValueError("源OBJ面必须采用独立的顶点//法线合同")
            vertex, normal = map(int, match)
            if not (1 <= vertex <= len(vertices) and 1 <= normal <= len(faces)):
                raise ValueError("源OBJ面索引超出顶点或法线数量")
            ids.append(f"{vertex}/{vertex}/{normal}")
        if len(ids) != 3:
            raise ValueError("仅支持已批准三角面")
        lines[i] = "f " + " ".join(ids)
        repaired_faces += 1
    if repaired_faces != len(faces):
        raise ValueError("UV派生步骤遗失三角面")
    return ("\n".join(lines) + "\n").encode("utf-8")


def expected() -> dict[str, bytes]:
    """确定性生成派生OBJ/MTL与自身Manifest，保留原始源的内容哈希。"""
    generated = {}
    rows = []
    for source in SOURCE_MANIFEST["meshSources"]:
        name = source["relativePath"]
        original = SOURCE_FILES[name]
        from_repository = GEN["canonical_repository_text"](
            (GEN["ROOT"] / name).read_bytes()
        )
        if from_repository != original or digest(original) != source["sha256"]:
            raise ValueError("源OBJ与当前生成器/Manifest不一致：" + name)
        repaired = with_triangle_uv(original)
        generated[name] = repaired
        rows.append({
            "role": source["role"],
            "relativePath": name,
            "sourceSha256": digest(original),
            "stagedSha256": digest(repaired),
            "sourceTriangles": source["triangles"],
        })
    material = "VillagePCGPrototypeMaterials.mtl"
    generated[material] = SOURCE_FILES[material]
    data = {
        "schemaVersion": 1,
        "owner": "DBAWorldPack_Village",
        "sourceScope": "NonProductionTechnicalOnly",
        "cannotCook": True,
        "originalSourcesUnchanged": True,
        "stageDir": str(STAGE),
        "files": rows,
        "materialSha256": digest(generated[material]),
    }
    generated[MANIFEST] = (json.dumps(data, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    return generated


def main() -> int:
    parser = argparse.ArgumentParser(description="Village技术OBJ合法UV修复，存于Saved目录")
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--apply", action="store_true", help="首次新建完整阶段源，若已存在则拒绝覆盖")
    group.add_argument("--verify", action="store_true", help="只读验证阶段源的全部字节")
    flags = parser.parse_args()
    outputs = expected()
    if not STAGE.is_relative_to(ROOT / "Saved/Validation"):
        raise ValueError("UV派生文件不得写到正式内容包或Git跟踪的SourceArt")
    if flags.apply:
        if STAGE.exists():
            raise FileExistsError("技术UV阶段源已存在，拒绝覆盖：" + str(STAGE))
        STAGE.mkdir(parents=True)
        for name, data in outputs.items():
            (STAGE / name).write_bytes(data)
    if not STAGE.is_dir() or {p.name for p in STAGE.iterdir()} != set(outputs):
        raise RuntimeError("阶段源文件集合缺失或存在未知项")
    for name, data in outputs.items():
        if (STAGE / name).read_bytes() != data:
            raise RuntimeError("阶段源出现非预期字节差异：" + name)
    print("SSLM_VILLAGE_UV_STAGING_PASS", len(outputs) - 2, "stage", STAGE)
    print("TECHNICAL_ONLY_NEVER_COOK_NO_WORLD_ACTOR_OR_GAEA")


if __name__ == "__main__":
    raise SystemExit(main())
