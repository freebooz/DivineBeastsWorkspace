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
