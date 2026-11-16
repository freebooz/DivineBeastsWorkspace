"""SSLM 插件字体原生审查/制作；只能从正式工程的直接 Monolith 执行。

不启动编译、不联网、不签人工结论。扫描 AssetRegistry 的真实 WidgetBlueprint，
不是依赖 WBP 文件名前缀；通过实际 Font/TextStyle 路径修改，保留其他样式字段。
先 audit() 留下原生失败清单，再 author() 分批保存，最终独立编辑器 verify()。
"""
from __future__ import annotations

import json
import os
import shutil
import time
import importlib.util
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Saved/Validation/PluginSceneFontAudit/20261030"
SIZE = 10


def context():
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.get_project_file_path())).resolve()
    if project != ROOT / "Game/DivineBeastsArena.uproject":
        raise RuntimeError("拒绝操作其他工程")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("需要锁定 UE5.8")
    manifest = json.loads((ROOT / "Docs/Implementation/PluginDemos/PluginDemoManifest.json").read_text(encoding="utf-8-sig"))
    mounts = {p["plugin_id"]: (ROOT / p["descriptor_path"]).parent / "Content" for p in manifest["plugins"]}
    # 主工程中仍有插件保留的旧覆盖层/人工工具界面；不能仅凭目录排除这些真实入口。
    mounts["Game"] = ROOT / "Game/Content"
    OUT.mkdir(parents=True, exist_ok=True)
    return mounts


def get_path(obj, path):
    value = obj
    for key in path:
        value = value.get_editor_property(key)
    return value


def replace_nested(value, path, replacement):
    """显式复制嵌套 Slate 结构并逐层写回，避免仅修改 Python 临时副本。"""
    if not path:
        return replacement
    result = value.copy()
    child = result.get_editor_property(path[0])
    result.set_editor_property(path[0], replace_nested(child, path[1:], replacement))
    return result


def font_paths(widget):
    for class_name, path in (
        ("TextBlock", ("font", "size")),
        ("EditableText", ("widget_style", "font", "size")),
        ("EditableTextBox", ("widget_style", "text_style", "font", "size")),
        ("MultiLineEditableText", ("widget_style", "font", "size")),
        ("MultiLineEditableTextBox", ("widget_style", "text_style", "font", "size")),
        ("ComboBoxString", ("font", "size")),
        ("ComboBoxKey", ("font", "size")),
        ("SpinBox", ("font", "size")),
        ("RichTextBlock", ("default_text_style", "font", "size")),
    ):
        klass = getattr(unreal, class_name, None)
        if klass is not None and isinstance(widget, klass):
            yield path


def assets():
    mounts = context()
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    records = registry.get_assets_by_class(unreal.TopLevelAssetPath("/Script/UMGEditor", "WidgetBlueprint"), True)
    result = []
    for record in records:
        package = str(record.package_name)
        if package.split("/")[1] in mounts:
            blueprint = record.get_asset()
            if not isinstance(blueprint, unreal.WidgetBlueprint):
                raise RuntimeError("注册类与实际资产不符: " + package)
            result.append(blueprint)
    return sorted(result, key=lambda bp: bp.get_path_name())


def inspect(blueprints):
    rows = []
    prefixes = {bp.get_path_name() + ":WidgetTree.": bp for bp in blueprints}
    nodes = list(unreal.ObjectIterator(unreal.Widget))
    for widget in nodes:
        path = widget.get_path_name()
        prefix = path.split(":WidgetTree.", 1)[0] + ":WidgetTree."
        if prefix not in prefixes:
            continue
        for fields in font_paths(widget):
            size = float(get_path(widget, fields))
            rows.append({"asset": prefixes[prefix].get_path_name().split(".")[0],
                         "widget": widget.get_name(), "class": widget.get_class().get_path_name(),
                         "fields": list(fields), "size": size, "object": widget})
    return rows


def record(name, blueprints, rows, **extra):
    data = {"editor_pid": os.getpid(), "epoch": time.time(), "font_size": SIZE,
            "scope": "actual plugin WidgetBlueprint font fields; runtime/Slate/native styles checked separately",
            "assets": [bp.get_path_name() for bp in blueprints],
            "asset_count": len(blueprints), "font_fields": len(rows),
            "noncompliant": sum(row["size"] != SIZE for row in rows),
            "rows": [{k: v for k, v in row.items() if k != "object"} for row in rows],
            "human_review": "pending", **extra}
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return {k: data[k] for k in ("asset_count", "font_fields", "noncompliant", "editor_pid")}


