"""独立UE进程读取已保存村庄薄雾，不写回地图；不代替RHI/GPU/人工验收。"""
import hashlib
import json
import math
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parent
source = root / "Tools/Unreal/World/AuthorVillageMistUE.py"
exec(compile(source.read_text(encoding="utf-8"), str(source), "exec"))
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(MAP)
report = inspect()
expected = json.loads((OUT / "Authoring.json").read_text(encoding="utf-8"))
assert report["playerStarts"] == expected["playerStarts"], "出生点合同变化"
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
assert not any(a.get_actor_label().startswith("Transient_Mist_") for a in actors), "审查代理不应进入保存地图"
fogs = [a for a in actors if isinstance(a, unreal.ExponentialHeightFog)]
assert len(fogs) == 1
c = fogs[0].get_component_by_class(unreal.ExponentialHeightFogComponent)
for key, value in {"fog_density": .008, "fog_height_falloff": .18,
                   "fog_max_opacity": .32, "start_distance": 1200.}.items():
    assert math.isclose(c.get_editor_property(key), value, abs_tol=1e-5), key
assert not c.get_editor_property("enable_volumetric_fog")
color = c.get_editor_property("fog_inscattering_luminance")
expected_color = srgb_color((72, 93, 115))
assert all(math.isclose(getattr(color, k), getattr(expected_color, k), abs_tol=1e-5) for k in ("r", "g", "b"))
local = [a for a in actors if isinstance(a, unreal.LocalFogVolume)]
assert len(local) == 3
by_name = {a.get_actor_label(): a for a in local}
for name, xyz, scale in LOCAL_VOLUMES:
    a = by_name[name]
    assert unreal.Name(TAG) in a.tags
    assert (a.get_actor_location() - unreal.Vector(*xyz)).length() < .01
    assert (a.get_actor_scale3d() - unreal.Vector(scale, scale, scale)).length() < .01
    lc = a.get_component_by_class(unreal.LocalFogVolumeComponent)
    for key, value in {"radial_fog_extinction": .025, "height_fog_extinction": .32,
                       "height_fog_falloff": 2000., "fog_start_distance": 1200.}.items():
        assert math.isclose(lc.get_editor_property(key), value, abs_tol=1e-5), name + "/" + key
# 最新用户明确指定偏暗月光；在同一次独立重载中核对真实灯光和曝光。
main = [a for a in actors if isinstance(a, unreal.DirectionalLight)]
sky = [a for a in actors if isinstance(a, unreal.SkyLight)]
assert len(main) == len(sky) == 1
moon = main[0].get_component_by_class(unreal.DirectionalLightComponent)
assert math.isclose(moon.get_editor_property("intensity"), .85, abs_tol=1e-5)
moon_color = moon.get_editor_property("light_color")
assert (moon_color.r, moon_color.g, moon_color.b) == (190, 214, 242)
assert math.isclose(sky[0].get_component_by_class(unreal.SkyLightComponent).get_editor_property("intensity"), .18, abs_tol=1e-5)
exposure = [a for a in actors if a.get_actor_label() == "VillageMoonlightExposure"]
assert len(exposure) == 1 and exposure[0].get_editor_property("unbound")
settings = exposure[0].get_editor_property("settings")
assert settings.get_editor_property("auto_exposure_method") == unreal.AutoExposureMethod.AEM_MANUAL
assert settings.get_editor_property("override_auto_exposure_method")
assert math.isclose(settings.get_editor_property("auto_exposure_bias"), 2.3, abs_tol=1e-5)
map_file = CONTENT / "Maps/L_Village_Start.umap"
actual_hash = hashlib.sha256(map_file.read_bytes()).hexdigest()
assert actual_hash == expected["mapSHA256"], "保存后地图被其他写入改动"
assert not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()
report.update(status="PASS", mapSHA256=actual_hash, independentColdLoad=True,
              playerStartsPreserved=True, transientReviewActorsSaved=False, moonlightVerified=True,
              moonIntensity=.85, skyIntensity=.18, fixedExposureBias=2.3)
(OUT / "ColdValidation.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print("VILLAGE_MIST_COLD_LOAD_PASS", actual_hash)
