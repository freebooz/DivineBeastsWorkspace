#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""依据真实UE5.8 Niagara引擎模板生成Monolith可执行资源制作载荷。

此脚本只生成JSON规格，不写uasset。执行期必须通过正在运行的UE编辑器、
Monolith Niagara的create_system_from_spec写资源，逐Emitter检查renderers、shader编译、
保存回读及质量预算后才能算交付。
模块路径来自锁定引擎F:/UnrealEngine-5.8.0-release中的真实.uasset路径，
单个雨/雪核心系统复用近景和远景Emitter，不按天气强弱复制系统。
"""
from __future__ import annotations

import json
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[2]
DESIGN = HERE / "WeatherNiagaraAuthoringSpec_V1.json"
OUT = HERE / "WeatherNiagaraMonolithPayloads_V1.json"

# 锁定引擎现有的"Minimal" NiagaraEmitter模板资产；不是人造路径。
SOURCE_EMITTER = "/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal"
SPAWN_RATE = "/Niagara/Modules/Emitter/SpawnRate.SpawnRate"
INIT = "/Niagara/Modules/Spawn/Initialization/InitializeParticle.InitializeParticle"
BOX = "/Niagara/Modules/Spawn/Location/BoxLocation.BoxLocation"
VELOCITY = "/Niagara/Modules/Spawn/Velocity/AddVelocity.AddVelocity"
GRAVITY = "/Niagara/Modules/Update/Forces/GravityForce.GravityForce"
SOLVE = "/Niagara/Modules/Solvers/SolveForcesAndVelocity.SolveForcesAndVelocity"


def main() -> None:
    design = json.loads(DESIGN.read_text(encoding="utf-8"))
    if design["schemaVersion"] != 1 or len(design["systems"]) != 3:
        raise ValueError("天气VFX设计规格数量不符")
    ids = {
        "Rain": ("WeatherNearSpawnRate", "WeatherFarSpawnRate"),
        "Snow": ("WeatherNearSpawnRate", "WeatherFarSpawnRate"),
    }
    payloads = []
    for item in design["systems"]:
        name = item["name"]
        weather = "Rain" if "Rain" in name else "Snow"
        nearby = "Near"
        emitters = []
        for index, e in enumerate(item["emitters"]):
            spawn_variable = "User.WeatherNearSpawnRate" if index == 0 else "User.WeatherFarSpawnRate"
            modules = [
                {"stage": "emitter_update", "script": SPAWN_RATE,
                 "bindings": {"Spawn Rate": spawn_variable}},
                {"stage": "particle_spawn", "script": INIT},
                {"stage": "particle_spawn", "script": BOX},
                {"stage": "particle_spawn", "script": VELOCITY},
                {"stage": "particle_update", "script": SOLVE},
            ]
            if weather == "Rain" and name.endswith("_Rain"):
                modules.insert(-1, {"stage": "particle_update", "script": GRAVITY})
            emitters.append({
                "name": e["name"],
                "asset": SOURCE_EMITTER,
                "modules": modules,
                "renderers": [{
                    "class": "Sprite",
                    "material": design["rules"]["materialRoot"] + "/" + e["spriteMaterial"]
                }],
            })
        near_default = item["emitters"][0].get("spawnRateAtIntensity1", 90) * 0.5
        far_default = item["emitters"][1].get("spawnRateAtIntensity1", 0) * 0.5 if len(item["emitters"]) > 1 else 0
        spec = {
            "user_parameters": [
                {"name": "User.WeatherIntensity", "type": "Float", "default": 0.5},
                {"name": "User.WeatherNearSpawnRate", "type": "Float", "default": near_default},
                {"name": "User.WeatherFarSpawnRate", "type": "Float", "default": far_default},
            ],
            "emitters": emitters,
        }
        payloads.append({
            "action": "create_system_from_spec",
            "params": {
                "save_path": design["rules"]["systemRoot"] + "/" + name,
                "spec": spec,
            },
            "validation": {
                "expectedEmitters": len(emitters),
                "minimumRenderersPerEmitter": 1,
                "requireModuleInputsBound": [spawn_variable for spawn_variable in [
                    "User.WeatherNearSpawnRate",
                    "User.WeatherFarSpawnRate" if len(emitters) > 1 else "",
                ] if spawn_variable],
                "requireShaderCompile": True,
                "requireSaveAndReload": True,
                "requireNonEmptySystem": True,
            },
            "notes": "仅供真实Monolith执行：每次返回failed_steps/errors必须为0；创建前必须先有真实粒子材质与有效模板。",
        })
    result = {
        "schemaVersion": 1,
        "state": "prepared_specs_not_ue_assets",
        "monolithNamespace": "niagara",
        "tool": "niagara_query",
        "sourceEmitter": SOURCE_EMITTER,
        "moduleScripts": [SPAWN_RATE, INIT, BOX, VELOCITY, GRAVITY, SOLVE],
        "payloads": payloads,
        "workflow": [
            "确认Niagara模板.uasset及全部模块脚本确实存在于锁定UE5.8。",
            "确保9张真实Texture2D及三张真实雨雪VFX材质已通过UE保存和回读。",
            "调用niagara_query(create_system_from_spec)逐系统制作；验证failed_steps=0。",
            "使用get_ordered_modules/get_module_inputs检查SpawnRate绑定User.WeatherNear/FarSpawnRate。",
            "逐System调用request_compile/get_system_diagnostics/validate_system/save_system，检查真实渲染器材质和Shader。",
            "禁止将空发射器或只有名称的uasset标记已完成。",
        ],
    }
    content = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if OUT.exists() and OUT.read_text(encoding="utf-8") != content:
        raise FileExistsError("已有Niagara执行载荷经过修改，拒绝自动覆盖；请先人工审查")
    OUT.write_text(content, encoding="utf-8")
    print("MONOLITH_NIAGARA_SPEC_PREPARED", len(payloads),
          "EMITTERS", sum(len(p["params"]["spec"]["emitters"]) for p in payloads))
    print("NOT_CREATED_NIAGARA_UASSET", str(OUT))


if __name__ == "__main__":
    main()
