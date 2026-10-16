"""独立重开狼衍生Blend并重新导入FBX，核对原FBX骨架身份、动作时长和侧倒接地。

只验证源资源，不修改UE资产；输出JSON供Monolith导入阶段复核，不代替客户端动画验收。
"""
from __future__ import annotations

import json
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parents[3]
SOURCE = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/SourceArt/Creatures/Wolf"
DERIVED = SOURCE / "DerivedAnimations"


def import_signature(path: Path) -> dict:
    """在独立空场景导入一个FBX，读取骨架父链/静止矩阵和动作时长，防止同名残留误判。"""
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.render.fps = 30
    bpy.ops.import_scene.fbx(filepath=str(path))
    rigs = [obj for obj in bpy.data.objects if obj.type == "ARMATURE"]
    assert len(rigs) == 1, "动画源必须仅有一套原始狼骨架"
    rig = rigs[0]
    return {"rig": rig.name, "bones": {b.name: {"parent": b.parent.name if b.parent else None,
        "matrix": [v for row in b.matrix_local for v in row]} for b in rig.data.bones},
        "actions": [{"name": a.name, "duration_seconds": (a.frame_range[1] - a.frame_range[0]) / 30}
                    for a in bpy.data.actions]}


def height(mesh) -> float:
    """真实蒙皮求值的世界最低高度，单位为源场景单位；不把骨骼位置当成网格接地。"""
    evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
    return min((evaluated.matrix_world @ v.co).z for v in evaluated.data.vertices)


def main():
    """独立重开确认动作可编辑，导出骨架与既有原FBX一致，并登记可追溯数值证据。"""
    original = import_signature(SOURCE / "Wolf.fbx")
    output = {}
    for action, seconds in [("Bite", 0.8), ("Death", 1.6)]:
        imported = import_signature(DERIVED / f"Wolf_{action}.fbx")
        assert imported["rig"] == original["rig"]
        assert set(imported["bones"]) == set(original["bones"]), "不得增加/遗漏既有骨骼"
        error = 0.0
        for name, bone in imported["bones"].items():
            assert bone["parent"] == original["bones"][name]["parent"], name
            error = max(error, max(abs(a - b) for a, b in zip(bone["matrix"], original["bones"][name]["matrix"])))
        assert error < 0.0001, f"原骨架静止矩阵发生变化: {error}"
        assert len(imported["actions"]) == 1
        assert abs(imported["actions"][0]["duration_seconds"] - seconds) < 0.0001
        output[action] = {"bone_count": len(imported["bones"]), "max_rest_matrix_error": error,
                          "duration_seconds": imported["actions"][0]["duration_seconds"]}
    bpy.ops.wm.open_mainfile(filepath=str(DERIVED / "Wolf_CombatActions.blend"))
    rig, mesh = bpy.data.objects["WolfArmature"], bpy.data.objects["Wolf"]
    assert {"Idle", "Walking", "Bite", "Death"}.issubset({a.name for a in bpy.data.actions})
    scene = bpy.context.scene
    rig.animation_data.action = bpy.data.actions["Death"]
    heights = []
    for frame in range(1, 50):
        scene.frame_set(frame)
        heights.append(height(mesh))
    assert max(heights) - min(heights) < 0.02, "侧倒动作真实蒙皮不得穿地或悬浮"
    output["saved_blend"] = {"actions": [a.name for a in bpy.data.actions],
        "death_floor_min": min(heights), "death_floor_max": max(heights),
        "original_preserved": True, "ue_import_and_client_validation": "pending"}
    (DERIVED / "SourceValidation.json").write_text(json.dumps(output, ensure_ascii=False, indent=2), encoding="utf-8")
    print("WOLF_ANIMATION_SOURCE_VERIFIED=" + json.dumps(output, ensure_ascii=False))


if __name__ == "__main__":
    main()
