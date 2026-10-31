# -*- coding: utf-8 -*-
"""标准火球原生CPU/GPU粒子回读：50cm球头、世界空间长尾和四档扰动。

仅供Monolith内显式运行。使用当前编辑器中不保存的Transient Actor，不修改地图、
Niagara或权威弹体。按4000cm/s移动视觉锚点以验证历史轨迹，不代替联机弹道测试。
"""
import json
import math
from pathlib import Path
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "Saved/Validation/FireballNiagara20261031"


def validate(qualities=None):
    """创建、采样并始终销毁自有Actor，恢复进程质量档；不设置帧率上限。"""
    OUT.mkdir(parents=True, exist_ok=True)
    subsystem = u.get_editor_subsystem(u.EditorActorSubsystem)
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    system = u.load_asset("/DBAPresentationPack_Core/VFX/SpellProjectiles/Niagara/NS_DBA_FireballFlight")
    original_quality = u.SystemLibrary.get_console_variable_int_value("fx.Niagara.QualityLevel")
    rows = []
    actor = None
    try:
        # 每次MCP调用只采样一个已生效档位；WorldManager的质量缓存需要真实引擎帧更新。
        # 四档由外部在调用之间切换，不能在同一Python主线程帧中伪造档位生效。
        for quality in (qualities if qualities is not None else (original_quality,)):
            u.SystemLibrary.execute_console_command(world, "fx.Niagara.QualityLevel " + str(quality))
            actor = subsystem.spawn_actor_from_class(u.NiagaraActor, u.Vector(-1200, 0, 150), transient=True)
            actor.set_actor_label("SSLM_FireballFlight_SimulationValidation")
            component = actor.get_editor_property("niagara_component")
            component.set_force_solo(True)
            component.set_asset(system)
            component.activate(True)
            # 视觉锚点每个子步真实移动，世界空间Ribbon不得被适配模块重置到局部原点。
            for step in range(36):
                actor.set_actor_location(u.Vector(-1200 + 4000 * (step + 1) / 60, 0, 150), False, False)
                component.advance_simulation(1, 1 / 60)
            cache = u.NiagaraSimCacheFunctionLibrary.create_niagara_sim_cache(component)
            cache = u.NiagaraSimCacheFunctionLibrary.capture_niagara_sim_cache_immediate(
                cache, u.NiagaraSimCacheCreateParameters(), component, True, .01)
            if not cache or not cache.is_cache_valid():
                raise RuntimeError("原生SimCache无效")
            print("NATIVE_CACHE_EMITTERS", [str(name) for name in cache.get_emitter_names()])
            scales = cache.read_vector_attribute("Scale", "Core")
            if len(scales) != 1 or any(abs(getattr(scales[0], axis) - .5) > .001 for axis in ("x", "y", "z")):
                raise RuntimeError("真实GPU球体缩放不是0.5：" + str(scales))
            positions = cache.read_position_attribute("Position", "FireTrail", False)
            if len(positions) < 8:
                raise RuntimeError("世界空间长尾粒子不足：" + str(len(positions)))
            length = max(p.x for p in positions) - min(p.x for p in positions)
            if not 400 <= length <= 1200:
                raise RuntimeError("长尾实际空间长度不符合参考：" + str(length))
            ripple_colors = cache.read_color_attribute("Color", "AirRipple")
            alphas = [color.a for color in ripple_colors]
            trace = {"quality": quality, "core_scales": [str(v) for v in scales],
                     "trail_particles": len(positions), "trail_extent_cm": length,
                     "air_ripple_alpha": alphas,
                     "air_ripple_scale": [str(v) for v in cache.read_vector_attribute("Scale", "AirRipple")]}
            (OUT / ("FlightSimulation.quality" + str(quality) + ".trace.json")).write_text(
                json.dumps(trace, ensure_ascii=False, indent=2), encoding="utf-8")
            if quality == 0 and any(alpha > .0001 for alpha in alphas):
                raise RuntimeError("Low仍有空气扰动可见Alpha")
            if quality > 0 and (not alphas or max(alphas) <= 0):
                raise RuntimeError("非Low热扰动未激活")
            expected_alpha = 0 if quality <= 0 else (.5 if quality == 1 else (1 if quality == 2 else 1.5))
            if len(alphas) != 1 or abs(alphas[0] - expected_alpha) > .001:
                raise RuntimeError("四档扰动强度不符：" + str(alphas))
            rows.append({"quality": quality, "core_particles": len(scales), "core_scale": [.5, .5, .5],
                         "sphere_source_diameter_cm": 100, "core_diameter_cm": 50,
                         "trail_particles": len(positions), "trail_extent_cm": length,
                         "air_ripple_alpha": alphas, "movement_speed_cm_per_second": 4000})
            component.deactivate()
            subsystem.destroy_actor(actor)
            actor = None
    finally:
        if actor:
            subsystem.destroy_actor(actor)
        u.SystemLibrary.execute_console_command(world, "fx.Niagara.QualityLevel " + str(original_quality))
    report = {"Passed": True, "Rows": rows, "Sample": "native NiagaraSimCache CPU/GPU",
              "AuthorityMovementTested": False, "GPUBudgetMeasured": False}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / ("FlightSimulation.quality" + str(rows[0]["quality"]) + ".json")).write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return report


if __name__ == "__main__":
    validate()
