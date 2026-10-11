#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""在已经运行的独占鸟群PIE宿主中验证真实组件身份和拒绝替换。
调用方先用锁定UE5.8构建并建立PIE；本脚本不启动/停止编辑器、不创建资产，
不把请求配置数量当成GPU粒子数。缺少真实游戏世界或出现多个LocalPlayer明确失败。
运行前须显式传入-BirdOwnedIntegrationReview；只创建并销毁本脚本的瞬态Actor。
"""
import json
import uuid
from pathlib import Path
import unreal


def accepted(result):
    """UE Python bool+OutError成功返回字符串（可为空），失败返回None。"""
    return result if isinstance(result, bool) else result is not None


def set_runtime_properties(obj, values):
    """使用运行态赋值+显式Apply，避免编辑器Pre/PostEdit引发组件重注册/重建。
    此测试不验证编辑器属性修改事务。
    """
    for name, value in values.items():
        obj.set_editor_property(name, value, unreal.PropertyAccessChangeNotifyMode.NEVER)


def run():
    if "-BirdOwnedIntegrationReview" not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError("仅允许独占的鸟群PIE验证进程")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    world = editor.get_game_world()
    if world is None:
        raise RuntimeError("须先建立真实PIE世界，不能以编辑器世界冒充运行验证")
    services = [obj for obj in unreal.ObjectIterator(unreal.DivineBeastsPresentationClientSubsystem)
                if isinstance(obj.get_outer(), unreal.LocalPlayer)]
    if len(services) != 1:
        raise RuntimeError("需要唯一真实LocalPlayer服务，不猜测多玩家所有权")
    service = services[0]
    prefix = "DBA.Review.Ownership." + uuid.uuid4().hex
    report = {"status": "running", "scope": "real_component_request_lifecycle",
              "gpuValidation": "not_performed", "cases": []}
    actor = None
    out = Path(__file__).resolve().parents[3] / "Saved/Validation/BirdFlockAudit/OwnershipReview.json"

    def require(name, condition):
        report["cases"].append({"name": name, "pass": bool(condition)})
        if not condition:
            raise AssertionError(name)

    try:
        gs = unreal.get_default_object(unreal.GameplayStatics)
        transform = unreal.Transform(location=unreal.Vector(0, 0, 450))
        actor = gs.call_method('BeginDeferredActorSpawnFromClass', args=(
            world, unreal.Actor, transform,
            unreal.SpawnActorCollisionHandlingMethod.ALWAYS_SPAWN, None,
            unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        if actor is None:
            raise RuntimeError("无法创建本次瞬态Actor夹具")
        # 先完成Actor construction，防止稍后FinishSpawningActor把动态组件销毁为TRASH。
        gs.call_method('FinishSpawningActor', args=(
            actor, transform, unreal.SpawnActorScaleMethod.MULTIPLY_WITH_ROOT))
        first = actor.call_method('AddComponentByClass', args=(
            unreal.DivineBeastsBirdFlockSceneComponent, False, unreal.Transform(), True))
        second = actor.call_method('AddComponentByClass', args=(
            unreal.DivineBeastsBirdFlockSceneComponent, False, unreal.Transform(), True))
        set_runtime_properties(first, {"flock_id": "None", "species_id": "Seagull", "bird_count": 3})
        set_runtime_properties(second, {"flock_id": "None", "species_id": "Eagle", "bird_count": 5})
        # 两个组件均已设置配置，随后注册并执行真实BeginPlay。
        actor.call_method('FinishAddComponent', args=(first, False, unreal.Transform()))
        actor.call_method('FinishAddComponent', args=(second, False, unreal.Transform()))
        first_id, second_id = first.get_path_name(), second.get_path_name()
        require("同Actor双组件默认身份独立", first_id != second_id
                and service.get_requested_bird_count(first_id) == 3
                and service.get_requested_bird_count(second_id) == 5)

        set_runtime_properties(first, {"flock_id": prefix + ".Rejected", "bird_count": 513})
        require("换ID与非法数量的候选被拒绝", not accepted(first.apply_bird_flock()))
        require("拒绝候选保留合法旧绑定", service.get_requested_bird_count(first_id) == 3
                and service.get_requested_bird_count(prefix + ".Rejected") == 0)

        shared = prefix + ".Shared"
        set_runtime_properties(first, {"flock_id": shared, "bird_count": 3})
        require("有效候选可替换旧绑定", accepted(first.apply_bird_flock())
                and service.get_requested_bird_count(first_id) == 0
                and service.get_requested_bird_count(shared) == 3)
        set_runtime_properties(second, {"flock_id": shared, "bird_count": 8})
        require("显式身份拒绝其他活跃Owner占用", not accepted(second.apply_bird_flock())
                and service.get_requested_bird_count(shared) == 3
                and service.get_requested_bird_count(second_id) == 5)
        second.stop_bird_flock()
        require("失败候选的Stop不影响其他Owner", service.get_requested_bird_count(shared) == 3
                and service.get_requested_bird_count(second_id) == 0)
        set_runtime_properties(first, {"bird_count": 0})
        require("零数量撤销自有绑定", accepted(first.apply_bird_flock())
                and service.get_requested_bird_count(shared) == 0)
        end_id = prefix + ".EndPlay"
        set_runtime_properties(second, {"flock_id": end_id, "bird_count": 4})
        require("卸载前自有请求已受理", accepted(second.apply_bird_flock())
                and service.get_requested_bird_count(end_id) == 4)
        actor.destroy_actor()
        actor = None
        require("Actor销毁经EndPlay撤销自有请求", service.get_requested_bird_count(end_id) == 0)
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
        print("BIRD_OWNERSHIP_REVIEW", json.dumps(report, ensure_ascii=False))
    return report


run()
