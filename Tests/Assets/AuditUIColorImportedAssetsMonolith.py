# -*- coding: utf-8 -*-
"""第三层原图导入后的阻断审计，只能由Monolith的editor.run_python在真实UE编辑器运行。
只读纹理、原图和清单；仅写Saved证据，不编辑UI资产、图像、地图或人工客户端。
调用方可以传入IMPORT_MANIFEST，缺省检查当前原稿；错误会抛出异常，禁止把工具执行成功当颜色验收。
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import unreal

def main():
    """确认正式工程后审计全部项目原图；GPU/打包/人工验收在报告中明确保持未验证。"""
    project = Path(unreal.Paths.get_project_file_path()).resolve()
    if project.name != "DivineBeastsArena.uproject" or project.parent.name != "Game":
        raise RuntimeError("当前不是正式神兽联盟Game工程")
    root = project.parents[1]
    module_spec = importlib.util.spec_from_file_location("dba_ui_import_policy", root / "Tools/Unreal/UI/ImportDBAThemeTexturesMonolith.py")
    policy = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(policy)
    manifest = globals().get("IMPORT_MANIFEST")
    if manifest is None:
        manifest = json.loads(policy.DEFAULT_MANIFEST.read_text(encoding="utf-8-sig"))
    results = []
    for item in manifest["Files"]:
        dest = item["Destination"]
        source = (root / item["Source"]).resolve()
        errors = []
        settings = {}
        if not dest.startswith("/DBAUIPack_Core/UI/") or not dest.isascii():
            errors.append("资产目标越过项目公共UI边界")
        else:
            texture = unreal.load_asset(dest)
            if texture is None or texture.get_class().get_name() != "Texture2D":
                errors.append("真实Texture2D缺失")
            else:
                settings = {"srgb": texture.get_editor_property("srgb"),
                    "compression": str(texture.get_editor_property("compression_settings")),
                    "mips": str(texture.get_editor_property("mip_gen_settings")),
                    "group": str(texture.get_editor_property("lod_group")),
                    "adjustments": [texture.get_editor_property(k) for k in
                                    ("adjust_brightness", "adjust_brightness_curve", "adjust_vibrance",
                                     "adjust_saturation", "adjust_rgb_curve", "adjust_hue")],
                    "legacy_gamma": texture.get_editor_property("use_legacy_gamma")}
                errors.extend(policy.validate_import_settings(item.get("Role", "Color"), settings))
        if not source.is_relative_to(root) or not source.is_file():
            errors.append("原图源路径越界或缺失")
        elif hashlib.sha256(source.read_bytes()).hexdigest().lower() != item["SHA256"].lower():
            errors.append("原图字节哈希不一致")
        results.append({"Asset": dest, "Role": item.get("Role", "Color"), "Settings": settings,
                        "Errors": errors, "Passed": not errors})
    report = {"Project": str(project), "Count": len(results), "Passed": sum(r["Passed"] for r in results),
              "GPUVerified": False, "ClientCookVerified": False, "ManualAcceptance": False, "Results": results}
    target = root / "Saved/Validation/AnimationHUD/UIColorImportBlockingAudit.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    if report["Passed"] != report["Count"]:
        raise RuntimeError("UI原图导入审计失败：" + json.dumps([r for r in results if not r["Passed"]], ensure_ascii=False))
    print(json.dumps({"Count": report["Count"], "Passed": report["Passed"], "GPUVerified": False, "Report": str(target)}))

if __name__ == "__main__":
    main()
