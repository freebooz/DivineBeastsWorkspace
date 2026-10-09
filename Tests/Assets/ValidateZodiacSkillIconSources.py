#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
《神兽联盟》十二生肖技能图标源文件与制作清单一致性审计。
适用端：工具/CI（持续集成），不进入 UE 客户端或 Dedicated Server。
本脚本只读取原始 PNG、策划提示词和各英雄包 Monolith 清单；
不创建纹理 .uasset，不授予技能，也不冒充 Editor 导入、Cook 或人工视觉验收。
退出码：0=源文件和机器可审计元数据一致，1=存在缺失、重复、图像损坏或来源校验失败。
"""
from __future__ import annotations

import hashlib
import json
import struct
import sys
from collections import Counter
from pathlib import Path


WORKSPACE = Path(__file__).resolve().parents[2]
HERO_IDS = {
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse",
    "Goat", "Monkey", "Rooster", "Dog", "Boar",
}
SLOTS = {"BasicAttack", "Passive", "Active01", "Active02", "Ultimate"}
PROMPTS = WORKSPACE / "Docs/Art/ZodiacSkillIconPrompts.json"


def check_png(path: Path) -> tuple[int, int, str, int]:
    """读取 PNG 文件头和全部原始摘要；仅用于完整性，不进行视觉质量或版权鉴定。"""
    payload = path.read_bytes()
    if payload[:8] != b"\x89PNG\r\n\x1a\n" or payload[12:16] != b"IHDR":
        raise ValueError(f"不是有效的 PNG/IHDR：{path}")
    width, height = struct.unpack(">II", payload[16:24])
    if width <= 0 or height <= 0:
        raise ValueError(f"PNG 文件像素范围无效：{path}")
    return width, height, hashlib.sha256(payload).hexdigest().upper(), len(payload)


def main() -> int:
    """审核唯一归属、确切大小、内容摘要、技能身份空值和未交付与已交付状态的区分。"""
    spec = json.loads(PROMPTS.read_text(encoding="utf-8-sig"))
    entries = spec.get("Icons")
    if spec.get("SchemaVersion") != 1 or not isinstance(entries, list):
        raise ValueError("技能图标提示词清单结构版本或 Icons 列表非法")
    if spec.get("RequestedCount") != 60 or len(entries) != 60:
        raise ValueError("清单条目不是已确认的12生肖×5槽位共60枚")

    seen: set[tuple[str, str]] = set()
    asset_ids: set[str] = set()
    hashes: set[str] = set()
    per_hero = Counter()
    manifests: dict[str, dict] = {}
    total_bytes = 0

    for item in entries:
        hero = item["Hero"]
        slot = item["Slot"]
        if hero not in HERO_IDS or slot not in SLOTS:
            raise ValueError(f"英雄/槽位非法：{hero}/{slot}")
        if (hero, slot) in seen:
            raise ValueError(f"英雄槽位重复：{hero}/{slot}")
        seen.add((hero, slot))

        relative = item["SourceRelativePath"]
        path = (WORKSPACE / relative).resolve()
        expected_root = (WORKSPACE /
            f"Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_{hero}/SourceArt/UI/SkillIcons").resolve()
        if path.parent != expected_root or path.name != item["FileName"] or path.suffix.lower() != ".png":
            raise ValueError(f"图源未归属到声明的唯一英雄内容包：{hero}/{slot}")

        asset_name = item["TextureAssetPath"]
        if not asset_name.startswith(f"/DBAHeroPack_{hero}/UI/Abilities/") or asset_name in asset_ids:
            raise ValueError(f"引擎目标路径冲突或脱离内容包：{asset_name}")
        asset_ids.add(asset_name)
        if item.get("AbilityId") is not None:
            # 正式技能 ID 的独立策划审批不在图标阶段，防止从 ArtMotif 猜测生产协议身份。
            raise ValueError(f"图源清单不得自行批准生产 AbilityId：{hero}/{slot}")

        width, height, digest, size = check_png(path)
        if width != height or (width, height) != (1254, 1254):
            raise ValueError(f"图源像素尺寸不符合本批实际生成规格：{path.name}")
        if digest in hashes:
            raise ValueError(f"重复源图像摘要：{path.name}")
        hashes.add(digest)
        total_bytes += size
        per_hero[hero] += 1

        if hero not in manifests:
            manifest_path = WORKSPACE / f"Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_{hero}/Docs/MonolithGenerationManifest.json"
            manifests[hero] = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        manifest = manifests[hero]
        candidates = [entry for entry in manifest.get("SourceArtwork", {}).get("Files", [])
                      if entry.get("Hero") == hero and entry.get("Slot") == slot]
        if len(candidates) != 1:
            raise ValueError(f"英雄包证据缺少唯一槽位记录：{hero}/{slot}")
        row = candidates[0]
        if (row.get("Sha256", "").upper() != digest or row.get("Bytes") != size or
                row.get("Width") != width or row.get("Height") != height or
                row.get("SourceRelativePath") != relative or row.get("TextureAssetPath") != asset_name):
            raise ValueError(f"英雄包图源摘要/尺寸/名称不一致：{hero}/{slot}")

    if set(per_hero) != HERO_IDS or any(per_hero[hero] != 5 for hero in HERO_IDS):
        raise ValueError("十二生肖各五枚资源检查未全部覆盖")

    print("ART_SOURCE_FILES=60")
    print("HERO_COVERAGE=12/12")
    print("ICON_SLOTS_PER_HERO=5")
    print(f"PNG_TOTAL_BYTES={total_bytes}")
    print("PNG_SHA256_AND_PER_HERO_MANIFEST=PASS")
    print("STABLE_ABILITY_IDS=UNAPPROVED")
    print("LIMITS=Only source PNG checks; not UE Texture2D import, ability binding, Widget, Cook, gameplay or visual acceptance.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (KeyError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"ASSET_SOURCE_AUDIT_FAILED: {exc}", file=sys.stderr)
        sys.exit(1)
