"""仅通过Monolith在真实Village地图配置原生薄雾；不修改地形、出生点或天气权威。"""
import hashlib
import json
import shutil
from datetime import datetime, timezone
from pathlib import Path
import unreal

MAP = "/DBAWorldPack_Village/Maps/L_Village_Start"
TAG = "DBA.Village.MorningMist"
ROOT = Path(unreal.Paths.project_dir()).resolve().parent
OUT = ROOT / "Saved/Validation/VillageMist_20261018"
CONTENT = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/Content"
LOCAL_VOLUMES = (
    ("VillageMist_NorthEast", (8500, 6000, 70), 10.0),
    ("VillageMist_EastMeadow", (5000, -9500, 55), 8.0),
    ("VillageMist_NorthWest", (-10500, 8500, 70), 10.0),
)


def srgb_color(rgb):
    def linear(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return unreal.LinearColor(*(linear(c) for c in rgb), 1)


def world():
    assert unreal.Paths.get_project_file_path().replace("\\", "/").endswith("/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject")
    assert unreal.SystemLibrary.get_engine_version().startswith("5.8.")
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    w = editor.get_editor_world()
    assert w.get_path_name().split(".")[0] == MAP, "必须先打开正式新手村"
    assert not editor.get_game_world(), "PIE中不能改写地图"
    return w


def player_starts():
    return {a.get_name(): a.get_actor_transform().export_text()
            for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
            if isinstance(a, unreal.PlayerStart)}


def inspect():
    w = world()
    rows = []
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        if isinstance(actor, unreal.ExponentialHeightFog):
            c = actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
            props = ("fog_density", "fog_height_falloff", "start_distance", "fog_max_opacity",
                     "enable_volumetric_fog", "fog_inscattering_luminance", "second_fog_data")
        elif isinstance(actor, unreal.LocalFogVolume):
            c = actor.get_component_by_class(unreal.LocalFogVolumeComponent)
            props = ("radial_fog_extinction", "height_fog_extinction", "height_fog_falloff",
                     "height_fog_offset", "fog_start_distance", "fog_phase_g", "fog_albedo", "fog_emissive")
        else:
            continue
        rows.append({"name": actor.get_actor_label(), "class": actor.get_class().get_name(),
                     "transform": actor.get_actor_transform().export_text(),
                     "properties": {key: str(c.get_editor_property(key)) for key in props}})
    return {"map": w.get_path_name(), "fog": rows, "playerStarts": player_starts(),
            "fogLuminanceSRGB": "485D73", "localAlbedoSRGB": "CBD8E6",
            "linearConversion": "IEC 61966-2-1",
            "newNiagaraSystems": 0, "newMaterials": 0, "volumetricFogEnabled": False,
            "authorityChanged": False, "gpuMeasured": False, "manualVisualAccepted": False}


def apply():
    world()
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages(), "存在未保存地图修改，拒绝覆盖"
    assert not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), "存在未保存内容修改，先保留原工作"
    before = inspect()
    OUT.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    backup = OUT / "Backups" / stamp
    map_file = CONTENT / "Maps/L_Village_Start.umap"
    for path in (map_file, CONTENT / "__ExternalActors__/Maps/L_Village_Start",
                 CONTENT / "__ExternalObjects__/Maps/L_Village_Start"):
        if path.is_file():
            dst = backup / path.relative_to(CONTENT)
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, dst)
        elif path.is_dir():
            shutil.copytree(path, backup / path.relative_to(CONTENT))
    (backup / "Before.json").write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding="utf-8")
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    fogs = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.ExponentialHeightFog)]
    assert len(fogs) == 1, "只复用现有唯一高度雾，不叠加竞争全局雾"
    fog = fogs[0]
    component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    # 第一层负责远景空气透视；第二层在地表快速衰减，近身12米保持可读。
    settings = {"fog_density": .008, "fog_height_falloff": .18, "start_distance": 1200.,
                "fog_max_opacity": .32, "enable_volumetric_fog": False,
                "fog_inscattering_luminance": srgb_color((72, 93, 115))}
    for prop, value in settings.items():
        component.set_editor_property(prop, value)
    second = component.get_editor_property("second_fog_data")
    for prop, value in {"fog_density": .006, "fog_height_falloff": 1.25, "fog_height_offset": 50.}.items():
        second.set_editor_property(prop, value)
    component.set_editor_property("second_fog_data", second)
    fog.set_folder_path("Village/Atmosphere")
    fog.tags = list(dict.fromkeys(list(fog.tags) + [unreal.Name(TAG)]))
    existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    for label, position, scale in LOCAL_VOLUMES:
        a = existing.get(label)
        if a:
            assert isinstance(a, unreal.LocalFogVolume) and unreal.Name(TAG) in a.tags, "同名Actor不属于本次薄雾"
        else:
            a = actors.spawn_actor_from_class(unreal.LocalFogVolume, unreal.Vector(*position))
        a.set_actor_label(label)
        a.set_folder_path("Village/Atmosphere/MorningMist")
        a.set_actor_location(unreal.Vector(*position), False, False)
        a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
        a.tags = list(dict.fromkeys(list(a.tags) + [unreal.Name(TAG)]))
        c = a.get_component_by_class(unreal.LocalFogVolumeComponent)
        for prop, value in {"radial_fog_extinction": .025, "height_fog_extinction": .32,
                            "height_fog_falloff": 2000., "height_fog_offset": 0.,
                            "fog_phase_g": .2, "fog_albedo": srgb_color((203, 216, 230)),
                            "fog_emissive": unreal.LinearColor(0, 0, 0, 1),
                            "fog_start_distance": 1200.}.items():
            c.set_editor_property(prop, value)
    assert player_starts() == before["playerStarts"], "PlayerStart发生非预期变化，禁止保存"
    assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level(), "UE地图保存失败"
    result = inspect()
    result.update(status="SavedPendingColdAndVisualVerification", backup=str(backup),
                  mapSHA256=hashlib.sha256(map_file.read_bytes()).hexdigest())
    (OUT / "Authoring.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print("VILLAGE_MIST_SAVED", len(result["fog"]), str(OUT / "Authoring.json"))
    return result
