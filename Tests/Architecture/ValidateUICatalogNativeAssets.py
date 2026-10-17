# -*- coding: utf-8 -*-
"""SSLM 公共与竞技 UI Catalog→原生控件蓝图文件的持续可复现门禁。

以版本库中的 C++ 页面目录、正式WBP和Monolith版本化记录为真源。
Saved/Monolith 下的32份旧设计样板是可选制作产物，不是现行UI必须存在的资产。
此脚本不执行引擎命令、不会制作/修改.uasset，也不把文件扫描冒充蓝图编译或设备验收。
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core"
PUBLIC = PACK / "Content/UI"
ARENA = ROOT / "Game/Plugins/DivineBeasts/DBAArena/Content/UI"
UI_CLIENT = ROOT / "Game/Plugins/DivineBeasts/DBAClient/Source/DivineBeastsUIClient"
PUBLIC_CATALOG = UI_CLIENT / "Private/Screens/DivineBeastsUIScreenCatalog.cpp"
ARENA_CATALOG = ROOT / ("Game/Plugins/DivineBeasts/DBAArena/Source/"
                        "DivineBeastsArenaClient/Private/UI/DivineBeastsArenaUIScreenCatalog.cpp")
LEGACY = ROOT / "Saved/Monolith/GameUIComponentSpecs/Manifest.json"
PARENTS = {
    "Boot": "DivineBeastsBootScreen",
    "Login": "DivineBeastsLoginScreen",
    "CharacterCreate": "DivineBeastsCharacterCreateScreen",
    "CharacterSelect": "DivineBeastsCharacterSelectScreen",
    "LoadingTravel": "DivineBeastsLoadingTravelScreen",
    "ErrorReconnect": "DivineBeastsErrorReconnectScreen",
    "SystemMenu": "DivineBeastsSystemMenuScreen",
    "Settings": "DivineBeastsDeviceSettingsScreen",
    "Inventory": "DivineBeastsInventoryScreen",
    "Quest": "DivineBeastsQuestScreen",
    "WorldJournal": "DivineBeastsWorldJournalScreen",
    "Social": "DivineBeastsSocialScreenBase",
    "LiveOps": "DivineBeastsLiveOpsScreenBase",
    "OpenWorldHUD": "DivineBeastsOpenWorldHUD",
    "VillageMainHUD": "DivineBeastsVillageHUD",
    "TutorialGuidance": "DivineBeastsTutorialHUD",
    "TrainingControls": "DivineBeastsTrainingHUD",
    "Toast": "DivineBeastsNotificationWidget",
}
ARENA_PARENTS = {
    "Matchmaking": "DivineBeastsMatchmakingScreen",
    "MatchFoundReady": "DivineBeastsMatchFoundReadyScreen",
    "HeroSelection": "DivineBeastsArenaHeroSelectionScreen",
    "Scoreboard": "DivineBeastsScoreboardScreen",
    "PostMatch": "DivineBeastsPostMatchResultScreen",
    "ArenaHUD": "DivineBeastsArenaHUD",
}
BIND_HEADERS = {
    "Login": "Screens/Login/DivineBeastsLoginScreen.h",
    "CharacterCreate": "Screens/Characters/DivineBeastsCharacterCreateScreen.h",
    "CharacterSelect": "Screens/Characters/DivineBeastsCharacterSelectScreen.h",
    "LoadingTravel": "Screens/Loading/DivineBeastsLoadingTravelScreen.h",
    "ErrorReconnect": "Screens/Connection/DivineBeastsErrorReconnectScreen.h",
    "SystemMenu": "Screens/System/DivineBeastsSystemMenuScreen.h",
    "Settings": "Screens/System/DivineBeastsDeviceSettingsScreen.h",
    "WorldJournal": "Screens/World/DivineBeastsWorldJournalScreen.h",
}


def catalog_entries(public: str, arena: str) -> list[dict[str, str]]:
    """仅识别C++稳定 MakeScreen/MakeHUD/MakeNotification 项，不把规格图片当蓝图。"""
    result: list[dict[str, str]] = []
    for kind, folder, matcher in (
        ("Screen", "Screens", "MakeScreen"),
        ("HUD", "HUD", "MakeHUD"),
        ("Notification", "Notifications", "MakeNotification"),
    ):
        pattern = re.compile(r'\b' + matcher + r'\(\s*TEXT\("([^"]+)"\),\s*TEXT\("([^"]+)"\)')
        for match in pattern.finditer(public):
            identity, name = match.groups()
            result.append({"id": identity, "owner": "public", "name": name,
                "parent": PARENTS.get(name, ""),
                "asset": str(PUBLIC / folder / ("WBP_DBA_UI_" + name + ".uasset"))})
    for match in re.finditer(r'\bMakeScreen\(\s*TEXT\("([^"]+)"\),\s*TEXT\("([^"]+)"\)', arena):
        identity, name = match.groups()
        result.append({"id": identity, "owner": "arena", "name": name,
            "parent": ARENA_PARENTS.get(name, ""),
            "asset": str(ARENA / "Screens" / ("WBP_DBA_UI_Arena_" + name + ".uasset"))})
    if re.search(r'\bMakeHUD\(\s*\)', arena):
        result.append({"id": "UI.HUD.Arena", "owner": "arena", "name": "ArenaHUD",
            "parent": ARENA_PARENTS["ArenaHUD"],
            "asset": str(ARENA / "HUD/WBP_DBA_UI_ArenaHUD.uasset")})
    return result


def required_bind_names(source: str) -> list[str]:
    """只核对C++的BindWidget必需字段；Optional缺失不构成生产资产错误。"""
    return re.findall(r'UPROPERTY\(meta=\(BindWidget\)\)\s*TObjectPtr<[^>]+>\s*(\w+)',
                      source, flags=re.S)


def audit(entries: list[dict[str, str]]) -> dict:
    issues: list[str] = []
    ids: set[str] = set()
    paths: set[str] = set()
    present = names_found = required = public_count = arena_count = 0
    for row in entries:
        ident = row["id"]
        if ident in ids or row["asset"] in paths:
            issues.append("重复ScreenId或资产路径：" + ident)
        ids.add(ident)
        paths.add(row["asset"])
        public_count += int(row["owner"] == "public")
        arena_count += int(row["owner"] == "arena")
        if not ident.startswith("UI.") or not row["parent"]:
            issues.append("目录身份或项目原生父类未定义：" + ident)
        path = Path(row["asset"])
        if not path.is_file() or path.stat().st_size <= 2500:
            issues.append("目录已登记但无真实WBP文件：" + ident)
            continue
        symbols = path.read_bytes().decode("latin-1")
        if row["parent"] not in symbols:
            issues.append("WBP未出现期望原生父类符号（UE反射待核）：" + ident)
        else:
            present += 1
        if row["owner"] == "public" and row["name"] in BIND_HEADERS:
            header = UI_CLIENT / "Public" / BIND_HEADERS[row["name"]]
            if not header.is_file():
                issues.append("BindWidget头不存在：" + str(header))
                continue
            for name in required_bind_names(header.read_text(encoding="utf-8-sig")):
                required += 1
                if name in symbols:
                    names_found += 1
                else:
                    issues.append("WBP缺少C++必需命名控件：" + ident + "/" + name)
    if public_count < 18 or arena_count < 6:
        issues.append("公共或竞技目录已登记页面数量低于历史已交付下限")
    root = PUBLIC / "Root/WBP_DBA_UI_RootLayout.uasset"
    if not root.is_file() or "DivineBeastsRootLayout" not in root.read_bytes().decode("latin-1"):
        issues.append("公共RootLayout资产或原生父类符号异常")
    audit_manifest = PACK / "Docs/MonolithGenerationManifest.json"
    if not audit_manifest.is_file():
        issues.append("版本管理的Monolith审计清单不存在")
    else:
        try:
            obj = json.loads(audit_manifest.read_text(encoding="utf-8-sig"))
            if not isinstance(obj, dict) or not obj.get("MonolithVersion"):
                issues.append("Monolith审计清单缺工具版本")
        except (OSError, ValueError) as err:
            issues.append("Monolith审计清单不可读取：" + str(err))
    editor = ROOT / "Game/Source/DivineBeastsArenaEditor.Target.cs"
    server = ROOT / "Game/Source/DivineBeastsArenaServer.Target.cs"
    if not editor.is_file() or not server.is_file():
        issues.append("Client/Server目标构建规则不完整")
    else:
        editor_text = editor.read_text(encoding="utf-8")
        server_text = server.read_text(encoding="utf-8")
        if 'EnablePlugins.Add("DBAUIPack_Core")' not in editor_text or (
                'EnablePlugins.Add("DBAArena")' not in editor_text):
            issues.append("Editor Target缺公共UI或竞技UI父类包")
        if 'EnablePlugins.Add("DBAUIPack_Core")' in server_text:
            issues.append("DedicatedServer显式启用了纯客户端UI内容包")
    return {
        "check": "SSLM现行公共/竞技UI目录与真实蓝图包",
        "publicCatalog": public_count, "arenaCatalog": arena_count,
        "registeredUI": len(entries), "nativeAssetParentNamePresent": present,
        "requiredBindWidgetCount": required, "requiredNamesFound": names_found,
        "historicalSpecs": ("optional_present" if LEGACY.is_file() else "optional_not_generated"),
        "issues": issues, "staticCheckPassed": not issues,
        "nativeBlueprintCompileVerified": False, "clientServerCookVerified": False,
        "meaning": "仅保证版本化目录/保存资产/控件名称合同，不能代替Monolith原生编译、重载、输入与客户端签收。",
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="SSLM UI 目录/蓝图静态资产门禁")
    parser.add_argument("--require-legacy-specs", action="store_true",
                        help="仅历史32件声明规格重建任务使用，缺Saved时明确拒绝")
    args = parser.parse_args()
    if args.require_legacy_specs and not LEGACY.is_file():
        parser.error("旧版Monolith声明式规格位于忽略的Saved目录，尚未生成")
    if not PUBLIC_CATALOG.is_file() or not ARENA_CATALOG.is_file():
        parser.error("真实公共或竞技UI目录源码缺失")
    entries = catalog_entries(PUBLIC_CATALOG.read_text(encoding="utf-8-sig"),
                              ARENA_CATALOG.read_text(encoding="utf-8-sig"))
    result = audit(entries)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(1 if result["issues"] else 0)


if __name__ == "__main__":
    main()
