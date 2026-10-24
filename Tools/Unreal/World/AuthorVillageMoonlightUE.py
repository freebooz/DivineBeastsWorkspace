"""通过Monolith原位修改Village月光与固定曝光；遵循用户指定的夜景例外。"""
import hashlib
import json
import runpy
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
MIST = runpy.run_path(str(ROOT / "Tools/Unreal/World/AuthorVillageMistUE.py"))
OUT = ROOT / "Saved/Validation/VillageMist_20261018"
EXPOSURE_LABEL = "VillageMoonlightExposure"


def inspect_lighting():
    MIST["world"]()
    rows = []
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    for a in actors:
        if isinstance(a, unreal.DirectionalLight):
            c = a.get_component_by_class(unreal.DirectionalLightComponent)
            props = ("intensity", "light_color", "atmosphere_sun_light", "light_source_angle")
        elif isinstance(a, unreal.SkyLight):
            c = a.get_component_by_class(unreal.SkyLightComponent)
            props = ("intensity", "light_color", "real_time_capture")
        elif isinstance(a, unreal.PostProcessVolume) and a.get_actor_label() == EXPOSURE_LABEL:
            settings = a.get_editor_property("settings")
            rows.append({"label": a.get_actor_label(), "class": "PostProcessVolume",
                         "unbound": a.get_editor_property("unbound"),
                         "settings": {k: str(settings.get_editor_property(k)) for k in (
                             "auto_exposure_method", "auto_exposure_bias",
                             "auto_exposure_apply_physical_camera_exposure", "bloom_intensity")}})
            continue
        else:
            continue
        rows.append({"label": a.get_actor_label(), "class": a.get_class().get_name(),
                     "rotation": a.get_actor_rotation().export_text(),
                     "properties": {k: str(c.get_editor_property(k)) for k in props}})
    return rows


def apply_moonlight(preserve_fog=False, output_dir=None):
    """复用既有月光；仅切换照明时保留现有雾参数，证据可单独归档。"""
    out = Path(output_dir) if output_dir else OUT
    MIST["world"]()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "地图有未保存修改，拒绝覆盖"
    # 只保存当前地图；其他任务未保存的内容资产保持原样，不执行SaveAll。
    before_starts = MIST["player_starts"]()
    before = inspect_lighting()
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup = out / "Backups" / (stamp + "_BeforeMoonlight")
    backup.mkdir(parents=True)
    map_file = MIST["CONTENT"] / "Maps/L_Village_Start.umap"
    shutil.copy2(map_file, backup / "L_Village_Start.umap")
    (backup / "LightingBefore.json").write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding="utf-8")
    api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = api.get_all_level_actors()
    main = [a for a in actors if isinstance(a, unreal.DirectionalLight)]
    sky = [a for a in actors if isinstance(a, unreal.SkyLight)]
    assert len(main) == len(sky) == 1, "只调整现有唯一主光和天光"
    moon = main[0].get_component_by_class(unreal.DirectionalLightComponent)
    moon.set_editor_property("intensity", 1.0)
    moon.set_light_color(MIST["srgb_color"]((190, 214, 242)), True)
    moon.set_editor_property("light_source_angle", 1.2)
    main[0].set_actor_rotation(unreal.Rotator(pitch=-32, yaw=-35, roll=0), False)
    # 主光仍沿用SkyAtmosphere原生槽位；仅以低照度和冷色表达月光，不另建天空播放器。
    moon.set_editor_property("atmosphere_sun_light", True)
    moon.set_editor_property("atmosphere_sun_light_index", 0)
    sky_component = sky[0].get_component_by_class(unreal.SkyLightComponent)
    sky_component.set_editor_property("intensity", .65)
    sky_component.set_editor_property("real_time_capture", True)
    for a in actors:
        if preserve_fog:
            continue
        if isinstance(a, unreal.ExponentialHeightFog):
            a.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property(
                "fog_inscattering_luminance", MIST["srgb_color"]((72, 93, 115)))
        elif isinstance(a, unreal.LocalFogVolume) and unreal.Name(MIST["TAG"]) in a.tags:
            a.get_component_by_class(unreal.LocalFogVolumeComponent).set_editor_property(
                "fog_albedo", MIST["srgb_color"]((203, 216, 230)))
    pp_actors = [a for a in actors if a.get_actor_label() == EXPOSURE_LABEL]
    assert len(pp_actors) <= 1
    pp = pp_actors[0] if pp_actors else api.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    assert isinstance(pp, unreal.PostProcessVolume)
    pp.set_actor_label(EXPOSURE_LABEL)
    pp.set_folder_path("Village/Atmosphere")
    pp.tags = [unreal.Name("DBA.Village.MoonlightExposure")]
    pp.set_editor_property("unbound", True)
    pp.set_editor_property("priority", -50.)
    settings = pp.get_editor_property("settings")
    for key, value in {"override_auto_exposure_method": True,
                       "auto_exposure_method": unreal.AutoExposureMethod.AEM_MANUAL,
                       "override_auto_exposure_apply_physical_camera_exposure": True,
                       "auto_exposure_apply_physical_camera_exposure": False,
                       "override_auto_exposure_bias": True, "auto_exposure_bias": 2.5,
                       "override_bloom_intensity": True, "bloom_intensity": .1}.items():
        settings.set_editor_property(key, value)
    pp.set_editor_property("settings", settings)
    assert MIST["player_starts"]() == before_starts
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    digest = hashlib.sha256(map_file.read_bytes()).hexdigest()
    result = {"status": "SavedPendingColdAndVisualVerification", "map": MIST["MAP"],
              "request": "光照调整暗一些，月光照明", "lighting": inspect_lighting(),
              "mapSHA256": digest, "backup": str(backup), "playerStarts": before_starts,
              "fogPreserved": preserve_fog,
              "colorPolicyException": "用户明确指定Village月夜氛围，覆盖该地图默认晨光方向",
              "gpuMeasured": False, "manualVisualAccepted": False}
    (out / "MoonlightAuthoring.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if not preserve_fog:
        fog_report = MIST["inspect"]()
        fog_report.update(status=result["status"], mapSHA256=digest,
                          backup=json.loads((OUT / "Authoring.json").read_text(encoding="utf-8"))["backup"])
        (out / "Authoring.json").write_text(json.dumps(fog_report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("VILLAGE_MOONLIGHT_SAVED", digest)
    return result
