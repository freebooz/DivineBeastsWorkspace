#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""战斗反馈发布就绪清单（纯只读，不代替UE编译、Monolith或Cook）。

用法：
    python Tests/Architecture/InspectCombatFeedbackDeliveryReadiness.py
    python Tests/Architecture/InspectCombatFeedbackDeliveryReadiness.py --require-ready

第一个命令只输出有证据的现状，退出0；第二个命令要求模块文件及已约定的
蓝图资源均存在，否则退出1。磁盘文件存在也不能证明BuildId、反射注册、
资产类型、Cook或联机正确，完整验收须由UE5.8编辑器实际回读。
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GAME = ROOT / "Game"
PLUGINS = GAME / "Plugins"

# 这里是正式46插件内的现有模块，不允许为了数量重新创建插件或Server表现模块。
EDITOR_MODULES = (
    ("GamePlatformAnimation", "GamePlatform/Gameplay/GamePlatformAnimation", "GamePlatformAnimation"),
    ("GamePlatformAnimationClient", "GamePlatform/Gameplay/GamePlatformAnimation", "GamePlatformAnimationClient"),
    ("GamePlatformCameraClient", "GamePlatform/Presentation/GamePlatformCamera", "GamePlatformCameraClient"),
    ("GamePlatformSFXClient", "GamePlatform/Presentation/GamePlatformSFX", "GamePlatformSFXClient"),
    ("GamePlatformVFXClient", "GamePlatform/Presentation/GamePlatformVFX", "GamePlatformVFXClient"),
    ("GamePlatformPresentationCore", "GamePlatform/Presentation/GamePlatformPresentation", "GamePlatformPresentationCore"),
    ("MobaPresentationRuntime", "MobaCommon/Presentation/MobaPresentation", "MobaPresentationRuntime"),
    ("MobaPresentationClient", "MobaCommon/Presentation/MobaPresentation", "MobaPresentationClient"),
    ("DivineBeastsPresentationRuntime", "DivineBeasts/DBAClient", "DivineBeastsPresentationRuntime"),
    ("DivineBeastsArenaClient", "DivineBeasts/DBAArena", "DivineBeastsArenaClient"),
)

FLOATING_TEXT = (
    GAME / "Plugins/DivineBeasts/ContentPacks/Presentation/"
    "DBAUIPack_Core/Content/UI/Combat/WBP_DBA_UI_FloatingCombatText.uasset"
)

def inspect() -> dict:
    project_manifest_path = GAME / "Binaries/Win64/UnrealEditor.modules"
    project_manifest = {}
    if project_manifest_path.is_file():
        try:
            project_manifest = json.loads(project_manifest_path.read_text(encoding="utf-8-sig"))
        except (ValueError, OSError):
            project_manifest = {}
    expected_build_id = project_manifest.get("BuildId")

    module_rows = []
    for label, folder, module in EDITOR_MODULES:
        bin_path = PLUGINS / folder / "Binaries/Win64" / f"UnrealEditor-{module}.dll"
        meta_path = PLUGINS / folder / "Binaries/Win64" / "UnrealEditor.modules"
        binary_present = bin_path.is_file() and bin_path.stat().st_size > 0
        plugin_manifest = {}
        if meta_path.is_file():
            try:
                plugin_manifest = json.loads(meta_path.read_text(encoding="utf-8-sig"))
            except (ValueError, OSError):
                plugin_manifest = {}
        declared_output = plugin_manifest.get("Modules", {}).get(module)
        matches_dll = declared_output == bin_path.name
        build_id_consistent = bool(
            expected_build_id and plugin_manifest.get("BuildId") == expected_build_id
        )
        module_rows.append({
            "module": label,
            "dll_exists": binary_present,
            "module_manifest_exists": meta_path.is_file(),
            "manifest_declares_module": matches_dll,
            "build_id_matches_project_editor": build_id_consistent,
            "disk_load_candidate": binary_present and matches_dll and build_id_consistent,
            "binary_relative_path": bin_path.relative_to(ROOT).as_posix(),
            # 只读磁盘不可能证明引擎当前反射类真正已加载，须Monolith重新验证。
            "editor_loaded": "待编辑器实证",
        })

    # 不从文件名称推断真实UClass。下面只提供待AssetRegistry校验的候选清单。
    candidate_assets = []
    for base in (
        PLUGINS / "GamePlatform/Presentation/GamePlatformPresentation/Content",
        PLUGINS / "DivineBeasts/DBAClient/Content",
        PLUGINS / "DivineBeasts/ContentPacks",
    ):
        if not base.is_dir():
            continue
        for file in base.rglob("*.uasset"):
            name = file.stem.lower()
            if "hitfeedback" in name or "combatfeedback" in name:
                candidate_assets.append(file.relative_to(ROOT).as_posix())

    missing_modules = [m["module"] for m in module_rows if not m["dll_exists"]]
    not_loadable = [m["module"] for m in module_rows if not m["disk_load_candidate"]]
    all_manifest_files = all(row["module_manifest_exists"] for row in module_rows)
    floating_widget_exists = FLOATING_TEXT.is_file() and FLOATING_TEXT.stat().st_size > 0
    return {
        "project": "DivineBeastsWorkspace（神兽联盟工作空间）",
        "component": "三层打击反馈编辑器二进制与内容就绪清单",
        "editor_dll_count": len(module_rows),
        "editor_dll_present": len(module_rows) - len(missing_modules),
        "editor_disk_load_candidate_count": len(module_rows) - len(not_loadable),
        "missing_editor_modules": missing_modules,
        "not_loadable_on_disk_modules": not_loadable,
        "project_editor_build_id": expected_build_id,
        "module_manifest_files_present": all_manifest_files,
        "floating_text_widget_file_present": floating_widget_exists,
        "floating_text_widget": FLOATING_TEXT.relative_to(ROOT).as_posix(),
        "feedback_asset_name_candidates": sorted(candidate_assets)[:50],
        "class_registry_validation": "未执行，必须用真正的UE5.8编辑器",
        "complete_module_and_widget_file_gate": not not_loadable and floating_widget_exists,
        "caveat": "DLL和uasset的存在性仅为磁盘门禁；不代表模块可加载、资源类型正确、UE编译或Cook及联机验收通过",
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="输出战斗打击反馈客户端交付就绪现状")
    parser.add_argument("--require-ready", action="store_true",
                        help="严格模式：缺失任一Editor模块或浮字Widget时失败")
    args = parser.parse_args()
    result = inspect()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 1 if args.require_ready and not result["complete_module_and_widget_file_gate"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
