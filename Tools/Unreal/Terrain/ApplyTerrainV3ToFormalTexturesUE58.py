#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3.0正式十层地形基础色原位重导入（只允许UE5.8编辑器执行）。

用户于2026-10-18批准直接覆盖现有十层基础色；保留稳定Texture2D包名和原法线/ORM/高度。
只处理DBAWorldPack_Village已有十张Terrain/<Layer>/*BaseColor；不保存或重绑正式地图、
不改中性母材质、PCG、碰撞、导航、服务器逻辑。用户已确认的V3派生PNG是唯一输入。
严格要求Saved回退包包含10张原资产以及地图/母材质的SHA256快照。apply仅经真实UE
Texture2D重导入、保存；verify须通过另一个独立UE编辑器进程冷重载完成。
命令行使用TERRAIN_V3_FORMAL_MODE=inspect|apply|verify，默认为inspect（只读）。
同项目其它Editor正在保存同一路径时禁止执行apply；由调用者先取得独占资产操作窗口。
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODE = os.environ.get("TERRAIN_V3_FORMAL_MODE", "inspect").strip().lower()
MAPPING = ROOT / "Tools/Unreal/Terrain/TerrainV3ColorMapping.json"
V3_MANIFEST = ROOT / "Saved/ArtReview/TerrainColorV3/GenerationManifest.json"
BACKUP = ROOT / "Saved/ArtReview/TerrainColorV3FormalCover/Before_20261018/BackupManifest.json"
REPORT = ROOT / "Saved/ArtReview/TerrainColorV3FormalCover/AppliedManifest.json"
CONTENT = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                  "DBAWorldPack_Village/Content")
BRIDGE = "Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset"
MAP = "Maps/L_Village_Start.umap"


def sha256(path: Path) -> str:
    with path.open("rb") as handle:
        h = hashlib.sha256()
        for b in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(b)
        return h.hexdigest()


def inspect_inputs():
    if MODE not in {"inspect", "apply", "verify"}:
        raise ValueError("仅支持inspect/apply/verify；不得无门禁原位重导入")
    cfg = json.loads(MAPPING.read_text(encoding="utf-8"))
    generation = json.loads(V3_MANIFEST.read_text(encoding="utf-8-sig"))
    backup = json.loads(BACKUP.read_text(encoding="utf-8-sig"))
    if len(cfg["layers"]) != 10 or len(generation["layers"]) != 10 or len(backup["files"]) != 12:
        raise RuntimeError("十层颜色/12份回退文件清单不完整")
    if generation["paletteSha256"] != sha256(ROOT / cfg["referencePalette"]):
        raise RuntimeError("当前V3全局色板与派生PNG时的指纹不一致")
    if generation["productionManifestSha256"] != sha256(ROOT / cfg["baselinePBRManifest"]):
        raise RuntimeError("原始CC0 PBR来源清单被改动")
    bmap = {r["relative"].replace("\\", "/"): r for r in backup["files"]}
    gmap = {r["layer"]: r for r in generation["layers"]}
    rows = []
    for item in cfg["layers"]:
        layer = item["layer"]
        rel = f"Terrain/Textures/{layer}/T_DBA_Terrain_{layer}_BaseColor.uasset"
        dest = (CONTENT / rel)
        ref = gmap.get(layer)
        if ref is None or rel not in bmap or not dest.is_file():
            raise RuntimeError(f"{layer}正式源或备份缺失")
        src = ROOT / ref["derivedSourcePath"]
        if not src.is_file() or sha256(src) != ref["derivedSha256"]:
            raise RuntimeError(f"{layer} V3派生源已损坏或未生成")
        if not (BACKUP.parent / rel).is_file():
            raise RuntimeError(f"{layer}正式原资产快照丢失")
        if sha256(BACKUP.parent / rel) != bmap[rel]["sha256"]:
            raise RuntimeError(f"{layer}正式原资产快照HASH改变")
        rows.append({
            "layer": layer,
            "source": str(src),
            "sourceSha256": ref["derivedSha256"],
            "asset": f"/DBAWorldPack_Village/Terrain/Textures/{layer}/T_DBA_Terrain_{layer}_BaseColor",
            "destination": str(dest),
            "backupSha256": bmap[rel]["sha256"],
        })
    if len({x["layer"] for x in rows}) != 10 or len({x["asset"] for x in rows}) != 10:
        raise RuntimeError("地形层身份重复")
    for rel in (MAP, BRIDGE):
        old = bmap.get(rel)
        if old is None or sha256(CONTENT / rel) != old["sha256"]:
            raise RuntimeError("用户未授权改动正式地图或共享母材质，指纹已变")
    print("SSLM_V3_FORMAL_INPUTS_VERIFIED 10 BaseColor, 40 PBR original source, 12 safe backups", flush=True)
    return rows


def unreal_context():
    import unreal  # type: ignore
    if not unreal.Paths.get_project_file_path().replace("\\", "/").endswith(
            "/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("UE编辑器未打开正式SSLM工程")
    return unreal


def check_texture(unreal, row, expect_v3: bool):
    lib = unreal.EditorAssetLibrary
    obj = lib.load_asset(row["asset"])
    if not isinstance(obj, unreal.Texture2D):
        raise RuntimeError("稳定资产身份未加载为Texture2D：" + row["asset"])
    if not bool(obj.get_editor_property("srgb")):
        raise RuntimeError("正式BaseColor必须sRGB：" + row["layer"])
    if obj.get_editor_property("compression_settings") != unreal.TextureCompressionSettings.TC_DEFAULT:
        raise RuntimeError("正式BaseColor不是TC_DEFAULT：" + row["layer"])
    asset_data = lib.find_asset_data(row["asset"])
    dim = str(asset_data.get_tag_value("Dimensions") or "") if asset_data.is_valid() else ""
    if dim != "2048x2048":
        raise RuntimeError(f"{row['layer']}尺寸错误：{dim}")
    imp = obj.get_editor_property("asset_import_data")
    found = str(imp.get_first_filename()).replace("\\", "/").casefold() if imp else ""
    desired = row["source"].replace("\\", "/").casefold()
    if expect_v3 and found != desired:
        raise RuntimeError(f"{row['layer']}实际原生纹理导入路径未切换V3：{found}")
    return found


def apply(unreal, rows):
    if REPORT.exists():
        raise RuntimeError("正式十层覆盖收据已经存在；仅允许独立UE进程verify，不能重复覆盖")
    # Window其它Editor可能只占用部分地形资产。显式白名单允许本轮只接触独占可写资源，
    # 其余保留原样并登记Pending，绝不擅自关闭别人的UE会话或复制二进制绕过原生保存。
    selection = os.environ.get("TERRAIN_V3_FORMAL_APPLY_LAYERS", "").strip()
    names = set(selection.split(",")) if selection else {x["layer"] for x in rows}
    known = {x["layer"] for x in rows}
    if not names or not names.issubset(known):
        raise ValueError("指定的正式地形重导入白名单非法")
    lib = unreal.EditorAssetLibrary
    tool = unreal.AssetToolsHelpers.get_asset_tools()
    failures = []
    for row in rows:
        if row["layer"] not in names:
            continue
        try:
            already = check_texture(unreal, row, expect_v3=False)
            if already == row["source"].replace("\\", "/").casefold():
                print("SSLM_V3_FORMAL_ALREADY_REIMPORTED", row["layer"], flush=True)
            else:
                if sha256(Path(row["destination"])) != row["backupSha256"]:
                    raise RuntimeError("正式纹理发生未知修改，拒绝覆盖")
                target_dir, short = row["asset"].rsplit("/", 1)
                task = unreal.AssetImportTask()
                task.set_editor_property("filename", row["source"])
                task.set_editor_property("destination_path", target_dir)
                task.set_editor_property("destination_name", short)
                task.set_editor_property("automated", True)
                task.set_editor_property("replace_existing", True)
                task.set_editor_property("save", False)
                tool.import_asset_tasks([task])
                if not task.get_editor_property("imported_object_paths"):
                    raise RuntimeError("原生重导入无目标对象路径")
                obj = lib.load_asset(row["asset"])
                if not isinstance(obj, unreal.Texture2D):
                    raise RuntimeError("原生重导入未更新稳定Texture2D")
                obj.set_editor_property("srgb", True)
                obj.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
                if not lib.save_loaded_asset(obj, only_if_is_dirty=False):
                    raise RuntimeError("正式Texture2D保存失败，可能存在文件占用")
            check_texture(unreal, row, expect_v3=True)
            print("SSLM_V3_FORMAL_TEXTURE_REIMPORTED", row["layer"],
                  sha256(Path(row["destination"]))[:12], flush=True)
        except Exception as exc:
            failures.append({"layer": row["layer"], "errorZh": str(exc)})
            print("SSLM_V3_FORMAL_TEXTURE_FAILED", row["layer"], str(exc), flush=True)

    # 重读全部十个磁盘资产；只有所有十层都独立具备V3来源才写入完整apply清单。
    completed, pending = [], []
    for row in rows:
        try:
            check_texture(unreal, row, expect_v3=True)
            completed.append({**row, "actualSha256": sha256(Path(row["destination"])),
                              "importVerified": True})
        except Exception as exc:
            pending.append({"layer": row["layer"], "reasonZh": str(exc)})
    data = {
        "schemaVersion": 1, "date": "2026-10-18",
        "scopeZh": "正式十层Texture2D稳定身份配色覆盖；共享Village地图/母材质原样保留",
        "rows": completed, "pending": pending, "failures": failures,
        "coldVerifyPassed": False, "humanReviewPassed": False
    }
    REPORT.parent.mkdir(parents=True, exist_ok=True)
    if pending or failures:
        progress = REPORT.with_name("ProgressManifest.json")
        progress.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print("SSLM_V3_FORMAL_PARTIAL_REIMPORT", len(completed), "of", len(rows),
              "pending=", len(pending), "failed=", len(failures), flush=True)
        return
    data.pop("pending")
    data.pop("failures")
    data["mode"] = "apply"
    REPORT.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("SSLM_V3_FORMAL_ASSETS_SAVED_UE count=10 independent_verify_required=true", flush=True)


def verify(unreal, rows):
    if not REPORT.is_file():
        raise RuntimeError("未发现真实UE apply产物，不能宣称正式覆盖")
    old = json.loads(REPORT.read_text(encoding="utf-8"))
    reportrows = {r["layer"]: r for r in old["rows"]}
    if len(reportrows) != 10:
        raise RuntimeError("十份正式资产落盘记录不完整")
    for row in rows:
        check_texture(unreal, row, expect_v3=True)
        record = reportrows.get(row["layer"])
        if record is None or record["actualSha256"] != sha256(Path(row["destination"])):
            raise RuntimeError("编辑器重载后的正式纹理指纹不等于apply结果：" + row["layer"])
    old["coldVerifyPassed"] = True
    old["humanReviewPassed"] = False
    REPORT.write_text(json.dumps(old, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("SSLM_V3_FORMAL_COLD_VERIFY_PASS all_ten_import_sources_and_hashes=true map_untouched=true", flush=True)


def main():
    rows = inspect_inputs()
    if MODE == "inspect":
        print("SSLM_V3_FORMAL_PLAN reimport=10 change_map=false keep_PBR_normal_orm_height=true")
        return
    ue = unreal_context()
    if MODE == "apply":
        apply(ue, rows)
    elif MODE == "verify":
        verify(ue, rows)


if __name__ == "__main__":
    main()