def audit(name="Fonts.Before.json"):
    blueprints = assets()
    return record(name, blueprints, inspect(blueprints))


def author(start=0, count=20):
    mounts = context()
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None:
        raise RuntimeError("PIE 运行中，保留并拒绝修改共享 Widget")
    all_blueprints = assets()
    blueprints = all_blueprints[start:start + count]
    if not (OUT / "Fonts.Before.json").is_file():
        raise RuntimeError("先执行当前原生字体审查")
    targets = {bp.get_path_name().split(".")[0] for bp in blueprints}
    dirty = unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages() + unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
    conflict = [p.get_name() for p in dirty if p.get_name() in targets]
    if conflict:
        raise RuntimeError("目标包存在未保存编辑，拒绝覆盖: " + repr(conflict))
    rows = inspect(blueprints)
    by_asset = {}
    for row in rows:
        by_asset.setdefault(row["asset"], []).append(row)
    saved = []
    backup_root = OUT / "Backups/Fonts"
    for bp in blueprints:
        package = bp.get_path_name().split(".")[0]
        changed = [row for row in by_asset.get(package, []) if row["size"] != SIZE]
        if not changed:
            continue
        parts = package.strip("/").split("/")
        disk = mounts[parts[0]].joinpath(*parts[1:]).with_suffix(".uasset")
        backup = backup_root.joinpath(*parts).with_suffix(".uasset")
        if not backup.exists():
            backup.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(disk, backup)
        bp.modify()
        for row in changed:
            widget = row["object"]
            fields = row["fields"]
            widget.modify()
            original = widget.get_editor_property(fields[0])
            widget.set_editor_property(fields[0], replace_nested(original, fields[1:], float(SIZE)))
            if get_path(widget, fields) != SIZE:
                raise RuntimeError("字体原生写回失败: " + widget.get_path_name())
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        if bp.status != unreal.BlueprintStatus.BS_UP_TO_DATE:
            raise RuntimeError("蓝图编译失败，未保存: " + package)
        if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
            raise RuntimeError("原生保存失败: " + package)
        saved.append(package)
    after = inspect(blueprints)
    summary = record(f"Fonts.Batch.{start:04d}.{time.time_ns()}.json", blueprints, after,
                     saved=saved, start=start, count=count, total_assets=len(all_blueprints), cold_reload="pending")
    if summary["noncompliant"]:
        raise RuntimeError("本批编译后仍存在非 10 号字段")
    return {**summary, "saved": len(saved), "start": start, "total_assets": len(all_blueprints)}


def verify(require_cold=False):
    blueprints = assets()
    rows = inspect(blueprints)
    authored = [json.loads(p.read_text(encoding="utf-8")) for p in OUT.glob("Fonts.Batch.*.json")]
    styles_path = OUT / "Fonts.CommonStyles.json"
    if styles_path.is_file():
        style_record = json.loads(styles_path.read_text(encoding="utf-8"))
        authored.append({"editor_pid": style_record["pid"], "epoch": style_record["epoch"]})
    helper_path = ROOT / "Tools/Unreal/PluginDemos/AuthorIndependentPluginPagesMonolith.py"
    spec = importlib.util.spec_from_file_location("font_reader_process", helper_path)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    started = helper.current_process_started_epoch()
    cold = bool(authored) and all(item["editor_pid"] != os.getpid() and started > item["epoch"] for item in authored)
    summary = record("Fonts.Verified.json", blueprints, rows, independent_reload=cold,
                     reader_started_epoch=started, author_pids=sorted({item["editor_pid"] for item in authored}))
    if require_cold and not cold:
        raise RuntimeError("需要所有最终字体保存后独立启动的另一个 Editor")
    if not blueprints or not rows or summary["noncompliant"]:
        raise RuntimeError("当前插件字体有效字段未全为 10，详见 Fonts.Verified.json")
    return summary
