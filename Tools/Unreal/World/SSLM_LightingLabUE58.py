# -*- coding: utf-8 -*-
"""SSLM Lighting Lab UE5.8隔离关卡作者。

本脚本只通过UE5.8 Editor与既有晨间审核作者创建开发副本。
正式地图只读；任何哈希、几何、未保存包或编辑器状态不符均立即停止。
所有参数是待实机校准候选，不是正式天气/光照DataAsset。
"""
import json
import os
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

import unreal as u
import ReviewVillageMorningEnvironmentUE as author

ROOT = Path(u.Paths.project_dir()).resolve().parent
CONFIG_PATH = Path(__file__).with_name("SSLM_LightingLab_V1.json")


def load_config():
    """读取唯一作者JSON；不在代码中复制维护第二套光照参数。"""
    config = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
    assert config["schema_version"] == 1, "Lighting Lab配置版本不受支持"
    assert config["engine_version"] == "5.8", "只允许锁定UE5.8"
    assert config["formal_map"] == author.FORMAL, "正式地图身份与既有作者不一致"
    assert config["review_root"].startswith("/Game/Development/"), "实验关卡必须位于开发目录"
    assert config["output_relative"].startswith("Saved/Validation/"), "证据必须写入工程Saved目录"
    assert len(config["profiles"]) == 6, "必须提供六套环境候选"
    assert len(config["presets"]) == 11, "必须提供六套环境和五套阴影角候选"
    return config


def configure():
    """将实验路径注入既有作者模块，复用其正式地图哈希和几何保护。"""
    config = load_config()
    author.REVIEW = config["review_root"]
    author.OUT = ROOT / config["output_relative"]
    author.presets = lambda: config
    return config


def _find_component_values(environment, actor_class, component_class):
    for actor in environment:
        if actor["class"] != actor_class:
            continue
        for component in actor["components"]:
            if component["class"] == component_class:
                values = component["values"]
                assert "intensity" in values, actor_class + "缺少Intensity基线"
                return values
    raise AssertionError("正式地图缺少组件：" + actor_class + "/" + component_class)


def _find_actor(actors, actor_type):
    matches = [actor for actor in actors if isinstance(actor, actor_type)]
    assert len(matches) == 1, actor_type.__name__ + "数量必须为1，实际=" + str(len(matches))
    return matches[0]


def _srgb_color(rgb):
    assert len(rgb) == 3 and all(0 <= int(value) <= 255 for value in rgb), "颜色必须为sRGB三通道字节"
    return u.Color(r=int(rgb[0]), g=int(rgb[1]), b=int(rgb[2]), a=255)


def preflight():
    """在任何地图写入前建立当前正式地图哈希、环境与几何基线。"""
    config = configure()
    world = author.idle()
    assert world.get_path_name().split(".")[0] == author.FORMAL, "必须先打开正式新手村地图进行只读基线采集"
    assert not author.OUT.exists(), "证据目录已存在；拒绝覆盖旧实验，请人工归档后更换output_relative"
    for key in config["presets"]:
        package = author.package_file(config["review_root"] + "/Maps/L_" + key)
        assert not package.exists(), "目标实验地图已存在，拒绝覆盖：" + str(package)
    report = author.audit(config["evidence"]["author_baseline"])
    assert report["formal_sha256"], "正式地图SHA-256采集失败"
    author.write("LightingLab_Preflight.json", {
        "status": "preflight_passed",
        "formal_map": author.FORMAL,
        "formal_sha256": report["formal_sha256"],
        "engine": report["engine"],
        "pid": os.getpid(),
        "review_root": config["review_root"],
        "profile_count": len(config["profiles"]),
        "preset_count": len(config["presets"]),
        "formal_map_unchanged": True
    })
    return report


