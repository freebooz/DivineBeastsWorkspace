#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM竞技场VFX资源规格/文件存在性离线审计。

运行目标：静态确认七类Effect Type候选预算及三类NDC变量协议。
边界：不调用Unreal Editor、不读取.uasset内容、不测GPU；脚本通过不等于真实Niagara资源已验证。
输入：同目录ArenaVFXAssetsSpec_V1.json；输出：标准JSON报告，可选择严格要求原生资产存在。
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


ASSET_CLASSES = {"NiagaraEffectType", "NiagaraDataChannelAsset"}
TIERS = ("Epic", "High", "Medium", "Low")
REQUIRED_CHANNEL_VARS = {
    "Position": "Position",
    "Normal": "Vector3",
    "Color": "LinearColor",
    "Scale": "Float",
    "StyleId": "Int",
    "Seed": "Int",
}


def audit(spec: dict, workspace: Path, require_native: bool = False) -> dict:
    """仅检查声明和原生文件是否存在，不将存在性冒充UE AssetRegistry/Shader成功。"""
    problems = []
    if spec.get("schema_version") != 1:
        problems.append("规格版本不是V1")
    types = spec.get("effect_types", [])
    channels = spec.get("data_channels", [])
    if len(types) != 7 or len(channels) != 3:
        problems.append("需明确七类Effect Type和三类NDC通道")
    target = spec.get("targets", {})
    declared = [item.get("asset_name", "") for item in types + channels]
    if len(set(declared)) != len(declared) or not all(declared):
        problems.append("Effect Type/NDC资产名称缺失或重复")

    sums = {quality: 0 for quality in TIERS}
    for effect in types:
        if effect.get("unreal_asset_class") != "NiagaraEffectType":
            problems.append("Effect Type必须声明NiagaraEffectType资产类型")
        for quality in TIERS:
            value = effect.get("candidate_max_effect_type_instances", {}).get(quality)
            if not isinstance(value, int) or value < 1:
                problems.append(f"{effect.get('asset_name')}.{quality}预算必须为正整数")
            else:
                sums[quality] += value
    for quality in TIERS:
        if sums[quality] != target.get(quality):
            problems.append(f"{quality}各类别实例数之和与候选目标不一致")

    for channel in channels:
        if channel.get("unreal_asset_class") != "NiagaraDataChannelAsset":
            problems.append("NDC资产类型声明错误")
        actual = {var.get("name"): var.get("type") for var in channel.get("variables", [])}
        if actual != REQUIRED_CHANNEL_VARS:
            problems.append(f"{channel.get('asset_name')} 缺少必需名称/类型或存在额外字段")

    plugin = workspace / spec.get("platform_plugin_path", ".")
    found = []
    missing = []
    for category, asset_root, entries in (
        ("effect_type", "EffectTypes", types),
        ("data_channel", "DataChannels", channels),
    ):
        for entry in entries:
            path = plugin / "Content" / "Arena" / asset_root / (entry.get("asset_name", "") + ".uasset")
            if path.is_file():
                found.append({"kind": category, "path": str(path.relative_to(workspace)).replace("\\", "/")})
            else:
                missing.append({"kind": category, "path": str(path.relative_to(workspace)).replace("\\", "/")})

    if require_native and missing:
        problems.append("要求的UE原生资产尚未创建或尚未放在规定的Content目录")
    return {
        "success": not problems,
        "validation_scope_zh": "纯离线规格与.uasset路径检查；未进行UE反射、Shader、GPU和打包运行测试",
        "candidate_system_caps": sums,
        "native_files_found": found,
        "native_files_missing": missing,
        "unreal_native_assets_verified": False,
        "gpu_performance_verified": False,
        "errors": problems,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM竞技场VFX规格只读审计")
    parser.add_argument("--require-native", action="store_true", help="找不到预期UE原生文件即失败")
    parser.add_argument("--output", type=Path, help="可选JSON审计输出路径（只写报告，不碰资产）")
    args = parser.parse_args()
    workspace = Path(__file__).resolve().parents[4]
    spec = json.loads((Path(__file__).parent / "ArenaVFXAssetsSpec_V1.json").read_text(encoding="utf-8"))
    report = audit(spec, workspace, args.require_native)
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0 if report["success"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
