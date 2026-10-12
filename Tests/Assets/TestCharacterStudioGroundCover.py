#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""只读检查角色工作室桃花落地合同及真实稀疏落叶；不修改UE资产或业务状态。

输入是正式编辑器的Monolith端点。两层桃花必须切断地面扬起风绑定，空中风不变；
工作室须存在少量无碰撞叶片，使用已批准项目材质。此项是资产门禁，不冒充GPU动态验收。
"""
import argparse
import json
from TestCharacterEntryMonolithLayout import call

SYSTEM = "/DBAPresentationPack_Core/VFX/Environment/FallingFoliage/Niagara/NS_DBA_FallingFoliage_Peach"


def audit(endpoint):
    """读取真实函数调用输入，绑定回风或移除地面叶片均应失败。"""
    errors = []
    spec = call(endpoint, "niagara_query", {"action": "export_system_spec", "params": {"asset_path": SYSTEM}})["spec"]
    emitters = spec["emitters"]
    if len(emitters) != 2:
        errors.append("桃花近远两层不完整")
    for emitter in emitters:
        ground = [m for m in emitter["modules"] if "NM_GP_FoliageGroundMotion." in m["script"]]
        if len(ground) != 1:
            errors.append(emitter["name"] + "落地调用不唯一")
            continue
        value = call(endpoint, "niagara_query", {"action": "get_module_input_value", "params": {
            "asset_path": SYSTEM, "emitter": emitter["name"], "module_node": ground[0]["guid"], "input": "WindStrength"}})
        if value.get("is_linked") or value.get("is_dynamic_input") or float(value.get("value", "nan")) != 0:
            errors.append(emitter["name"] + "仍能在触地后产生向上阵风速度")
    result = call(endpoint, "editor_query", {"action": "run_python", "params": {"unattended": True, "command":
        "import unreal,json; actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors(); "
        "leaves=[a for a in actors if a.actor_has_tag('DBA.FrontEnd.GroundCover')]; "
        "print(json.dumps({'world':unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_path_name(),"
        "'leaves':[{'name':a.get_actor_label(),'noCollision':a.static_mesh_component.get_collision_enabled()==unreal.CollisionEnabled.NO_COLLISION,"
        "'material':a.static_mesh_component.get_material(0).get_path_name(),'scale':[a.get_actor_scale3d().x,a.get_actor_scale3d().y]} for a in leaves]}))"}})
    records = [json.loads(block["output"]) for block in result.get("output", []) if block.get("type") == "info" and block["output"].lstrip().startswith("{")]
    if len(records) != 1 or "/L_DBA_CharacterStudio." not in records[0]["world"]:
        errors.append("未读取正式工作室地图")
    else:
        leaves = records[0]["leaves"]
        if not 24 <= len(leaves) <= 64:
            errors.append("地面叶片缺失或过密：" + str(len(leaves)))
        for leaf in leaves:
            if not leaf["noCollision"] or not leaf["material"].startswith("/DBAPresentationPack_Core/"):
                errors.append("叶片误含玩法碰撞或非项目材质：" + leaf["name"])
            if not all(.035 <= size <= .12 for size in leaf["scale"]):
                errors.append("叶片尺寸异常：" + leaf["name"])
    return {"Passed": not errors, "Errors": errors, "GPUVerified": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="http://127.0.0.1:9316/mcp")
    report = audit(parser.parse_args().endpoint)
    print(json.dumps(report, ensure_ascii=False))
    raise SystemExit(0 if report["Passed"] else 1)
