#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM寒冰箭A3已验证源网格导入UE5.8的唯一受控入口。

文件职责：项目第三层内容包Review候选的原生StaticMesh导入，非GamePlatform通用VFX运行代码。
调用边界：只接受Monolith在独占正式UE5.8编辑器运行本文件的import_candidate函数。
资源所有权：只新建Review目录下的SM_DBA_IceArrow_CrystalA3Validated；不覆盖正式模型、
Niagara、目录Definition、权威弹道和服务器资源。异常失败关闭，留给调用方定位。
脚本直接运行只输出源文件检查信息，禁止脱离引擎制作假uasset。
"""
import hashlib
import importlib.util
import json
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
SOURCE = ROOT / (
    "Game/Plugins/DivineBeasts/ContentPacks/Presentation/"
    "DBAPresentationPack_Core/SourceArt/VFX/IceArrow/Review/"
    "SM_DBA_IceArrow_CrystalA3Validated.obj"
)
ASSET = "/DBAPresentationPack_Core/VFX/IceArrow/Meshes/Review/SM_DBA_IceArrow_CrystalA3Validated"
EXPECTED_SHA256 = "50fc88d083ef9cddaa1715dd32da8ad1b98716e200fc783f95c44f9658b59061"


def verify_source():
    """先核对确定的原始字节，防止旧A3缺法线模型被错误导入。"""
    if not SOURCE.is_file():
        raise FileNotFoundError("寒冰箭A3已验证网格源文件缺失：" + str(SOURCE))
    digest = hashlib.sha256(SOURCE.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise RuntimeError("寒冰箭A3源文件摘要变化，必须重新完成几何检查后更新批准摘要")
    return {"source": str(SOURCE), "sha256": digest, "target": ASSET}


def import_candidate():
    """仅由Monolith在拥有者编辑器显式调用，保存真实原生资源，失败不重建已存在资产。"""
    verified = verify_source()
    import unreal

    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    command_line = unreal.SystemLibrary.get_command_line()
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("非SSLM正式工程，拒绝导入A3")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("不是锁定的UE5.8引擎")
    if "-IceArrowOwnedAuthoring" not in command_line:
        raise RuntimeError("没有独占寒冰箭作者会话标记，禁止写资产")
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET):
        raise RuntimeError("A3目标资源已经存在，不覆盖，先回读审核")

    # 复用既有Importer规范，保留OBJ法线和UV，不开启新碰撞或自动贴图导入。
    script = Path(__file__).with_name("BuildIceArrowCrystalA.py")
    spec = importlib.util.spec_from_file_location("sslm_ice_a3_importer", script)
    importer = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(importer)
    importer.SOURCE = SOURCE
    result = importer.import_candidate()
    mesh = unreal.load_asset(ASSET)
    if mesh is None or not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError("UE原生StaticMesh导入后加载失败，禁止写入发布引用")
    if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, False):
        raise RuntimeError("UE原生StaticMesh保存失败，禁止继续制作Niagara")

    record = {
        "Status": "NativeImportSaved",
        "TimestampUtc": datetime.now(timezone.utc).isoformat(),
        "Engine": unreal.SystemLibrary.get_engine_version(),
        "Asset": ASSET,
        "SourceSha256": verified["sha256"],
        "ImportResult": result,
        "NiagaraModified": False,
        "ProductionDefinitionModified": False,
        "HumanVisualAccepted": False,
        "ClientCookVerified": False,
        "ServerCookVerified": False,
    }
    evidence = ROOT / "Saved/Validation/IceArrow/A3"
    evidence.mkdir(parents=True, exist_ok=True)
    report = evidence / ("Import_" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".json")
    report.write_text(json.dumps(record, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    unreal.log("SSLM_ICE_A3_IMPORT_SAVED=" + json.dumps(record, ensure_ascii=False))
    return record


if __name__ == "__main__":
    # 脱离编辑器只读验证源稿，真正的写入由Monolith显式调用import_candidate。
    print(json.dumps(verify_source(), ensure_ascii=False, indent=2))
