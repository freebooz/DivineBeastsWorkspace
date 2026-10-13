#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 通用材质函数库：先制作原生资源、最后统一编译的阶段执行入口。

所属：第三层开发审核工具，调用已有 GamePlatformSurface、GamePlatformVFX、
DBAPresentationPack_Core 三层归属对应的各类 UE5.8 原生资产作者。
当前仅负责制作：60项新增领域材质函数、12项MaterialLayer、1项共享混合层、
1项Actor展示蓝图与60份隔离审核Material。不运行任何显式Shader全量编译、
Blueprint最终编译、Cook或客户端/服务器构建；最终门禁由资源工单另行控制。
UI的7个函数和项目Widget Blueprint只能通过Monolith MCP制作为真实资产，
本文件不尝试绕开UI工具或将其当成已完成。
"""
from __future__ import annotations

import argparse
import contextlib
import os
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
TOOLS = ROOT / "Tools" / "Unreal" / "Surface"
DOMAIN = TOOLS / "AuthorMaterialLibraryDomainUE58.py"
LAYERS = TOOLS / "AuthorMaterialLibraryLayersUE58.py"
BLUEPRINT = TOOLS / "AuthorMaterialLibraryStandBlueprintUE58.py"

# 制作顺序来自正式资源任务队列：先函数，再PBR层，再Actor蓝图，最后审核Material。
# 仅调用已存在的源脚本；不得在制作阶段调用任意compile/finalize模式。
STEPS = (
    ("SurfaceFunctions", DOMAIN, ("--mode", "apply", "--owner", "Surface")),
    ("VFXFunctions", DOMAIN, ("--mode", "apply", "--owner", "VFX")),
    ("ProjectFunctions", DOMAIN, ("--mode", "apply", "--owner", "Project")),
    ("MaterialLayers", LAYERS, ("--mode", "apply")),
    ("ReviewStandBlueprint", BLUEPRINT, ("--mode", "apply")),
    ("SurfaceReviewMaterials", DOMAIN, ("--mode", "review", "--owner", "Surface")),
    ("VFXReviewMaterials", DOMAIN, ("--mode", "review", "--owner", "VFX")),
    ("ProjectReviewMaterials", DOMAIN, ("--mode", "review", "--owner", "Project")),
)


@contextlib.contextmanager
def argv_for(path: Path, arguments: tuple[str, ...]):
    """单一UE Python进程串行复用既有原生制作脚本，始终恢复调用方参数环境。"""
    prior = sys.argv[:]
    sys.argv[:] = [str(path), *arguments]
    try:
        yield
    finally:
        sys.argv[:] = prior


def require_unreal() -> None:
    """阻断普通Python假资产制作；蓝图与UMaterial只允许真实引擎写出。"""
    try:
        import unreal  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError("本阶段必须在实际UE5.8 Editor执行") from error
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("拒绝在其它工作区或工程制作原生材质：" + project)
    version = str(unreal.SystemLibrary.get_engine_version())
    if not version.startswith("5.8."):
        raise RuntimeError("必须使用工程锁定的虚幻引擎5.8版本，禁止跨版本保存.uasset：" + version)
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    if editor is None or editor.get_game_world() is not None:
        raise RuntimeError("材质批次制作前必须关闭PIE/Simulate，避免修改运行中的世界")
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    if registry.is_loading_assets():
        raise RuntimeError("UE资产注册表仍在加载，禁止在不完整索引下批量创建材质")


def inspect_only() -> None:
    """不创建资产；让业务审查者看到所有职责及实际创建顺序。"""
    print("MATERIAL_SOURCE_PHASE_TOTAL_STEPS", len(STEPS))
    for number, (name, source, args) in enumerate(STEPS, start=1):
        print("MATERIAL_SOURCE_PHASE_PLAN", number, name, source.relative_to(ROOT), " ".join(args))
        if not source.is_file():
            raise FileNotFoundError("被调UE作者源码不存在：" + str(source))
    print("MATERIAL_SOURCE_PHASE_NOTE UI需Monolith、地图需Monolith独立创建；本入口不会编译或Cook")


def apply_one(name: str, path: Path, args: tuple[str, ...]) -> None:
    """任一步骤异常即停止，明确保留已完成的原生资产，不执行隐式删除或回滚。"""
    if not path.is_file():
        raise FileNotFoundError("正式资源作者脚本不存在：" + str(path))
    print("MATERIAL_SOURCE_PHASE_BEGIN", name)
    with argv_for(path, args):
        runpy.run_path(str(path), run_name="__main__")
    print("MATERIAL_SOURCE_PHASE_DONE", name)


def main() -> None:
    """resume-from按原始阶段顺序重用各作者的已有资产只读确认能力。"""
    parser = argparse.ArgumentParser(description="通用材质库先制作后统一编译的UE5.8资源制作入口")
    parser.add_argument("--mode", choices=("inspect", "apply"), default="inspect")
    parser.add_argument("--resume-from", choices=[step[0] for step in STEPS], default=STEPS[0][0])
    args, _ = parser.parse_known_args()
    inspect_only()
    if args.mode == "inspect":
        return
    require_unreal()
    first = [x[0] for x in STEPS].index(args.resume_from)
    for name, path, sub_args in STEPS[first:]:
        apply_one(name, path, sub_args)
    print("MATERIAL_SOURCE_PHASE_APPLIED", len(STEPS) - first)
    print("FINAL_BUILD_REQUIRED 编译、Cook、GPU和人工验收依用户要求留至最终阶段")


if __name__ == "__main__":
    main()