def _apply_profile_to_current_map(key, config):
    """只对当前受保护的实验副本应用一个基线相对光照候选。"""
    baseline = author.check_formal()
    world = author.idle()
    expected_path = config["review_root"] + "/Maps/L_" + key
    assert world.get_path_name().split(".")[0] == expected_path, "当前关卡不是指定的Lighting Lab副本"
    preset = config["presets"][key]
    profile = config["profiles"][preset["profile"]]
    source_angle = float(preset.get("source_angle_override_deg", profile["sun_source_angle_deg"]))

    baseline_sun = _find_component_values(baseline["environment"], "DirectionalLight", "DirectionalLightComponent")
    baseline_sky = _find_component_values(baseline["environment"], "SkyLight", "SkyLightComponent")
    base_sun_intensity = float(baseline_sun["intensity"])
    base_sky_intensity = float(baseline_sky["intensity"])
    assert base_sun_intensity > 0 and base_sky_intensity >= 0, "正式地图光照基线无效"

    actors = author.actors()
    sun = _find_actor(actors, u.DirectionalLight)
    sky = _find_actor(actors, u.SkyLight)
    fog_actor = _find_actor(actors, u.ExponentialHeightFog)
    sun_component = sun.get_component_by_class(u.DirectionalLightComponent)
    sky_component = sky.get_component_by_class(u.SkyLightComponent)
    fog_component = fog_actor.get_component_by_class(u.ExponentialHeightFogComponent)

    sun_component.set_intensity(base_sun_intensity * float(profile["sun_intensity_multiplier"]))
    sun_component.set_editor_property("light_source_angle", source_angle)
    sun_component.set_editor_property("light_color", u.Color(r=255, g=255, b=255, a=255))
    sun_component.set_editor_property("use_temperature", True)
    sun_component.set_editor_property("temperature", float(profile["temperature_kelvin"]))
    sun_component.set_editor_property("volumetric_scattering_intensity", float(profile["sun_volumetric_scattering"]))

    rotation = next(row for row in baseline["environment"] if row["class"] == "DirectionalLight")["transform"]["rotation"]
    original = u.Quat(x=rotation[0], y=rotation[1], z=rotation[2], w=rotation[3]).rotator()
    sun.set_actor_rotation(
        u.Rotator(pitch=-float(profile["sun_altitude_deg"]), yaw=original.yaw, roll=original.roll),
        False
    )
    actual_rotation = sun.get_actor_rotation()
    assert abs(actual_rotation.pitch + float(profile["sun_altitude_deg"])) < 0.01, "太阳高度角回读不一致"
    assert abs(actual_rotation.yaw - original.yaw) < 0.01, "太阳方位角意外变化"
    assert abs(actual_rotation.roll - original.roll) < 0.01, "太阳Roll意外变化"

    expected_sun_intensity = base_sun_intensity * float(profile["sun_intensity_multiplier"])
    expected_sky_intensity = base_sky_intensity * float(profile["sky_intensity_multiplier"])
    sky_component.set_intensity(expected_sky_intensity)
    assert abs(float(sun_component.get_editor_property("intensity")) - expected_sun_intensity) <= max(0.01, expected_sun_intensity * 0.001), "方向光强度写入回读失败"
    assert abs(float(sky_component.get_editor_property("intensity")) - expected_sky_intensity) <= max(0.01, expected_sky_intensity * 0.001), "天空光强度写入回读失败"
    assert abs(float(sun_component.get_editor_property("light_source_angle")) - source_angle) < 0.001, "Source Angle写入回读失败"
    assert abs(float(sun_component.get_editor_property("temperature")) - float(profile["temperature_kelvin"])) < 0.1, "色温写入回读失败"

    fog_component.set_editor_property("fog_density", float(profile["fog_density"]))
    fog_component.set_editor_property("fog_height_falloff", float(profile["height_falloff"]))
    fog_component.set_editor_property("start_distance", float(profile["start_cm"]))
    fog_component.set_editor_property("fog_max_opacity", float(profile["max_opacity"]))
    fog_component.set_editor_property("fog_inscattering_luminance", author.srgb(profile["fog_srgb"]))
    fog_component.set_editor_property("enable_volumetric_fog", bool(profile["volumetric_enabled"]))
    fog_component.set_editor_property("volumetric_fog_albedo", _srgb_color(profile["volumetric_albedo_srgb"]))
    fog_component.set_editor_property("volumetric_fog_emissive", u.LinearColor(0, 0, 0, 0))
    fog_component.set_editor_property("volumetric_fog_distance", float(profile["volumetric_distance_cm"]))
    fog_component.set_editor_property("volumetric_fog_start_distance", 600.0)
    fog_component.set_editor_property("volumetric_fog_near_fade_in_distance", 400.0)
    fog_component.set_editor_property("volumetric_fog_scattering_distribution", float(profile["volumetric_scattering"]))
    fog_component.set_editor_property("volumetric_fog_extinction_scale", float(profile["volumetric_extinction_scale"]))
    assert abs(float(fog_component.get_editor_property("fog_density")) - float(profile["fog_density"])) < 0.0001, "高度雾密度写入回读失败"
    assert bool(fog_component.get_editor_property("enable_volumetric_fog")) == bool(profile["volumetric_enabled"]), "体积雾开关写入回读失败"

    layout = author.spatial_layout()
    second_fog = fog_component.get_editor_property("second_fog_data")
    second_fog.set_editor_property("fog_density", float(profile["second_density"]))
    second_fog.set_editor_property("fog_height_falloff", float(profile["second_height_falloff"]))
    second_fog.set_editor_property("fog_height_offset", float(layout["local_fog"][0]["location"][2]))
    fog_component.set_editor_property("second_fog_data", second_fog)

    local_actors = {actor.get_name(): actor for actor in actors}
    for placement in layout["local_fog"]:
        local_actor = local_actors.get(placement["actor"])
        assert local_actor, "缺少既有LocalFogVolume：" + placement["actor"]
        local_actor.set_actor_location(u.Vector(*placement["location"]), False, False)
        scale = float(placement["scale"])
        local_actor.set_actor_scale3d(u.Vector(scale, scale, scale))
        local_component = local_actor.get_component_by_class(u.LocalFogVolumeComponent)
        assert local_component, "LocalFogVolume组件缺失：" + placement["actor"]
        radial = float(profile["local_radial"]) if profile["local_enabled"] else 0.0
        height = float(profile["local_height"]) if profile["local_enabled"] else 0.0
        settings = {
            "radial_fog_extinction": radial,
            "height_fog_extinction": height,
            "height_fog_falloff": 2000.0,
            "height_fog_offset": 0.0,
            "fog_start_distance": float(profile["local_start_cm"]),
            "fog_phase_g": 0.2,
            "fog_albedo": author.srgb(profile["volumetric_albedo_srgb"]),
            "fog_emissive": u.LinearColor(0, 0, 0, 0)
        }
        for property_name, value in settings.items():
            local_component.set_editor_property(property_name, value)

    # 后处理曝光、白平衡、色彩分级、AO和Bloom继承正式地图基线；作者不主动改动。
    baseline_post = [
        row.get("settings_export") for row in baseline["environment"]
        if row["class"] == "PostProcessVolume"
    ]
    current_environment = author.environment()
    current_post = [
        row.get("settings_export") for row in current_environment
        if row["class"] == "PostProcessVolume"
    ]
    assert current_post == baseline_post, "作者意外改变后处理曝光/色彩设置，拒绝保存"
    assert author.geometry_equal(author.geometry(), baseline["geometry"]), "场景几何/植被实例/出生点变化，拒绝保存"

    author.save_review(key + "_Saved.json")
    author.write(key + "_LightingLab.json", {
        "status": "candidate_saved",
        "preset": key,
        "profile": preset["profile"],
        "source_angle_deg": source_angle,
        "formal_sha256": author.formal_hash(),
        "formal_map_unchanged": True,
        "geometry_equal": True,
        "post_process_unchanged": True,
        "environment": author.environment(),
        "engine": u.SystemLibrary.get_engine_version(),
        "pid": os.getpid()
    })


