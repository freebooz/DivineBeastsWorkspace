# -*- coding: utf-8 -*-
"""SSLM: 只读扫描正式新手村湖泊现有Actor、材质和几何范围。此脚本绝不保存/修改地图。"""
import unreal
import json
import collections
import os
import traceback

TARGET = "/DBAWorldPack_Village/Maps/L_Village_Start"
DEST = os.path.abspath(os.path.join(unreal.Paths.project_saved_dir(), "Validation", "Water", "VillageLakeInspection.json"))


def vec(v):
    return [round(float(v.x), 2), round(float(v.y), 2), round(float(v.z), 2)]


def main():
    subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not subsystem:
        raise RuntimeError("没有LevelEditorSubsystem")
    if not subsystem.load_level(TARGET):
        raise RuntimeError("无法打开正式新手村地图（只读扫描）")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world or str(world.get_outermost().get_name()) != TARGET:
        raise RuntimeError("编辑器加载地图身份不符")
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = list(actor_subsystem.get_all_level_actors())
    categories = collections.Counter()
    items = []
    for a in actors:
        try:
            name = str(a.get_actor_label())
            cls = str(a.get_class().get_name())
            categories[cls] += 1
            origin, extents = a.get_actor_bounds(False)
            center = vec(origin)
            extent = vec(extents)
            location = vec(a.get_actor_location())
            relevant = any(k in (name + cls).lower() for k in
                           ["water", "lake", "pool", "river", "湖", "池", "水", "plane", "landscape", "zone"])
            relevant = relevant or (extents.x > 2000 and extents.y > 2000 and extents.z < 250)
            if not relevant:
                continue
            record = {"label": name, "class": cls, "locationCm": location,
                      "boundsCenterCm": center, "boundsHalfExtentCm": extent,
                      "object": str(a.get_path_name())}
            try:
                if isinstance(a, unreal.StaticMeshActor):
                    comp = a.get_editor_property("static_mesh_component")
                    mesh = comp.get_editor_property("static_mesh")
                    record["staticMesh"] = str(mesh.get_path_name()) if mesh else None
                    mats = []
                    for i in range(comp.get_num_materials()):
                        material = comp.get_material(i)
                        mats.append(str(material.get_path_name()) if material else "")
                    record["materials"] = mats
            except Exception as e:
                record["meshInspectionError"] = str(e)
            items.append(record)
        except Exception as e:
            items.append({"actorError": str(e)})
    report = {"project": unreal.Paths.get_project_file_path(), "level": TARGET,
              "actorCountLoaded": len(actors), "classCount": dict(categories),
              "relevantActors": items, "notes": "仅当前编辑器可见/已加载Actor；WP未加载Cell需另审查"}
    os.makedirs(os.path.dirname(DEST), exist_ok=True)
    with open(DEST, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    unreal.log("SSLM_VILLAGE_LAKE_INSPECTION_OK " + DEST +
               " loaded=" + str(len(actors)) + " candidates=" + str(len(items)))


try:
    main()
except Exception:
    unreal.log_error("SSLM_VILLAGE_LAKE_INSPECTION_FAILED " + traceback.format_exc())
    raise
