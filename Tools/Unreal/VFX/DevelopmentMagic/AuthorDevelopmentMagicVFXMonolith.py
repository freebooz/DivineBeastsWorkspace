#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""原创开发魔法Niagara作者。全部真实材质/脚本/系统/定义由Monolith序列化，不写假资产。
三种外观共用唯一平台VFX框架；这里只定义项目内容。核心/光晕/尾迹分层，有限粒子和柔和宽高光。
模式inspect只打印计划；apply前核对本轮编辑器身份，动作失败立即停止，不自动重放。
回退：新目录可保留未启用，旧技能/VFX不删除；发布必须额外验证保存重载、真实GPU/客户端与服务器剥离。
"""
import argparse
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DEST = "/DBAPresentationPack_Core/VFX/DevelopmentMagic"
SPELLS = {"Fireball": (1.8, 0.35, 0.035), "Frostbolt": (0.16, 0.9, 1.65), "ArcaneMissiles": (0.72, 0.28, 1.8)}


def protocol():
    """复用项目现有Monolith JSON-RPC解包与失败门禁，独立保存本轮证据，不挪用Fauna所有权检查。"""
    spec = importlib.util.spec_from_file_location("magic_monolith_transport", ROOT / "Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py")
    module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    module.EVIDENCE = ROOT / "Saved/Validation/MagicHUD20261012/VFXAuthoring"
    module.editor_python("import unreal\nassert '-MagicHUDOwnedAuthoring' in unreal.SystemLibrary.get_command_line()\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\nprint('MAGIC_EDITOR_IDENTITY_VERIFIED')")
    return module


def author_materials_and_definitions_source():
    """返回由Monolith执行的UE Python：两份无噪声材质，材质所有权在项目视觉内容包。"""
    return '''import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials'
for name, mesh in [('M_DBA_DevMagicGlow',False),('M_DBA_DevMagicCrystal',True)]:
    path=root+'/'+name
    material=unreal.load_asset(path)
    if material is None: material=tools.create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
    if not isinstance(material,unreal.Material): raise RuntimeError('同名材质类型冲突')
    # 只接受本轮专用目录，重跑前先清本轮表达式；不修改平台材质或用户场景。
    lib=unreal.MaterialEditingLibrary
    lib.delete_all_material_expressions(material)
    material.set_editor_property('two_sided',True)
    material.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT if mesh else unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT if mesh else unreal.MaterialShadingModel.MSM_UNLIT)
    def expression(cls,x=0,y=0): return lib.create_material_expression(material,cls,x,y)
    particle=expression(unreal.MaterialExpressionParticleColor,-500,-150)
    lib.connect_material_property(particle,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if mesh:
        lib.connect_material_property(particle,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
        for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,0.12),(unreal.MaterialProperty.MP_METALLIC,0.08),(unreal.MaterialProperty.MP_OPACITY,0.85)]:
            scalar=expression(unreal.MaterialExpressionConstant,-200,200); scalar.set_editor_property('r',value)
            lib.connect_material_property(scalar,'',prop)
    else:
        uv=expression(unreal.MaterialExpressionTextureCoordinate,-900,200)
        center=expression(unreal.MaterialExpressionConstant2Vector,-900,350); center.set_editor_property('r',0.5); center.set_editor_property('g',0.5)
        delta=expression(unreal.MaterialExpressionSubtract,-700,200)
        lib.connect_material_expressions(uv,'',delta,'A'); lib.connect_material_expressions(center,'',delta,'B')
        length=expression(unreal.MaterialExpressionLength,-500,200); lib.connect_material_expressions(delta,'',length,'Input')
        scale=expression(unreal.MaterialExpressionMultiply,-350,200); scale.set_editor_property('const_b',2.0); lib.connect_material_expressions(length,'',scale,'A')
        inv=expression(unreal.MaterialExpressionOneMinus,-200,200); lib.connect_material_expressions(scale,'',inv,'Input')
        clamp=expression(unreal.MaterialExpressionSaturate,-50,200); lib.connect_material_expressions(inv,'',clamp,'Input')
        power=expression(unreal.MaterialExpressionPower,100,200); power.set_editor_property('const_exponent',2.0); lib.connect_material_expressions(clamp,'',power,'Base')
        alpha=expression(unreal.MaterialExpressionMultiply,250,200)
        lib.connect_material_expressions(power,'',alpha,'A'); lib.connect_material_expressions(particle,'A',alpha,'B')
        lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material,False): raise RuntimeError('材质保存失败')
    print('MAGIC_MATERIAL_SAVED',path)
'''


def module_source(color):
    """纯GPU外观表达式；核心/环绕/尾迹/命中控制不参与服务器路径或碰撞。
    低频变化、宽连续高光；没有噪声函数、粒子物理碰撞或权威伤害读取。
    """
    return f'''#if GPU_SIMULATION
float3 tint=float3({color[0]},{color[1]},{color[2]});
float a=Age; float phase=Index*2.399963+Age*2.0;
float envelope=1.0;
if(Mode==2) envelope=saturate(1.0-a/0.35);
if(Mode>=3) envelope=pow(saturate(1.0-a/0.8),1.4);
Particles.Color=float4(tint, envelope*(Mode==0 ? 0.8 : 0.42));
Particles.SpriteRotation=Index*137.50776;
Particles.SpriteSize=float2(Mode==0 ? 34.0 : Mode==1 ? 28.0 : Mode==2 ? 22.0 : 45.0,
                           Mode==0 ? 34.0 : Mode==1 ? 28.0 : Mode==2 ? 30.0 : 45.0);
Particles.Scale=float3(0.25,0.25,0.25)*envelope;
if(Mode==0) {{ Particles.Position=float3(0,0,0); }}
else if(Mode==1) {{ Particles.Position=float3(cos(phase)*14.0,sin(phase)*14.0,sin(phase*0.6)*9.0); }}
else if(Mode>=3) {{
    float radius=12.0+Age*80.0;
    Particles.Position=float3(cos(phase)*radius,sin(phase)*radius,sin(Index*1.7)*radius*0.6);
    Particles.Scale=float3(0.12,0.12,0.3)*envelope;
}}
#endif'''


def definitions_source():
    """平台附着/瞬时Definition，稳定逻辑ID/软资源/最长生命；不新建播放器或专用VFX插件。"""
    return '''import unreal
tools=unreal.AssetToolsHelpers.get_asset_tools()
root='/DBAPresentationPack_Core/VFX/DevelopmentMagic'
for spell in ['Fireball','Frostbolt','ArcaneMissiles']:
    for stage in ['Flight','Impact']:
        cls=unreal.GamePlatformVFXAttachedDefinition if stage=='Flight' else unreal.GamePlatformVFXInstantDefinition
        name='DA_DBA_VFX_Dev'+spell+stage; path=root+'/Definitions/'+name
        d=unreal.load_asset(path)
        if d is None:
            factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',cls)
            d=tools.create_asset(name,root+'/Definitions',cls,factory)
        if not isinstance(d,cls): raise RuntimeError('定义类型冲突: '+path)
        identifier='presentation.dba.development.magic.'+spell.lower()+'.'+stage.lower()+'@1'
        namespace,tail=identifier.rsplit('.',1); part,version=tail.rsplit('@',1)
        logical=unreal.GamePlatformId(); logical.set_editor_property('namespace',namespace); logical.set_editor_property('name',part); logical.set_editor_property('logical_version',int(version))
        d.set_editor_property('logical_id',logical)
        system=unreal.load_asset(root+'/Niagara/NS_DBA_Dev'+spell+stage)
        if not isinstance(system,unreal.NiagaraSystem): raise RuntimeError('系统未制作')
        d.set_editor_property('niagara_system',system)
        d.set_editor_property('auto_destroy',True); d.set_editor_property('allow_pooling',True)
        d.set_editor_property('max_lifetime_seconds',3.0 if stage=='Flight' else 0.8)
        if not unreal.EditorAssetLibrary.save_loaded_asset(d,False): raise RuntimeError('定义保存失败')
        print('MAGIC_DEFINITION_SAVED',path)
'''


def author():
    """先材质、再每种系统，最后六定义；编译失败停在失败阶段，不能发布半激活目录。"""
    p = protocol()
    p.editor_python(author_materials_and_definitions_source())
    results = []
    for spell, color in SPELLS.items():
        module = DEST + "/Niagara/NM_DBA_Dev" + spell + "Visual"
        p.niagara("create_module_from_hlsl", name="Dev" + spell + "Visual", save_path=module, hlsl=module_source(color),
                  inputs=[{"name": "Age", "type": "float"}, {"name": "Index", "type": "int"}, {"name": "Mode", "type": "int"}], outputs=[],
                  description="原创开发魔法纯GPU外观；低频柔和核心/环绕/尾迹/命中，不读取伤害。")
        for stage in ("Flight", "Impact"):
            system = DEST + "/Niagara/NS_DBA_Dev" + spell + stage
            emitter_specs = []
            modes = [("Core", 0, 1), ("Aura", 1, 12), ("Trail", 2, 80)] if stage == "Flight" else [("Flash", 3, 8), ("Halo", 3, 18), ("Fragments", 3, 14)]
            for name, mode, count in modes:
                spawn_script = "/Niagara/Modules/Emitter/SpawnRate.SpawnRate" if name == "Trail" else "/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"
                emitter_specs.append({"name": name, "asset": "/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal",
                    "properties": {"SimTarget": "GPUComputeSim", "bLocalSpace": "False" if name == "Trail" else "True"},
                    "modules": [{"stage": "emitter_update", "script": spawn_script, "inputs": {"SpawnRate" if name == "Trail" else "Spawn Count": str(count)}},
                                {"stage": "particle_update", "script": module, "bindings": {"Age": "Particles.Age", "Index": "Particles.UniqueID"}, "inputs": {"Mode": str(mode)}}],
                    "renderers": [{"class": "Mesh", "material": DEST + "/Materials/M_DBA_DevMagicCrystal"}] if name in ("Core", "Fragments") else []})
            p.niagara("create_system_from_spec", save_path=system, spec={"emitters": emitter_specs})
            for name, mode, count in modes:
                renderers = p.niagara("list_renderers", asset_path=system, emitter=name)["renderers"]
                if name in ("Core", "Fragments"):
                    for renderer in reversed(renderers):
                        if renderer["type"] == "sprite": p.niagara("remove_renderer", asset_path=system, emitter=name, renderer_index=renderer["index"])
                    p.niagara("set_renderer_mesh", asset_path=system, emitter=name, renderer_index=0,
                              mesh="/Engine/BasicShapes/Cone" if spell == "Frostbolt" else "/Engine/BasicShapes/Sphere")
                else:
                    p.niagara("set_renderer_material", asset_path=system, emitter=name, renderer_index=0, material=DEST + "/Materials/M_DBA_DevMagicGlow")
                p.niagara("set_module_input_value", asset_path=system, emitter=name, module_node="InitializeParticle", input="Lifetime", value="0.35" if name == "Trail" else "3.0" if stage == "Flight" else "0.8")
                p.niagara("set_static_switch_value", asset_path=system, emitter=name, module_node="EmitterState", input="Life Cycle Mode", value="Self")
                p.niagara("set_emitter_loop_profile", asset_path=system, emitter=name, loop_behavior="Once", loop_duration=2.5 if stage == "Flight" else 0.3)
                p.niagara("set_fixed_bounds", asset_path=system, emitter=name, min=[-200,-200,-200], max=[200,200,200], enabled=True)
            p.niagara("set_fixed_bounds", asset_path=system, min=[-250,-250,-250], max=[250,250,250], enabled=True)
            diagnostics = p.niagara("get_system_diagnostics", asset_path=system, compile_first=True)
            if diagnostics.get("error_count", 1) != 0: raise RuntimeError("Niagara编译失败: " + system + " " + json.dumps(diagnostics))
            p.niagara("save_system", asset_path=system, only_if_dirty=False)
            results.append({"Asset": system, "Diagnostics": diagnostics})
    p.editor_python(definitions_source())
    output = ROOT / "Saved/Validation/MagicHUD20261012/VFXAuthoring/Systems.json"
    output.write_text(json.dumps({"Systems": results, "ManualVisualAccepted": False, "ServerCookVerified": False}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"CompiledSystems": len(results), "Evidence": str(output)}, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--apply", action="store_true"); args = parser.parse_args()
    if args.apply: author()
    else: print(json.dumps({"Assets": [DEST + "/Niagara/NS_DBA_Dev" + name + stage for name in SPELLS for stage in ("Flight", "Impact")], "ActualAssetsModified": False}))
