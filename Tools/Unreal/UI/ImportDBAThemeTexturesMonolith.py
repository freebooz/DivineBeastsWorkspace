#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第三层UI原图导入请求生成器：只读源PNG、检查哈希并输出Monolith动作，不操作UE资产。
所有实际导入、设置、保存必须逐条由Monolith执行并回读。
Color表示sRGB设计图，AlphaMask表示线性遮罩；禁止凭文件名推断角色。
默认读取原稿清单；旧Theme清单可由--manifest指定，其缺省角色为Color。
输出归Saved；错误哈希、越界路径、非法角色抛出异常并返回非零码。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
DEFAULT_MANIFEST = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core/Docs/UIColorImportManifest.json"

def validate_import_settings(role: str, settings: dict) -> list[str]:
    """检查真实回读值，返回全部违反项；缺字段也失败，不能用缺省成功掩盖未读到的设置。"""
    errors = []
    if role not in ("Color", "AlphaMask"):
        errors.append("未知颜色角色")
    if settings.get("srgb") is not (role == "Color"):
        errors.append("sRGB与颜色角色不一致")
    # UE枚举回读可能是短名或带模块/数值的文本，按枚举标识规范化而不猜测资产名。
    def enum_name(value):
        return str(value).split(":")[0].split(".")[-1].strip("<> ").replace("_", "").lower()
    for field, expected in (("compression", "tceditoricon"), ("mips", "tmgsnomipmaps"), ("group", "texturegroupui")):
        if enum_name(settings.get(field)) != expected:
            errors.append(field + "设置不符合无损UI合同")
    if settings.get("adjustments") != [1.0, 1.0, 1.0, 0.0]:
        errors.append("存在额外颜色调整或未完整回读")
    return errors

def prepare(manifest: dict, root: Path) -> list[dict]:
    """完整预检后才返回动作；目标仅限第三层公共UI，稳定身份和源RGB不变。"""
    requests = []
    for item in manifest["Files"]:
        source = (root / item["Source"]).resolve()
        if not source.is_relative_to(root.resolve()):
            raise ValueError("源文件越过工作空间边界")
        if hashlib.sha256(source.read_bytes()).hexdigest().lower() != item["SHA256"].lower():
            raise ValueError("原稿哈希不一致: " + str(source))
        role = item.get("Role", "Color")
        if role not in ("Color", "AlphaMask"):
            raise ValueError("必须指定Color或AlphaMask")
        dest = item["Destination"]
        if not dest.startswith("/DBAUIPack_Core/UI/") or not dest.isascii():
            raise ValueError("目标必须为第三层公共UI的ASCII稳定身份")
        requests.append({"tool": "material.import_texture", "params": {
            "source_file": str(source), "dest_path": dest, "compression": "Default",
            "srgb": role == "Color", "lod_group": "UI", "replace_existing": True}})
        # 导入动作不支持EditorIcon参数，后置设置后须回读实际B8G8R8A8，不把默认DXT当无损。
        for prop, value in {"CompressionSettings": "TC_EditorIcon", "MipGenSettings": "TMGS_NoMipmaps",
                            "SRGB": role == "Color", "AdjustBrightness": 1.0, "AdjustSaturation": 1.0,
                            "AdjustRGBCurve": 1.0, "AdjustHue": 0.0}.items():
            requests.append({"tool": "blueprint.set_cdo_property", "params": {
                "asset_path": dest, "property_name": prop, "value": value, "strict": True}})
        requests.append({"tool": "material.get_texture_properties", "params": {"asset_path": dest}})
    # 保存不是验收；sRGB变更后须复核材质采样器类型，并重新加载执行GPU颜色回归。
    requests.append({"tool": "editor.save_packages", "params": {"packages": [item["Destination"] for item in manifest["Files"]]}})
    # 最后一项是实际引擎资产审计，失败会抛出错误；读取已存在资源仍须经过此门禁。
    requests.append({"tool": "editor.run_python", "params": {
        "command": "import runpy; runpy.run_path(" + repr(str(root / "Tests/Assets/AuditUIColorImportedAssetsMonolith.py"))
            + ", run_name='__main__', init_globals={'IMPORT_MANIFEST': " + repr(manifest) + "})",
        "mode": "execute_file", "unattended": True}})
    return requests

def main() -> None:
    """只生成可审核清单，无网络、图像编辑或UE二进制写入。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--output", type=Path, default=ROOT / "Saved/Monolith/UIColorImportRequests.json")
    args = parser.parse_args()
    spec = json.loads(args.manifest.read_text(encoding="utf-8-sig"))
    result = {"SchemaVersion": 1, "ActualAssetsModified": False, "Requests": prepare(spec, ROOT),
              "RequiredVerification": ["Monolith项目/版本身份", "保存重载后的纹理/材质/白色乘色",
                                       "DivineBeasts.UI.Color.SourceToSlate真实RHI回归", "最终客户端人工核验"]}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"request_count": len(result["Requests"]), "output": str(args.output)}))

if __name__ == "__main__":
    main()
