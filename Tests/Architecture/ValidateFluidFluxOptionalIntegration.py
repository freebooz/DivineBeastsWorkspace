#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fluid Flux 3.0 可选适配静态门禁（只读）。
验证三层依赖、角色工作室失败关闭和可选许可证交付位置。
普通模式缺插件允许通过：说明原生水面安全，不代表Fluid Flux已安装或运行。
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def _load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def _plugin_enabled(document: dict, name: str) -> bool:
    return any(item.get("Name") == name and item.get("Enabled") is True
               for item in document.get("Plugins", []))


def _check(condition: bool, message: str, failures: list[str]) -> None:
    if not condition:
        failures.append(message)


def locate_vendor_plugin(root: Path, specified: Path | None) -> Path | None:
    """只搜索明确、获授权的安装入口；不递归扫描机器或推测账户拥有资源。"""
    if specified is not None:
        return specified
    candidates = [
        root / "Game/Plugins/Marketplace/FluidFlux/FluidFlux.uplugin",
        root / "Game/Plugins/FluidFlux/FluidFlux.uplugin",
    ]
    engine_root = os.environ.get("UE_ROOT")
    if engine_root:
        candidates.append(Path(engine_root) / "Engine/Plugins/Marketplace/FluidFlux/FluidFlux.uplugin")
    return next((p for p in candidates if p.is_file()), None)


def verify(root: Path, vendor_descriptor: Path | None = None,
           require_vendor: bool = False) -> dict:
    failures: list[str] = []
    warnings: list[str] = []
    project = _load_json(root / "Game/DivineBeastsArena.uproject")
    _check(not _plugin_enabled(project, "FluidFlux"),
           "主工程不得默认启用第三方FluidFlux", failures)
    water_root = root / "Game/Plugins/GamePlatform/World/GamePlatformWater"
    water = _load_json(water_root / "GamePlatformWater.uplugin")
    modules = {m.get("Name"): m for m in water.get("Modules", [])}
    runtime = modules.get("GamePlatformWaterRuntime", {})
    client = modules.get("GamePlatformWaterClient", {})
    _check(runtime.get("Type") == "Runtime", "权威水域模块必须是双端Runtime", failures)
    _check(client.get("Type") == "ClientOnly" and
           "Server" not in client.get("TargetAllowList", []),
           "客户端水体模块必须从服务器排除", failures)
    runtime_rules = (water_root / "Source/GamePlatformWaterRuntime/GamePlatformWaterRuntime.Build.cs").read_text(encoding="utf-8")
    client_rules = (water_root / "Source/GamePlatformWaterClient/GamePlatformWaterClient.Build.cs").read_text(encoding="utf-8")
    for target, contents in (("WaterRuntime", runtime_rules), ("WaterClient", client_rules)):
        _check('"FluidFlux"' not in contents and '"Fluid Flux"' not in contents,
               f"{target}构建依赖不能硬包含Fluid Flux", failures)
    for module in ("Niagara", "Water", "FluidFlux"):
        _check(f'"{module}"' not in runtime_rules,
               f"服务器水体Runtime不能依赖{module}客户端渲染模块", failures)

    client_plugin = _load_json(root / "Game/Plugins/DivineBeasts/DBAClient/DBAClient.uplugin")
    client_modules = {m.get("Name"): m for m in client_plugin.get("Modules", [])}
    presentation = client_modules.get("DivineBeastsPresentationClient", {})
    _check(presentation.get("Type") == "ClientOnly" and
           "Server" not in presentation.get("TargetAllowList", []),
           "角色工作室水体策略必须在ClientOnly模块", failures)
    front_end = _load_json(
        root / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAFrontEndPack/DBAFrontEndPack.uplugin")
    _check(not _plugin_enabled(front_end, "FluidFlux") and
           not _plugin_enabled(client_plugin, "FluidFlux"),
           "角色前端包或项目客户端不能硬声明FluidFlux依赖", failures)
    logic = (root / "Game/Plugins/DivineBeasts/DBAClient/Source/"
             "DivineBeastsPresentationClient/Private/Water/"
             "DivineBeastsCharacterStudioWaterPolicy.cpp").read_text(encoding="utf-8")
    for term, description in [
        ("bFluidFlux3PreviewVerified", "缺少显式验收开关"),
        ("GConfig->GetBool", "缺少可验证项目配置读取"),
        ("IsRunningDedicatedServer()", "缺少专服保护"),
        ("IsRunningCommandlet()", "命令行烘焙不应激活GPU流体"),
        ("IPluginManager::Get().FindPlugin", "缺少供应商插件存在性检查"),
        ("NM_Standalone", "必须限制在本地非网络角色工作室"),
        ("DBAFrontEndPack", "必须限制在项目角色工作室内容包"),
        ("L_DBA_CharacterStudio", "必须限定真实角色选择地图"),
    ]:
        _check(term in logic, description, failures)

    found = locate_vendor_plugin(root, vendor_descriptor)
    vendor_status = "未发现正式安装证据"
    if found is not None and found.is_file():
        try:
            vendor = _load_json(found)
            _check(isinstance(vendor.get("FileVersion"), int),
                   "第三方插件描述文件结构异常", failures)
            vendor_status = "已找到插件描述文件；UE5.8编译及资产验收尚未验证"
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            failures.append(f"第三方插件描述文件无法解析：{exc}")
    else:
        warnings.append("未提供可读取的FluidFlux.uplugin；原生水体回退可验证，但无法证明已获得正版资产或UE5.8兼容。")
        if require_vendor:
            failures.append("严格安装门禁失败：缺少用户授权的FluidFlux.uplugin")
    return {
        "Passed": not failures,
        "VendorStatus": vendor_status,
        "PolicyDefault": "Native（默认原生水体）",
        "VerifiedByThisTool": "源码与静态配置；不证明GPU表现、UE Shader、客户端Cook或服务端剥离",
        "Failures": failures,
        "Warnings": warnings,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM Fluid Flux可选水体三层依赖与失败关闭检查")
    parser.add_argument("--vendor-uplugin", type=Path,
                        help="可选的正式安装FluidFlux.uplugin路径，不自动下载或修改")
    parser.add_argument("--require-vendor", action="store_true",
                        help="严格安装模式：找不到合法插件描述文件即失败，仍不代表性能验证")
    args = parser.parse_args()
    result = verify(ROOT, args.vendor_uplugin, args.require_vendor)
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if result["Passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
