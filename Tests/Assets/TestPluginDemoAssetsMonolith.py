"""SSLM开发UI Monolith导出只读门禁：工具层，不创建/编译/保存资产。

输入仅是实际工具回读的JSON证据；本离线门禁检查合同与结果形状，
不证明文件来源可信，不替代独立Editor重载、运行、颜色或人工核验。
无真实导出明确失败，禁止把未执行/缺资产当成通过。
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECIPES = ROOT / "Tools/Unreal/PluginDemos/PluginDemoUIMonolithRecipes.json"


def validate_readback(recipe: dict, evidence: dict) -> list[str]:
    """逐项检查正式工程、真实父类/树类型、保存重载/编译错误；不执行作者数据中的命令。"""
    errors = []
    if evidence.get("project") != recipe["project"] or not str(evidence.get("engine", "")).startswith("5.8."):
        errors.append("不是锁定正式工程/UE5.8。")
    if evidence.get("tool") != "Monolith MCP" or evidence.get("monolith_version") != "0.23.0":
        errors.append("缺实际锁定Monolith身份/版本。")
    if evidence.get("build_matches_source") is not True:
        errors.append("缺当前完整Editor源/二进制匹配门禁。")
    records = evidence.get("assets", [])
    if not isinstance(records, list) or any(not isinstance(r, dict) for r in records):
        return errors + ["资产回读不是完整对象数组。"]
    by_path = {r.get("path"): r for r in records}
    if len(by_path) != len(records):
        errors.append("资产回读身份重复。")
    for item in recipe["assets"]:
        path = f"{recipe['asset_root']}/{item['folder']}/{item['name']}"
        record = by_path.get(path)
        if not record:
            errors.append("缺真实资产回读：" + path)
            continue
        if record.get("parent") != item["parent"]:
            errors.append("真实父类不匹配：" + path)
        tree = record.get("controls", {})
        for name, kind in item["controls"].items():
            if not isinstance(tree, dict) or tree.get(name) != kind:
                errors.append(f"控件缺失/类型错误：{path}::{name}")
        for field in ("compiled", "saved", "independently_reloaded", "focus_validated", "accessibility_validated"):
            if record.get(field) is not True:
                errors.append(f"未实际完成{field}：{path}")
        if type(record.get("compile_errors")) is not int or record["compile_errors"] != 0:
            errors.append("编译错误/缺编译日志：" + path)
    if evidence.get("screen_definition") != recipe["screen_definition"]:
        errors.append("平台页面定义/焦点/生命周期合同未回读匹配。")
    if evidence.get("used_forbidden_widget_preview") is not False:
        errors.append("未排除已知错误Widget Gamma截图路径。")
    if evidence.get("viewport_checks") != ["1920x1080", "2560x1440", "3840x2160", "small_safe_area"]:
        errors.append("缺四种视口/安全区布局核查。")
    return errors


def main() -> int:
    """只读入口；0仅导出合同有效，1不完整，2缺实际证据。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    try:
        recipe = json.loads(RECIPES.read_text(encoding="utf-8-sig"))
        evidence = json.loads(args.evidence.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        print(json.dumps({"status": "not_executed", "reason": str(error)}, ensure_ascii=False))
        return 2
    if not isinstance(evidence, dict):
        print("证据必须为JSON对象。")
        return 1
    errors = validate_readback(recipe, evidence)
    print(json.dumps({"scope": "离线导出合同；不代表运行/人工验收", "errors": errors}, ensure_ascii=False, indent=2))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
