"""核对真实IoStore中本次技能绑定的发布闭包，不代表联机或人工视效验收。"""
import argparse
import csv
import json
from pathlib import Path

HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse",
          "Goat", "Monkey", "Rooster", "Dog", "Boar")


def inventory(path):
    with Path(path).open(encoding="utf-8-sig", newline="") as source:
        rows = [{key.strip(): value.strip() for key, value in row.items()}
                for row in csv.DictReader(source)]
    assert rows and "Filename" in rows[0] and "ChunkType" in rows[0], "缺少真实IoStore清单"
    return {row["Filename"].replace("\\", "/") for row in rows
            if row["ChunkType"] == "ExportBundleData"}


def contains(names, suffix):
    return any(name.endswith("/" + suffix) for name in names)


def inspect(client_csv, server_csv, cold_report):
    client = inventory(client_csv)
    server = inventory(server_csv)
    cold = json.loads(Path(cold_report).read_text(encoding="utf-8-sig"))
    assert cold["status"] == "PASS" and cold["heroCount"] == 12
    # 使用独立UE重载返回的实际资源路径，并确认运行资源不依赖审核目录。
    runtime = []
    editor_scripts = []
    for object_path in cold["loaded"]:
        package = object_path.split(".")[0].lstrip("/")
        mount, relative = package.split("/", 1)
        suffix = mount + "/Content/" + relative + ".uasset"
        # UNiagaraScript::IsEditorOnly：独立Module是作者源；编译结果随System Cook，Module本身不发布。
        # 精确枚举三份作者模块，不把任意缺失运行资源当作正常剥离。
        if relative.rsplit("/", 1)[-1] in ("NM_DBA_MissileRuntime_Head", "NM_DBA_MissileRuntime_TrailFade", "NM_DBA_MissileRuntime_TrailSpawn"):
            editor_scripts.append(suffix)
        else:
            runtime.append(suffix)
    runtime.append("DBAPresentationPack_Core/Content/VFX/DevelopmentMagic/Definitions/DA_DBA_VFX_DirectedMissileFlight.uasset")
    assert len(set(runtime)) == 10 and len(set(editor_scripts)) == 3, "运行包/作者模块闭包数量异常"
    authority = ["DBAGameplay/Content/Abilities/Placeholders/DT_DBA_PlaceholderBalance.uasset"]
    ui = []
    for hero in HEROES:
        root = "DBAGameplay/Content/Abilities/Placeholders/"
        authority.append(root + f"DA_DBA_{hero}_PlaceholderAbilitySet.uasset")
        for mode in ("Primary", "DirectedMissile"):
            authority.extend(root + f"{prefix}_DBA_{hero}_Placeholder{mode}.uasset"
                             for prefix in ("DA", "GA"))
            authority.append(root + f"GE_DBA_{hero}_Placeholder{mode}Cooldown.uasset")
        ui.append(f"DBAHeroPack_{hero}/Content/UI/Abilities/DA_DBA_{hero}_PlaceholderUIProfile.uasset")
    client_missing = [name for name in authority + runtime + ui if not contains(client, name)]
    server_missing = [name for name in authority if not contains(server, name)]
    editor_script_leaks = [name for name in editor_scripts if contains(client, name)]
    review_leaks = sorted(name for name in client
                          if "/VFX/CommonSkillVFX/" in name and "/Runtime/DirectedMissile/" not in name)
    server_leaks = sorted(name for name in server
                          if "/DBAPresentationPack_Core/Content/VFX/" in name)
    return {"status": "PASS" if not (client_missing or server_missing or review_leaks or server_leaks or editor_script_leaks) else "FAIL",
            "clientCsv": str(Path(client_csv).resolve()), "serverCsv": str(Path(server_csv).resolve()),
            "clientAssetCount": len(client), "serverAssetCount": len(server),
            "runtimePackagesVerified": len(runtime), "editorAuthorModulesStripped": len(editor_scripts),
            "editorAuthorModuleLeaks": editor_script_leaks, "authorityPackagesRequired": len(authority),
            "heroUIProfilesRequired": len(ui), "clientMissing": client_missing,
            "serverMissing": server_missing, "clientReviewLeaks": review_leaks,
            "serverVFXLeaks": server_leaks, "networkVerified": False, "manualVisualAccepted": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client-csv", required=True)
    parser.add_argument("--server-csv", required=True)
    parser.add_argument("--cold-report", required=True)
    parser.add_argument("--report", required=True)
    args = parser.parse_args()
    report = inspect(args.client_csv, args.server_csv, args.cold_report)
    Path(args.report).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    raise SystemExit(0 if report["status"] == "PASS" else 1)