def create_lab():
    """创建11份隔离地图；目标已存在时拒绝覆盖，不自动清理部分结果。"""
    config = configure()
    if not (author.OUT / config["evidence"]["author_baseline"]).exists():
        preflight()
    else:
        author.idle()
        author.check_formal()
    created = []
    for key in config["presets"]:
        destination = config["review_root"] + "/Maps/L_" + key
        package = author.package_file(destination)
        assert not package.exists(), "目标地图已存在；拒绝覆盖：" + str(package)
        author.load_formal_and_copy("L_" + key)
        _apply_profile_to_current_map(key, config)
        created.append(destination)
    author.write("LightingLab_AuthoringSummary.json", {
        "status": "authoring_complete",
        "maps": created,
        "count": len(created),
        "formal_sha256": author.formal_hash(),
        "formal_map_unchanged": True,
        "geometry_equal": True,
        "pid": os.getpid()
    })
    return created


def cold_audit():
    """必须在退出并重新启动的独立UE Editor进程中执行。"""
    config = configure()
    author.idle()
    baseline = author.check_formal()
    assert os.getpid() != int(baseline["pid"]), "必须重启Editor后执行冷重载审核"
    results = []
    for key in config["presets"]:
        path = config["review_root"] + "/Maps/L_" + key
        assert u.get_editor_subsystem(u.LevelEditorSubsystem).load_level(path), "冷重载关卡失败：" + path
        saved_path = author.OUT / (key + "_Saved.json")
        saved = json.loads(saved_path.read_text(encoding="utf-8"))
        assert author.environment() == saved["environment"], "冷重载环境字段变化：" + key
        assert author.geometry_equal(author.geometry(), baseline["geometry"]), "冷重载几何变化：" + key
        assert author.formal_hash() == baseline["formal_sha256"], "正式地图哈希变化：" + key
        results.append({"map": path, "environment_equal": True, "geometry_equal": True})
    report = {
        "status": "cold_reload_passed",
        "pid": os.getpid(),
        "author_pid": baseline["pid"],
        "formal_sha256": baseline["formal_sha256"],
        "formal_map_unchanged": True,
        "maps": results
    }
    author.write(config["evidence"]["cold_reload_report"], report)
    return report


def inspect_current():
    """只读输出当前关卡环境和GI/阴影/雾效CVar。"""
    config = configure()
    world = author.idle()
    names = [
        "r.DynamicGlobalIlluminationMethod", "r.ReflectionMethod",
        "r.RayTracing", "r.Lumen.HardwareRayTracing",
        "r.Shadow.Virtual.Enable", "r.VolumetricFog",
        "r.VolumetricFog.GridPixelSize", "r.VolumetricFog.GridSizeZ"
    ]
    return {
        "world": world.get_path_name(),
        "engine": u.SystemLibrary.get_engine_version(),
        "environment": author.environment(),
        "cvars": {name: u.SystemLibrary.get_console_variable_int_value(name) for name in names},
        "formal_sha256": author.formal_hash(),
        "review_root": config["review_root"]
    }
