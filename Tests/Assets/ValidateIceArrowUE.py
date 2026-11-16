# -*- coding: utf-8 -*-
"""SSLM寒冰箭UE5.8独立冷启动原生资产验收。

目标：核对平台唯一VFX执行器消费的12纹理、4网格、5母材质、10实例、6个Niagara系统、
1运动脚本、3个稳定定义及1个新残霜定义。只读加载，不创建/保存或修改资产。
这是资源存在、身份与软引用门禁；Shader/GPU画面、Client/Server Cook和联网另行取证。
"""
from datetime import datetime, timezone
import json
from pathlib import Path
import unreal
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Tools/Unreal/VFX/SpellProjectiles"))
from SpellPresentationContract import binding

ROOT = "/DBAPresentationPack_Core/VFX/IceArrow"
OLD = "/DBAPresentationPack_Core/VFX/DevelopmentMagic"


def verify() -> None:
    """只读取当前项目，生成不可覆盖的独立JSON；缺资产或身份冲突将失败关闭。"""
    errors: list[str] = []
    seen_ids: set[str] = set()
    counts = {"textures": 0, "meshes": 0, "master_materials": 0,
              "material_instances": 0, "niagara_systems": 0,
              "niagara_motion_modules": 0, "definition_ids": 0}
    project = unreal.Paths.get_project_file_path().replace("\\", "/")
    if not project.endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("非SSLM正式项目，拒绝执行冷读检查")
    if not unreal.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("当前引擎并非锁定的UE5.8")

    def check_asset(path: str, expected_class: type, group: str):
        """强类型读入资产，错误不提升为通过数，不进行自动修复。"""
        item = unreal.load_asset(path)
        if item is None or not isinstance(item, expected_class):
            errors.append("缺少或类型错误：" + path + "，预期 " + expected_class.__name__)
            return None
        counts[group] += 1
        return item

    for name in ("CrackMask", "CrystalShard", "FrostMist", "FrozenGround", "IceFlake",
                 "IceNormal", "IceORM", "ImpactFlash", "RuneInner", "RuneOuter", "TrailStrip"):
        tex = check_asset(ROOT + "/Textures/T_DBA_IceArrow_" + name,
                          unreal.Texture2D, "textures")
        if tex and tex.get_editor_property("srgb"):
            errors.append("数据/VFX遮罩错误开启sRGB：" + name)

    for name in ("CrystalLance", "CrystalShard", "IceCrown", "TrailRibbon"):
        check_asset(ROOT + "/Meshes/SM_DBA_IceArrow_" + name,
                    unreal.StaticMesh, "meshes")

    # 用户彩色原稿与技术遮罩使用不同色彩合同，不能按旧遮罩规则关闭原稿sRGB。
    reference = check_asset(ROOT + "/Textures/T_DBA_IceArrow_GroundRuneReference", unreal.Texture2D, "textures")
    if reference and not reference.get_editor_property("srgb"):
        errors.append("法阵彩色原图必须按sRGB解码")

    for name in ("Crystal", "Luminous", "Mist", "Trail", "RuneColor"):
        check_asset(ROOT + "/Materials/M_DBA_IceArrow_" + name,
                    unreal.Material, "master_materials")

    for name in ("Crystal", "CrystalSprite", "FrozenGround", "GroundInner", "GroundOuter",
                 "ImpactCrystal", "ImpactFlash", "Mist", "Snow", "Trail"):
        check_asset(ROOT + "/Materials/MI_DBA_IceArrow_" + name,
                    unreal.MaterialInstanceConstant, "material_instances")

    for name in ("Cast", "GroundRune", "Launch", "Flight", "Impact", "FrostResidue"):
        check_asset(binding("Frostbolt", name)["system"] if name in ("Cast", "Flight", "Impact") else ROOT + "/Niagara/NS_DBA_IceArrow_" + name,
                    unreal.NiagaraSystem, "niagara_systems")

    check_asset(ROOT + "/Niagara/NM_DBA_IceArrowMotion",
                unreal.NiagaraScript, "niagara_motion_modules")

    def verify_definition(stage: str, definition_path: str, niagara_path: str, expected_type: type):
        """只验证已发布逻辑ID、类及独占软Niagara目标，不改变数据服务租约。"""
        definition = check_asset(definition_path, expected_type, "definition_ids")
        if definition is None:
            return
        identity = definition.get_editor_property("logical_id")
        namespace = str(identity.get_editor_property("namespace"))
        name = str(identity.get_editor_property("name"))
        version = int(identity.get_editor_property("logical_version"))
        logical = namespace + "." + name + "@" + str(version)
        expected = "presentation.dba.development.magic.frostbolt." + stage.lower() + "@1"
        if logical != expected or logical in seen_ids:
            errors.append("定义身份错误或重复：" + logical + "，预期 " + expected)
        seen_ids.add(logical)
        system = definition.get_editor_property("niagara_system")
        path = system.get_path_name().split(".")[0] if system else ""
        if path != niagara_path:
            errors.append("定义未接入新Niagara：" + stage + ": " + path)

    for stage, dir_name in (("Cast", "Casting/Definitions"),
                            ("Flight", "Definitions"), ("Impact", "Definitions")):
        base = OLD + "/" + dir_name + "/DA_DBA_VFX_DevFrostbolt" + stage
        system = binding("Frostbolt", stage)["system"]
        expected_type = (unreal.GamePlatformVFXInstantDefinition if stage == "Impact"
                         else unreal.GamePlatformVFXAttachedDefinition)
        verify_definition(stage, base, system, expected_type)

    verify_definition("Residue", ROOT + "/Definitions/DA_DBA_VFX_DevFrostboltResidue",
                      ROOT + "/Niagara/NS_DBA_IceArrow_FrostResidue",
                      unreal.GamePlatformVFXInstantDefinition)

    config = Path(unreal.Paths.project_config_dir()) / "DefaultGame.ini"
    try:
        if 'Path="/DBAPresentationPack_Core/VFX/IceArrow/Definitions"' not in config.read_text("utf-8"):
            errors.append("AssetManager主资产扫描未登记寒冰箭残霜目录")
    except (OSError, UnicodeError) as exc:
        errors.append("无法读取AssetManager配置：" + str(exc))

    expected_counts = {"textures": 12, "meshes": 4, "master_materials": 5,
                       "material_instances": 10, "niagara_systems": 6,
                       "niagara_motion_modules": 1, "definition_ids": 4}
    for key, expected in expected_counts.items():
        if counts[key] != expected:
            errors.append(key + " 数量不足或错误：" + str(counts[key]) + "/" + str(expected))

    from ValidateSpellPresentationUE58 import validate as validate_current_presentation
    errors.extend(validate_current_presentation(raise_on_error=False)['Errors'])
    result = {"Status": "PASS" if not errors else "FAIL", "Counts": counts,
              "ExpectedCounts": expected_counts, "DefinitionIds": sorted(seen_ids),
              "Errors": errors, "ShaderCompileVerified": False, "GPUVisualVerified": False,
              "ClientCookVerified": False, "ServerCookVerified": False,
              "NetworkVerified": False, "HumanVisualAccepted": False}
    folder = Path(unreal.Paths.project_saved_dir()) / "Validation" / "IceArrow"
    folder.mkdir(parents=True, exist_ok=True)
    report = folder / ("ColdReadback_" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ") + ".json")
    report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", "utf-8")
    unreal.log("SSLM_ICE_ARROW_UE_READBACK=" + json.dumps(
        {"status": result["Status"], "counts": counts, "errors": errors,
         "report": str(report)}, ensure_ascii=False))
    if errors:
        raise RuntimeError("寒冰箭原生冷读未通过：" + "; ".join(errors[:10]))


if __name__ == "__main__":
    verify()
