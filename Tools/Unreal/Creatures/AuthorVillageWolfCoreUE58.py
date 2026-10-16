#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UE5.8 新手村狼 AI 定义 + 服务器安全蓝图原生作者。

普通 Python 可用 inspect 读取制作合同；apply-core 必须运行于正式 UE5.8 编辑器 Python 环境。
只新建两份真实UE原生资产，不伪造 .uasset，不修改原新手村地图，不操作第三方 Fab 素材。
本轮不触发 C++ 编译、自动化测试、蓝图显式编译或 Cook。创建后的正式完整验证留待独立任务。

调用（示例，具备实际 Editor 与项目模块后）：
UnrealEditor-Cmd.exe DivineBeastsArena.uproject -DisablePlugins=Monolith \
  -EnablePlugins=PythonScriptPlugin,DBAGameplay,DBAWorlds,DBAWorldPack_Village \
  -SSLMVillageWolfAuthoring -ExecutePythonScript=<绝对路径>
显式 -SSLMVillageWolfAuthoring 标志启动时自动执行 apply-core；普通 Python 默认 inspect。
"""
from __future__ import annotations

import json
import os
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
CONTRACT = Path(__file__).with_name("VillageWolfAuthoringSpec.json")
SPEC = json.loads(CONTRACT.read_text(encoding="utf-8-sig"))
MODE = os.environ.get("SSLM_VILLAGE_WOLF_AUTHOR_MODE", "inspect").strip().lower()
OWNER_KEY = "SSLMVillageWolfCoreOwner"
OWNER_VALUE = "SSLM.Wolf.NativeAuthor.v1"


def require(condition, reason: str) -> None:
    if not condition:
        raise RuntimeError(reason)


def inspect() -> None:
    """纯信息输出：不存在真实原生结果时不得声称已生成。"""
    core = SPEC["Core"]
    result = {
        "模式": "inspect",
        "原生角色蓝图待创建": core["Blueprint"],
        "原生AI数据资产待创建": core["AIDefinition"],
        "外部狼美术": SPEC["Visual"]["ResourceSourceStatus"],
        "修改生产地图": False,
        "编译验证": False,
    }
    print("SSLM_WOLF_AUTHOR_INSPECT", json.dumps(result, ensure_ascii=False))


def python_unreal_preflight(unreal):
    """先验证真实项目/引擎/状态，避免在未就绪环境中误生成无效蓝图。"""
    require(unreal.SystemLibrary.get_engine_version().startswith("5.8."),
            "此作者只支持 UE5.8")
    actual_project = Path(unreal.Paths.get_project_file_path()).resolve()
    expected_project = WORKSPACE / "Game/DivineBeastsArena.uproject"
    require(actual_project == expected_project.resolve(),
            "拒绝写入其它UE项目：" + str(actual_project))
    commandline = unreal.SystemLibrary.get_command_line()
    require("-SSLMVillageWolfAuthoring" in commandline,
            "缺少作者进程专用命令行标志，拒绝创建")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    require(editor is not None and editor.get_game_world() is None,
            "在PIE或模拟运行时禁止创建狼蓝图和AI定义")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(),
            "编辑器存在未保存地图，不应改动当前会话")
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),
            "编辑器存在未保存内容包，避免覆盖其它开发者资产")
    require(not SPEC["Policy"]["InitialEnabled"] and
            not SPEC["Policy"]["InitialClientVisualVerified"],
            "当前合同必须保持服务器刷怪和客户端可见性默认关闭")
    cls_ai = getattr(unreal, "GamePlatformAIDefinition", None)
    cls_wolf = getattr(unreal, "DivineBeastsVillageWolf", None)
    cls_enum = getattr(unreal, "GamePlatformAIBrainType", None)
    require(cls_ai and cls_wolf and cls_enum,
            "需要真实加载平台AI Definition和SSLM原生狼角色C++类；本工具不代替C++编译")
    require(hasattr(cls_enum, "NONE"), "平台AI枚举缺失NONE原生轻量决策分支")
    return cls_ai, cls_wolf, cls_enum


def assert_target_unoccupied_or_owned(unreal, path, expected_type):
    """已存在非本作者资源一律拒绝覆盖或重命名。"""
    lib = unreal.EditorAssetLibrary
    if not lib.does_asset_exist(path):
        return None
    asset = lib.load_asset(path)
    require(isinstance(asset, expected_type), "已存在其它原生类型：" + path)
    require(lib.get_metadata_tag(asset, OWNER_KEY) == OWNER_VALUE,
            "已存在但未由本作者标记，拒绝改写：" + path)
    print("SSLM_WOLF_CORE_ALREADY_EXISTS", path)
    return asset


def make_struct(unreal, type_name, values):
    cls = getattr(unreal, type_name, None)
    require(cls is not None, "UE反射结构不存在：" + type_name)
    item = cls()
    for key, value in values.items():
        item.set_editor_property(key, value)
    return item


def set_contract_ai_values(unreal, ai, enum):
    """平台AI保留唯一状态/感知/攻击策略；这里只填项目差异，不实现第二套AI。"""
    values = SPEC["Core"]
    profile = values["AI"]
    ai.set_editor_property("ai_definition_id", unreal.Name(values["AIDefinitionId"]))
    ai.set_editor_property("brain_type", enum.NONE)
    ai.set_editor_property("perception_profile", make_struct(
        unreal, "GamePlatformAIPerceptionProfile", {
            "sight_radius": float(profile["Perception"]["SightRadius"]),
            "lose_sight_radius": float(profile["Perception"]["LoseSightRadius"]),
            "peripheral_vision_half_angle_degrees": float(profile["Perception"]["PeripheralVisionHalfAngleDegrees"]),
            "max_age": float(profile["Perception"]["MaxAge"]),
            "enable_hearing": bool(profile["Perception"]["bEnableHearing"]),
            "enable_damage_sense": bool(profile["Perception"]["bEnableDamageSense"])
        }))
    ai.set_editor_property("target_selection_profile", make_struct(
        unreal, "GamePlatformAITargetSelectionProfile", {
            "max_candidates": int(profile["Target"]["MaxCandidates"]),
            "memory_seconds": float(profile["Target"]["MemorySeconds"]),
            "prefer_visible_targets": bool(profile["Target"]["bPreferVisibleTargets"])
        }))
    ai.set_editor_property("home_policy", make_struct(
        unreal, "GamePlatformAIHomePolicy", {
            "patrol_radius": float(profile["Home"]["PatrolRadius"]),
            "leash_radius": float(profile["Home"]["LeashRadius"]),
            "return_home_acceptance_radius": float(profile["Home"]["ReturnHomeAcceptanceRadius"]),
            "patrol_acceptance_radius": float(profile["Home"]["PatrolAcceptanceRadius"])
        }))
    ai.set_editor_property("update_profile", make_struct(
        unreal, "GamePlatformAIUpdateProfile", {
            "decision_interval": float(profile["Update"]["DecisionInterval"]),
            "move_refresh_interval": float(profile["Update"]["MoveRefreshInterval"]),
            "move_refresh_distance": float(profile["Update"]["MoveRefreshDistance"]),
            "attack_retry_backoff": float(profile["Update"]["AttackRetryBackoff"])
        }))
    ai.set_editor_property("attack_range", float(profile["AttackRangeCm"]))
    ai.set_editor_property("preferred_range", float(profile["PreferredRangeCm"]))
    # PrimaryAbilityTag（首选GAS技能标签）留空，现阶段走原有服务器原生普通咬击。
    # 不为动物美术、任务或网络依赖增加额外字段。


def create_core_asset(unreal, path, cls, factory, before_save=None):
    folder, name = path.rsplit("/", 1)
    lib = unreal.EditorAssetLibrary
    existing = assert_target_unoccupied_or_owned(
        unreal, path, unreal.Blueprint if cls == unreal.Blueprint else cls)
    if existing is not None:
        return existing, False
    require(not lib.does_asset_exist(path), "目标已被其它作者占用：" + path)
    if not lib.does_directory_exist(folder):
        require(lib.make_directory(folder), "无法创建UE资产目录：" + folder)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, cls, factory)
    require(asset is not None, "虚幻编辑器资产工厂创建失败：" + path)
    if before_save is not None:
        before_save(asset)
    lib.set_metadata_tag(asset, OWNER_KEY, OWNER_VALUE)
    require(lib.save_loaded_asset(asset, only_if_is_dirty=False),
            "UE原生资产未保存成功：" + path)
    print("SSLM_WOLF_CORE_NATIVE_SAVED", path)
    return asset, True


def apply_core(unreal):
    cls_ai, cls_wolf, cls_enum = python_unreal_preflight(unreal)
    core = SPEC["Core"]
    blueprint_path = core["Blueprint"]
    ai_path = core["AIDefinition"]
    lib = unreal.EditorAssetLibrary
    # 两个目标同时检查，再修改任意包；已被人工创建或同名错类型时严格拒绝覆盖。
    for path, cls in ((ai_path, cls_ai), (blueprint_path, unreal.Blueprint)):
        assert_target_unoccupied_or_owned(unreal, path, cls)

    factory_ai = unreal.DataAssetFactory()
    factory_ai.set_editor_property("data_asset_class", cls_ai)
    ai, ai_created = create_core_asset(
        unreal, ai_path, cls_ai, factory_ai,
        before_save=lambda asset: set_contract_ai_values(unreal, asset, cls_enum))

    factory_bp = unreal.BlueprintFactory()
    factory_bp.set_editor_property("parent_class", cls_wolf)
    bp, bp_created = create_core_asset(
        unreal, blueprint_path, unreal.Blueprint, factory_bp)
    # 不调用compile_blueprint，也不启动PIE：用户当前要求仅完成源码和蓝图作者动作。
    # Actor蓝图仅继承原生胶囊、AI状态和GAS/Combat，不添加客户端Mesh/Niagara组件。
    expected = {"AIDefinition": ai_path, "WolfServerBlueprint": blueprint_path}
    created = {"AIDefinition": ai_created, "WolfServerBlueprint": bp_created}
    report_path = WORKSPACE / "Saved/Validation/VillageWolf/Authoring/CoreAuthorManifest.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps({
        "Project": "SSLM", "EngineVersion": str(unreal.SystemLibrary.get_engine_version()),
        "NativeAssets": expected, "CreatedInThisRun": created,
        "VisualsVerified": False, "ExternalFabImported": False,
        "CompiledOrCooked": False, "GameplayRuntimeAccepted": False
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("SSLM_WOLF_CORE_AUTHOR_COMPLETED", json.dumps(expected, ensure_ascii=False))

def main():
    require(SPEC["SchemaVersion"] == 1 and SPEC["Project"] == "SSLM",
            "狼资产作者合同版本不匹配")
    require(MODE in ("inspect", "apply-core"),
            "SSLM_VILLAGE_WOLF_AUTHOR_MODE仅支持inspect/apply-core")
    try:
        import unreal  # type: ignore
    except ImportError:
        require(MODE == "inspect", "仅UE5.8编辑器可执行apply-core，不得伪造.uasset")
        inspect()
        return

    # 作者进程具备明确的 -SSLMVillageWolfAuthoring 授权时可直接执行，
    # 不需要在命令行中设置用户级环境变量或通过外壳启动构建任务。
    if MODE == "apply-core" or "-SSLMVillageWolfAuthoring" in unreal.SystemLibrary.get_command_line():
        apply_core(unreal)
        return
    inspect()


if __name__ == "__main__":
    main()
