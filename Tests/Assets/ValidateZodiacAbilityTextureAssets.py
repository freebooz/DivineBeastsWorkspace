#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
《神兽联盟》生肖技能纹理 UE 二进制资产与源图映射审计。
只读验证 12 个 HeroPack（英雄内容插件）下预定的 60 个纹理文件均真实存在，
并含有 UE Package（虚幻资产包）头标识；不复制、重命名或伪造 .uasset。
适用范围：文件系统结构证据。完整 Texture2D 反射类型、可加载性、压缩设置、
视觉效果、独立启动 Cook 仍须 Monolith / Unreal Editor（虚幻编辑器）实测。
"""
from __future__ import annotations

import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ICONS = ROOT / "Docs/Art/ZodiacSkillIconPrompts.json"
CONTENT_ROOT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes"
PACKAGE_MAGIC = bytes.fromhex("c1832a9e")


def main() -> int:
    manifest = json.loads(ICONS.read_text(encoding="utf-8"))
    entries = manifest["Icons"]
    expected_heroes = {
        "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
        "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar"
    }
    seen: set[str] = set()
    counts: Counter[str] = Counter()
    errors: list[str] = []
    total_size = 0
    for row in entries:
        hero = row["Hero"]
        asset_path = row["TextureAssetPath"]
        if hero not in expected_heroes:
            errors.append(f"未知英雄：{hero}")
            continue
        mount = f"/DBAHeroPack_{hero}/"
        if not asset_path.startswith(mount):
            errors.append(f"不属于当前英雄内容包的资源路径：{asset_path}")
            continue
        if asset_path in seen:
            errors.append(f"重复虚幻资产身份：{asset_path}")
            continue
        seen.add(asset_path)
        suffix = asset_path[len(mount):]
        relative = Path(suffix + ".uasset")
        if relative.is_absolute() or any(part == ".." for part in relative.parts):
            errors.append(f"不合法的资源相对路径：{asset_path}")
            continue
        file = CONTENT_ROOT / f"DBAHeroPack_{hero}" / "Content" / relative
        if not file.is_file():
            errors.append(f"纹理资产尚未导入：{file.relative_to(ROOT)}")
            continue
        with file.open("rb") as stream:
            header = stream.read(4)
        if header != PACKAGE_MAGIC or file.stat().st_size < 4096:
            errors.append(f"不是完整有效的基本UE资产包结构：{file.relative_to(ROOT)}")
            continue
        total_size += file.stat().st_size
        counts[hero] += 1
        if row.get("AbilityId") is not None:
            errors.append(f"未经策划批准不得创建生产技能身份：{asset_path}")
    if len(entries) != 60 or set(counts) != expected_heroes or any(c != 5 for c in counts.values()):
        errors.append(f"生肖技能纹理数量与目录不一致：{dict(counts)}")
    for error in errors:
        print(f"FAIL | {error}")
    if errors:
        print(f"RESULT | FAILED Errors={len(errors)}")
        return 1
    print("RESULT | PASS Heroes=12 UETexturePackageFiles=60 EachHero=5")
    print(f"UE_PACKAGE_BYTES={total_size}")
    print("PACKAGE_MAGIC=PASS (header check, not editor UAsset validation)")
    print("ABILITY_IDS_APPROVED=0")
    print("LIMITS | 仅验证已存在的UE包结构；完整UTexture2D/压缩/引用/Cook由Monolith另行验收。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
