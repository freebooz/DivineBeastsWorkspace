#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM寒冰箭 A3 网格对照版 Niagara 的 Monolith 原生制作任务。

职责：只在SSLM正式UE5.8编辑器中复用现有审核用 Flight_A 系统，得到独立 Flight_A3，
保留所有现有发射器、材质、曲线、Ribbon，只更换 CrystalArrow 的 Mesh Renderer网格。
使用 Monolith Niagara 动作读取/修改/编译/保存；不存在可写引擎时不得宣称任务完成。
正式 Definition、Catalog、旧A/A2资源和服务器权威弹道一律不修改。
运行本脚本必须由已确认编辑器所有权的Monolith会话显式执行；默认打印只读计划。
"""
import argparse
import importlib.util
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
BASE = "/DBAPresentationPack_Core/VFX/IceArrow"
FLIGHT_A = BASE + "/Niagara/Review/NS_DBA_IceArrow_Flight_A"
FLIGHT_A3 = BASE + "/Niagara/Review/NS_DBA_IceArrow_Flight_A3"
MESH_A3 = BASE + "/Meshes/Review/SM_DBA_IceArrow_CrystalA3Validated"
EMITTER = "CrystalArrow"


def protocol():
    """只复用项目既有Monolith协议，禁止偷偷转接其它端口或其它工程。"""
    bridge = ROOT / "Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py"
    spec = importlib.util.spec_from_file_location("sslm_ice_a3_protocol", bridge)
    adapter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(adapter)
    adapter.EVIDENCE = ROOT / "Saved/Validation/IceArrow/A3/MonolithCalls"
    identity = (
        "import unreal\n"
        "assert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()\n"
        "assert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith("
        "'/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\n"
        "assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')\n"
        "print('ICE_A3_EDITOR_OWNED')"
    )
    adapter.editor_python(identity)
    return adapter


def ensure_candidate_mesh(adapter):
    """通过Monolith调用有SHA保护的原生导入脚本，仅新增自己的Review网格。"""
    importer = ROOT / "Tools/Unreal/VFX/DevelopmentMagic/ImportIceArrowA3Validated.py"
    script = (
        "import importlib.util,unreal\n"
        f"mesh_path={MESH_A3!r}\n"
        "if not unreal.EditorAssetLibrary.does_asset_exist(mesh_path):\n"
        f" s=importlib.util.spec_from_file_location('ice_a3_native_import',{str(importer)!r})\n"
        " m=importlib.util.module_from_spec(s);s.loader.exec_module(m);m.import_candidate()\n"
        "assert isinstance(unreal.load_asset(mesh_path), unreal.StaticMesh)\n"
        "print('ICE_A3_MESH_CONFIRMED')"
    )
    adapter.editor_python(script)


def author_candidate():
    """复制审核系统，仅更换冰晶主体网格；诊断异常时拒绝保存及替换正式身份。"""
    adapter = protocol()
    # 先读取所有权及候选身份；防止重复执行在同名原生资源上产生隐蔽覆盖。
    adapter.editor_python(
        "import unreal\n"
        f"assert isinstance(unreal.load_asset({FLIGHT_A!r}),unreal.NiagaraSystem)\n"
        f"assert not unreal.EditorAssetLibrary.does_asset_exist({FLIGHT_A3!r})\n"
        "print('ICE_A3_NIAGARA_TARGET_UNUSED')"
    )
    ensure_candidate_mesh(adapter)
    adapter.editor_python(
        "import unreal\n"
        f"source={FLIGHT_A!r};target={FLIGHT_A3!r}\n"
        "assert not unreal.EditorAssetLibrary.does_asset_exist(target)\n"
        "assert unreal.EditorAssetLibrary.duplicate_asset(source,target), '独立Niagara审核系统复制失败'\n"
        "assert isinstance(unreal.load_asset(target),unreal.NiagaraSystem)\n"
        "print('ICE_A3_SYSTEM_DUPLICATED')"
    )
    renderers = adapter.niagara("list_renderers", asset_path=FLIGHT_A3, emitter=EMITTER)
    if not isinstance(renderers, dict):
        raise RuntimeError("Monolith未提供可检查的Renderer结构，拒绝猜测索引")
    options = [entry for entry in renderers.get("renderers", [])
               if "mesh" in str(entry.get("type", "")).lower()]
    if len(options) != 1:
        raise RuntimeError("CrystalArrow预期只有一个Mesh Renderer，实际为：" +
                           json.dumps(renderers, ensure_ascii=False))
    renderer_index = options[0].get("index")
    if not isinstance(renderer_index, int):
        raise RuntimeError("Monolith返回Renderer索引无效")
    adapter.niagara("set_renderer_mesh", asset_path=FLIGHT_A3,
                    emitter=EMITTER, renderer_index=renderer_index, mesh=MESH_A3)
    after = adapter.niagara("list_renderers", asset_path=FLIGHT_A3, emitter=EMITTER)
    if MESH_A3 not in json.dumps(after, ensure_ascii=False):
        raise RuntimeError("设置A3网格后未回读到真实Mesh引用，拒绝保存为成功")
    diagnostics = adapter.niagara("get_system_diagnostics", asset_path=FLIGHT_A3,
                                  compile_first=True)
    if diagnostics.get("error_count", 1):
        raise RuntimeError("A3 Niagara原生编译存在错误：" +
                           json.dumps(diagnostics, ensure_ascii=False))
    # 保留并公开任何未解决警告；旧Ribbon启发式误报也不得直接认定为新系统无警告。
    adapter.niagara("save_system", asset_path=FLIGHT_A3, only_if_dirty=False)
    adapter.editor_python(
        "import unreal\n"
        f"a=unreal.load_asset({FLIGHT_A3!r})\n"
        "assert isinstance(a,unreal.NiagaraSystem)\n"
        "assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)\n"
        "print('ICE_A3_SYSTEM_NATIVE_SAVED')"
    )
    record = {
        "State": "NativeCandidateSaved",
        "RecordedUtc": datetime.now(timezone.utc).isoformat(),
        "SourceSystem": FLIGHT_A,
        "ReviewSystem": FLIGHT_A3,
        "ChangedEmitter": EMITTER,
        "NewMesh": MESH_A3,
        "RendererIndex": renderer_index,
        "Diagnostics": diagnostics,
        "GameplayIntegrationVerified": False,
        "ClientCookVerified": False,
        "HumanVisualAccepted": False
    }
    folder = ROOT / "Saved/Validation/IceArrow/A3"
    folder.mkdir(parents=True, exist_ok=True)
    report = folder / ("FlightCandidate_" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".json")
    report.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(record, ensure_ascii=False, indent=2))
    return record


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true",
                        help="只在所属UE5.8编辑器、可用Monolith工具下建立真实审核候选")
    arguments = parser.parse_args()
    if arguments.apply:
        author_candidate()
    else:
        print(json.dumps({"NativeWrite": False, "Source": FLIGHT_A,
                          "Target": FLIGHT_A3, "Mesh": MESH_A3,
                          "Gate": "MonolithOwnedUE58"}, ensure_ascii=False, indent=2))
