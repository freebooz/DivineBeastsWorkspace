#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在已经运行的独占蝴蝶/鱼群PIE宿主中验证真实组件身份和拒绝替换。
调用方先用锁定UE5.8构建并建立PIE；本脚本不启动/停止编辑器、不创建资产，
不把请求配置数量当成GPU粒子数。缺少真实游戏世界或出现多个LocalPlayer明确失败。
运行前须显式传入-FaunaOwnedIntegrationReview；只创建并销毁本脚本的瞬态Actor。
"""
import json
import uuid
from pathlib import Path
import unreal


def accepted(result):
    """兼容UE Python含OutError的返回元组；读取真实UFUNCTION布尔返回值。"""
    return bool(result[0]) if isinstance(result, tuple) else bool(result)


def run():
    if "-FaunaOwnedIntegrationReview" not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError("仅允许独占的蝴蝶/鱼群PIE验证进程")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world is None:
        raise RuntimeError("须先建立真实PIE世界，不能以编辑器世界冒充运行验证")
    services = [obj for obj in unreal.ObjectIterator(unreal.DivineBeastsPresentationClientSubsystem)
                if isinstance(obj.get_outer(), unreal.LocalPlayer)]
    if len(services) != 1:
        raise RuntimeError("需要唯一真实LocalPlayer服务，不猜测多玩家所有权")
    service = services[0]
    prefix = "DBA.Review.FaunaOwnership." + uuid.uuid4().hex
    report = {"status": "running", "scope": "real_component_request_lifecycle",
              "gpuValidation": "not_performed", "cases": []}
    actor = None
    out = Path(__file__).resolve().parents[3] / "Saved/Validation/BirdFlockAudit/FaunaOwnershipReview.json"

    def require(name, condition):
        report["cases"].append({"name": name, "pass": bool(condition)})
        if not condition:
            raise AssertionError(name)

    try:
        actor = unreal.GameplayStatics.begin_deferred_actor_spawn_from_class(
            world, unreal.Actor, unreal.Transform(location=unreal.Vector(0, 0, 450)),
            unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN)
        if actor is None:
            raise RuntimeError("无法创建本次瞬态Actor夹具")
        first = actor.add_component_by_class(
            unreal.DivineBeastsFaunaSceneComponent, False, unreal.Transform(), False)
        second = actor.add_component_by_class(
            unreal.DivineBeastsFaunaSceneComponent, False, unreal.Transform(), False)
        first.set_editor_properties({"habitat_id": "None", "species_id": "ButterflyAzure", "count": 3, "seed": 137})
        second.set_editor_properties({"habitat_id": "None", "species_id": "FishSilver", "count": 5, "seed": 137})
        unreal.GameplayStatics.finish_spawning_actor(
            actor, unreal.Transform(location=unreal.Vector(0, 0, 450)))
        first_id, second_id = first.get_path_name(), second.get_path_name()
        require("同Actor双组件默认身份独立", first_id != second_id
                and service.get_requested_fauna_count(first_id) == 3
                and service.get_requested_fauna_count(second_id) == 5)

        first.set_editor_properties({"habitat_id": prefix + ".Rejected", "count": 513})
        require("换ID与非法数量的候选被拒绝", not accepted(first.apply_fauna()))
        require("拒绝候选保留合法旧绑定", service.get_requested_fauna_count(first_id) == 3
                and service.get_requested_fauna_count(prefix + ".Rejected") == 0)

        first.set_editor_properties({"habitat_id": prefix + ".BadSeed", "count": 3, "seed": 1000001})
        require("超界种子拒绝且保留旧绑定", not accepted(first.apply_fauna())
                and service.get_requested_fauna_count(first_id) == 3
                and service.get_requested_fauna_count(prefix + ".BadSeed") == 0)
        first.set_editor_properties({"seed": -1})
        require("负种子拒绝且保留旧绑定", not accepted(first.apply_fauna())
                and service.get_requested_fauna_count(first_id) == 3)

        shared = prefix + ".Shared"
        first.set_editor_properties({"habitat_id": shared, "count": 3, "seed": 137})
        require("有效候选可替换旧绑定", accepted(first.apply_fauna())
                and service.get_requested_fauna_count(first_id) == 0
                and service.get_requested_fauna_count(shared) == 3)
        first.set_editor_properties({"count": 512, "seed": 1000000})
        require("数量和种子上界可受理", accepted(first.apply_fauna())
                and service.get_requested_fauna_count(shared) == 512)
        first.set_editor_properties({"count": 3, "seed": 0})
        require("种子零值可受理", accepted(first.apply_fauna())
                and service.get_requested_fauna_count(shared) == 3)
        second.set_editor_properties({"habitat_id": shared, "count": 8})
        require("显式身份拒绝其他活跃Owner占用", not accepted(second.apply_fauna())
                and service.get_requested_fauna_count(shared) == 3
                and service.get_requested_fauna_count(second_id) == 5)
        second.stop_fauna()
        require("失败候选的Stop不影响其他Owner", service.get_requested_fauna_count(shared) == 3
                and service.get_requested_fauna_count(second_id) == 0)
        first.set_editor_property("count", 0)
        require("零数量撤销自有绑定", accepted(first.apply_fauna())
                and service.get_requested_fauna_count(shared) == 0)
        end_id = prefix + ".EndPlay"
        second.set_editor_properties({"habitat_id": end_id, "count": 4})
        require("卸载前自有请求已受理", accepted(second.apply_fauna())
                and service.get_requested_fauna_count(end_id) == 4)
        actor.destroy_actor()
        actor = None
        require("Actor销毁经EndPlay撤销自有请求", service.get_requested_fauna_count(end_id) == 0)
        report["status"] = "passed"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = repr(error)
        raise
    finally:
        if actor is not None:
            actor.destroy_actor()
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print("FAUNA_OWNERSHIP_REVIEW", json.dumps(report, ensure_ascii=False))
    return report


run()
