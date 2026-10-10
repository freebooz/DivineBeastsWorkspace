#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生肖技能开发资产只读验收（不等于正式技能交付验收）。

作用域：DivineBeastsWorkspace（神兽联盟工作空间）内 /Game/Development/DivineBeasts/Abilities。
验证：真实磁盘引擎资产非空、开发/发布烘焙隔离、60 张图标索引与 12 英雄开发 UI 配置存在性。
限制：不能从 .uasset 原始字节代替 UE AssetRegistry、GameplayAbility 编译和联机测试。
不删除、不重命名、不生成、也不写入任何游戏资产。
"""
from __future__ import annotations

import argparse
import json
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEV_ASSETS = ROOT / "Game/Content/Development/DivineBeasts/Abilities"
CONFIG = ROOT / "Game/Config/DefaultGame.ini"
ICON_CATALOG = ROOT / "Docs/Art/ZodiacSkillIconPrompts.json"

SAMPLE_REQUIRED = {
    "DT_DBA_Rat_DevBalance.uasset": "开发技能等级数值表",
    "DA_DBA_Rat_DevPrimary.uasset": "开发技能逻辑定义",
    "GA_DBA_Rat_DevPrimary.uasset": "开发 GAS 技能蓝图",
    "DA_DBA_Rat_DevAbilitySet.uasset": "开发技能授权集",
    "UI/Profiles/DA_DBA_Rat_DevUIProfile.uasset": "开发技能显示配置（与服务器逻辑隔离）",
}
HEROES = (
    "Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
    "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar",
)
REQUIRED_DEV_ROOT = "/Game/Development/DivineBeasts/Abilities"


def verify(label: str, valid: bool, detail: str = "") -> bool:
    """逐项报告实际结论；错误作为布尔结果向上传递，禁止吞掉阻断。"""
    print(f"{'PASS' if valid else 'FAIL'} | {label}"
          + (f" | {detail}" if detail else ""))
    return valid


def inspect(*, require_all_profiles: bool, require_all_definitions: bool, verify_hashes: bool) -> int:
    """验证最小真资产链，并单独报告尚未制作的 UI 资产，绝不冒充正式技能内容。"""
    ok = True
    for filename, meaning in SAMPLE_REQUIRED.items():
        path = DEV_ASSETS / filename
        present = path.is_file() and path.stat().st_size > 500
        ok &= verify(f"子鼠样板：{meaning}", present, filename)
    config_text = CONFIG.read_text(encoding="utf-8-sig")
    never_cook = (
        f'+DirectoriesToNeverCook=(Path="{REQUIRED_DEV_ROOT}")' in config_text
    )
    scanned = (
        f'(Path="{REQUIRED_DEV_ROOT}")' in config_text
        and 'PrimaryAssetType="GamePlatformDefinition"' in config_text
    )
    ok &= verify("开发技能目录排除正式 Cook", never_cook)
    ok &= verify("现有 GamePlatformDefinition 扫描开发逻辑目录", scanned)
    catalog = json.loads(ICON_CATALOG.read_text(encoding="utf-8"))
    icons = catalog["Icons"]
    pairs = {(entry["Hero"], entry["Slot"]) for entry in icons}
    ok &= verify("十二生肖五槽图标源目录完整",
                 len(pairs) == 60 and len(icons) == 60
                 and {hero for hero, _ in pairs} == set(HEROES))
    rat_icon = next((
        x for x in icons if x["Hero"] == "Rat" and x["Slot"] == "BasicAttack"
    ), None)
    texture_path = (
        ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Heroes"
        / "DBAHeroPack_Rat/Content/UI/Abilities"
        / f"{rat_icon['TextureAssetPath'].rsplit('/', 1)[-1]}.uasset"
    ) if rat_icon else None
    ok &= verify("子鼠样板图标使用已导入的真实 Texture2D 文件",
                 texture_path is not None and texture_path.is_file()
                 and texture_path.stat().st_size > 500)
    profiles = [
        hero for hero in HEROES
        if (DEV_ASSETS / f"UI/Profiles/DA_DBA_{hero}_DevUIProfile.uasset").is_file()
    ]
    print(f"DEVELOPMENT_PROFILES={len(profiles)}/12; "
          f"MISSING={','.join(hero for hero in HEROES if hero not in profiles) or 'None'}")
    if require_all_profiles:
        ok &= verify("十二生肖开发 UIProfile 资产已真实创建", len(profiles) == 12)
    # 新增正式引擎生成的60个开发逻辑定义，只检查磁盘存在/大小；
    # UObject反射、逻辑ID、行引用和12×5图标仍需在真正UE编辑器内用Monolith验证。
    slots = ("BasicAttack", "Passive", "Active01", "Active02", "Ultimate")
    definition_files = []
    for hero in HEROES:
        for slot in slots:
            suffix = "Primary" if hero == "Rat" and slot == "BasicAttack" else slot
            definition_files.append(DEV_ASSETS / f"DA_DBA_{hero}_Dev{suffix}.uasset")
    actual_definition_files = [
        path for path in definition_files if path.is_file() and path.stat().st_size > 500
    ]
    print(f"DEVELOPMENT_DEFINITIONS={len(actual_definition_files)}/60")
    if require_all_definitions:
        ok &= verify("60份开发技能逻辑定义真实存在", len(actual_definition_files) == 60)
        common_table = DEV_ASSETS / "DT_DBA_Zodiac_DevBalance.uasset"
        ok &= verify("统一开发技能数值表真实存在",
                     common_table.is_file() and common_table.stat().st_size > 500)

    if verify_hashes:
        # 审计已归档的UE二进制文件摘要，杜绝旧清单误报“所有资源已保存”。
        manifest_file = ROOT / "Docs/Implementation/ZodiacDevelopmentUEAssetEvidence_20261009.json"
        manifest = json.loads(manifest_file.read_text(encoding="utf-8-sig"))
        registered = manifest.get("Assets", [])
        hashes_pass = (len(registered) == 76 and manifest.get("AssetCount") == 76)
        for entry in registered:
            file_path = ROOT / entry["RelativePath"]
            if not file_path.is_file():
                hashes_pass = False
                continue
            digest = hashlib.sha256(file_path.read_bytes()).hexdigest()
            hashes_pass &= digest == entry["SHA256"]
        ok &= verify("76份开发UE资产的归档SHA-256校验通过",
                     hashes_pass, f"Entries={len(registered)}")
        ok &= verify("开发清单明确不包含正式生产技能",
                     manifest.get("FormalAbilityDefinitionsApproved") == 0
                     and not manifest.get("ApprovedFormalAbilityIds"))
    print("SCOPE=DEVELOPMENT_ONLY; RELEASE_APPROVED=NO")
    print("REAL_ENGINE_RUNTIME_GRANT_AND_COOK=NOT_COVERED")
    return 0 if ok else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="只读核验生肖技能开发资源")
    parser.add_argument("--require-all-profiles", action="store_true",
                        help="同时要求十二生肖开发技能显示配置全部落盘")
    parser.add_argument("--require-all-definitions", action="store_true",
                        help="同时要求60份真实开发技能定义和统一数值表文件落盘")
    parser.add_argument("--verify-hashes", action="store_true",
                        help="重新核对已归档的76个UE资产的真实SHA-256")
    opts = parser.parse_args()
    raise SystemExit(inspect(require_all_profiles=opts.require_all_profiles,
                             require_all_definitions=opts.require_all_definitions,
                             verify_hashes=opts.verify_hashes))
