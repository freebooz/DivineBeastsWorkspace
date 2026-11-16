#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM V3 黑帧根因真实 UE5.8 世界诊断：V3对照 Cliff、不修改或保存关卡。

只反射加载后Actor/Light/landscape/component材质、可见状态、World类型与实际视点。
对V3先前基于UE坐标的Pitch/Yaw与UE官方FindLookAtRotation作点乘比较，
可明确检查是否相机指向错误。只读取工程，不创建/替换Actor。
"""
from __future__ import annotations

import json
import math
import os
from pathlib import Path

MAP = os.getenv(
    "SSLM_TERRAIN_DIAGNOSTIC_MAP",
    "/Game/Development/Terrain/Maps/L_DBA_TerrainColorV3Review",
)
ROOT = Path(__file__).resolve().parents[3]
FILE = ROOT / "Game/Saved/Validation/TerrainV3BlackFrameActorCompare_20261018" / (
    MAP.rsplit("/", 1)[-1] + "_Revised.json"
)


def read_attr(obj, name):
    try:
        v = obj.get_editor_property(name)
        if hasattr(v, "get_path_name"):
            return str(v.get_path_name())
        return str(v)[:320]
    except Exception as exc:
        return "unavailable: " + str(exc)[:80]


def v3_camera_math(unreal, origin, extent):
    d = max(extent.x, extent.y)
    eye = unreal.Vector(
        origin.x + .34 * d,
        origin.y - .50 * d,
        origin.z + .70 * d + max(6000.0, extent.z),
    )
    target = unreal.Vector(origin.x, origin.y, origin.z)
    delta = unreal.Vector(target.x - eye.x, target.y - eye.y, target.z - eye.z)
    computed = unreal.Rotator(
        math.degrees(math.atan2(delta.z, math.hypot(delta.x, delta.y))),
        math.degrees(math.atan2(delta.y, delta.x)),
        0.0,
    )
    official = unreal.MathLibrary.find_look_at_rotation(eye, target)
    get_forward = unreal.MathLibrary.get_forward_vector
    forward = get_forward(computed)
    aim = get_forward(official)
    length = math.sqrt(delta.x ** 2 + delta.y ** 2 + delta.z ** 2)
    dot = (forward.x * delta.x + forward.y * delta.y + forward.z * delta.z) / length
    control = (aim.x * delta.x + aim.y * delta.y + aim.z * delta.z) / length
    return {
        "eye": [eye.x, eye.y, eye.z],
        "target": [target.x, target.y, target.z],
        "computedPitchYaw": [computed.pitch, computed.yaw],
        "officialPitchYaw": [official.pitch, official.yaw],
        "forwardDotTarget": dot,
        "officialForwardDotTarget": control,
    }


def main():
    import unreal  # type: ignore
    if not str(unreal.Paths.get_project_file_path()).replace("\\", "/").endswith(
        "/Game/DivineBeastsArena.uproject"
    ):
        raise RuntimeError("不是SSLM正式UE5.8项目")
    editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not editor or not editor.load_level(MAP):
        raise RuntimeError("关卡实际加载失败 " + MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    actors = list(unreal.EditorLevelLibrary.get_all_level_actors())
    settings = world.get_world_settings() if hasattr(world, "get_world_settings") else None
    rows = []
    terrain_bounds = None
    for actor in actors:
        kind = actor.get_class().get_name()
        location = actor.get_actor_location()
        rotation = actor.get_actor_rotation()
        info = {
            "label": actor.get_actor_label(),
            "class": kind,
            "position": [location.x, location.y, location.z],
            "rotation": [rotation.pitch, rotation.yaw, rotation.roll],
            "hidden": read_attr(actor, "hidden"),
            "editorOnly": read_attr(actor, "is_editor_only_actor"),
            "actorHiddenInGame": read_attr(actor,"hidden"),
        }
        for member in ("light_component", "directional_light_component", "sky_light_component"):
            try:
                comp = actor.get_editor_property(member)
                if comp:
                    info["lightComponent"] = {
                        "class": comp.get_class().get_name(),
                        "intensity": read_attr(comp, "intensity"),
                        "visible": read_attr(comp, "visible"),
                        "affectsWorld": read_attr(comp, "affects_world"),
                        "castShadows": read_attr(comp, "cast_shadows"),
                    }
                    break
            except Exception:
                continue
        if isinstance(actor, unreal.LandscapeProxy):
            origin, extent = actor.get_actor_bounds(only_colliding_components=False)
            terrain_bounds = (origin, extent)
            cls = getattr(unreal, "LandscapeComponent", None)
            if cls:
                comps = list(actor.get_components_by_class(cls))
                info["numLandscapeComponents"] = len(comps)
                info["componentSample"] = [
                    {
                        "name": c.get_name(),
                        "visible": read_attr(c, "visible"),
                        "hiddenInGame": read_attr(c, "hidden_in_game"),
                        "isRegistered": str(c.is_registered()) if hasattr(c, "is_registered") else "unsupported",
                        "material": c.get_material(0).get_path_name() if c.get_material(0) else None,
                    }
                    for c in comps[:3]
                ]
            info["landscapeMaterial"] = read_attr(actor, "landscape_material")
            info["componentBounds"] = {
                "center": [origin.x, origin.y, origin.z],
                "extent": [extent.x, extent.y, extent.z],
            }
        rows.append(info)
    report = {
        "map": MAP,
        "world": world.get_path_name(),
        "worldSettings": settings.get_path_name() if settings else None,
        "worldSettingProperties": {
            n: read_attr(settings, n)
            for n in (
                "world_gravity_z",
                "b_force_no_precomputed_lighting",
                "enable_world_bounds_checks",
                "pauser",
                "time_dilation",
            )
        } if settings else {},
        "actors": rows,
        "cameraMath": v3_camera_math(unreal, *terrain_bounds) if terrain_bounds else None,
        "readOnly": True,
    }
    FILE.parent.mkdir(parents=True, exist_ok=True)
    FILE.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(
        "SSLM_TERRAIN_V3_WORLD_ACTORS_DIAGNOSTIC",
        json.dumps(report, ensure_ascii=False)[:21000],
        flush=True,
    )


if __name__ == "__main__":
    main()
