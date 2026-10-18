#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM新手村地形材质真实UE5.8只读图审计；不生成或保存任何资产。"""
from __future__ import annotations
import json
from pathlib import Path

MASTER = "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
FUNCTION = "/GamePlatformSurface/MaterialFunctions/Core/MF_GP_TextureAntiTiling"

def view_pin(expr, name):
    try:
        raw = expr.get_editor_property(name)
        if hasattr(raw, "get_editor_property"):
            origin = raw.get_editor_property("expression")
            return origin.get_name() if origin else None
        return str(raw)[:200]
    except Exception:
        return None

def main():
    import unreal # type: ignore
    a = unreal.EditorAssetLibrary
    lib = unreal.MaterialEditingLibrary
    m = a.load_asset(MASTER)
    if not isinstance(m, unreal.Material):
        raise RuntimeError("未发现UE新手村真实母材质")
    expressions = lib.get_material_expressions(m)
    result = []
    for x in expressions:
        row = {"name":x.get_name(),"class":x.get_class().get_name()}
        for field in ("parameter_name","texture","code","desc","sampler_type","texture_object"):
            try:
                value = x.get_editor_property(field)
                if value is not None:
                    row[field] = str(value)[:220]
            except Exception: pass
        linked = {}
        for field in ("a","b","alpha","coordinates","uvs","input","texture_object","texture"):
            source = view_pin(x,field)
            if source is not None: linked[field] = source
        if linked: row["pins"] = linked
        result.append(row)
    # 只读连接信息：记录各基础色/法线/ORM采样如何进入输出和雪层链路。
    critical = []
    for x in expressions:
        nodes = []
        try:
            nodes = [i.get_name() for i in lib.get_inputs_for_material_expression(m, x)]
        except Exception as exc:
            nodes = ["not_available:" + str(exc)[:90]]
        if nodes or "TextureSample" in x.get_class().get_name():
            try: input_names = [str(i) for i in lib.get_material_expression_input_names(x)]
            except Exception: input_names = []
            critical.append({"target":x.get_name(),"sources":nodes,"input_names":input_names})
    roots = {}
    for alias, prop in (("BaseColor",unreal.MaterialProperty.MP_BASE_COLOR),
                        ("Normal",unreal.MaterialProperty.MP_NORMAL),
                        ("Roughness",unreal.MaterialProperty.MP_ROUGHNESS)):
        try:
            n=lib.get_material_property_input_node(m,prop)
            roots[alias]=n.get_name() if n else None
        except Exception as exc:
            roots[alias]="unavailable:"+str(exc)[:100]
    print("SSLM_VILLAGE_BOMBING_CONNECTIONS",json.dumps({"roots":roots,"nodes":critical},ensure_ascii=False),flush=True)
    fn = a.load_asset(FUNCTION)
    print("SSLM_VILLAGE_BOMBING_INSPECT", json.dumps({
        "master":MASTER, "exprCount":len(expressions),
        "functionAvailable":fn is not None,
        "nodes":result,
    },ensure_ascii=False), flush=True)

if __name__=="__main__": main()
