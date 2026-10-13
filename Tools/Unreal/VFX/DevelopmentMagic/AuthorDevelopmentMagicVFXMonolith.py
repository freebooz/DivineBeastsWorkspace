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
    lib.set_base_material_usage(material,unreal.MaterialUsage.MATUSAGE_NIAGARA_MESH_PARTICLES if mesh else unreal.MaterialUsage.MATUSAGE_NIAGARA_SPRITES,True)
    def expression(cls,x=0,y=0): return lib.create_material_expression(material,cls,x,y)
    particle=expression(unreal.MaterialExpressionParticleColor,-500,-150)
    lib.connect_material_property(particle,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if mesh:
        lib.connect_material_property(particle,'RGB',unreal.MaterialProperty.MP_BASE_COLOR)
        for prop,value in [(unreal.MaterialProperty.MP_ROUGHNESS,0.12),(unreal.MaterialProperty.MP_METALLIC,0.08),(unreal.MaterialProperty.MP_OPACITY,0.85)]:
            scalar=expression(unreal.MaterialExpressionConstant,-200,200); scalar.set_editor_property('r',value)
            lib.connect_material_property(scalar,'',prop)
        # 宽连续体积明暗；纯能量材质不靠碎片高光，预览无环境灯也应能辨识球体。
        fresnel=expression(unreal.MaterialExpressionFresnel,-500,450); fresnel.set_editor_property('exponent',2.0); fresnel.set_editor_property('base_reflect_fraction',0.0)
        inverted=expression(unreal.MaterialExpressionOneMinus,-250,450)
        if not lib.connect_material_expressions(fresnel,'',inverted,''): raise RuntimeError('Fresnel连接失败')
        contrast=expression(unreal.MaterialExpressionMultiply,-100,450); contrast.set_editor_property('const_b',0.7)
        lib.connect_material_expressions(inverted,'',contrast,'A')
        lift=expression(unreal.MaterialExpressionAdd,50,450); lift.set_editor_property('const_b',0.3); lib.connect_material_expressions(contrast,'',lift,'A')
        shaded=expression(unreal.MaterialExpressionMultiply,200,450); lib.connect_material_expressions(particle,'RGB',shaded,'A'); lib.connect_material_expressions(lift,'',shaded,'B')
        # 两条宽低频流动能量带让核心有可读体积与运动；不用Noise/锐化/碎点堆出质感。
        uv=expression(unreal.MaterialExpressionTextureCoordinate,-900,700)
        mask=expression(unreal.MaterialExpressionComponentMask,-750,700); mask.set_editor_property('r',True); mask.set_editor_property('g',False)
        if not lib.connect_material_expressions(uv,'',mask,''): raise RuntimeError('能量带UV输入连接失败')
        bands=expression(unreal.MaterialExpressionMultiply,-600,700); bands.set_editor_property('const_b',2.0); lib.connect_material_expressions(mask,'',bands,'A')
        time=expression(unreal.MaterialExpressionTime,-900,850)
        speed=expression(unreal.MaterialExpressionMultiply,-600,850); speed.set_editor_property('const_b',0.18); lib.connect_material_expressions(time,'',speed,'A')
        phase=expression(unreal.MaterialExpressionAdd,-400,700); lib.connect_material_expressions(bands,'',phase,'A'); lib.connect_material_expressions(speed,'',phase,'B')
        wave=expression(unreal.MaterialExpressionSine,-250,700); wave.set_editor_property('period',1.0)
        if not lib.connect_material_expressions(phase,'',wave,''): raise RuntimeError('能量带Sine输入连接失败')
        amplitude=expression(unreal.MaterialExpressionMultiply,-100,700); amplitude.set_editor_property('const_b',0.35); lib.connect_material_expressions(wave,'',amplitude,'A')
        baseline=expression(unreal.MaterialExpressionAdd,50,700); baseline.set_editor_property('const_b',0.8); lib.connect_material_expressions(amplitude,'',baseline,'A')
        energy=expression(unreal.MaterialExpressionMultiply,350,450); lib.connect_material_expressions(shaded,'',energy,'A'); lib.connect_material_expressions(baseline,'',energy,'B')
        lib.connect_material_property(energy,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        uv=expression(unreal.MaterialExpressionTextureCoordinate,-900,200)
        center=expression(unreal.MaterialExpressionConstant2Vector,-900,350); center.set_editor_property('r',0.5); center.set_editor_property('g',0.5)
        delta=expression(unreal.MaterialExpressionSubtract,-700,200)
        lib.connect_material_expressions(uv,'',delta,'A'); lib.connect_material_expressions(center,'',delta,'B')
        length=expression(unreal.MaterialExpressionLength,-500,200)
        if not lib.connect_material_expressions(delta,'',length,''): raise RuntimeError('径向Length输入连接失败')
        scale=expression(unreal.MaterialExpressionMultiply,-350,200); scale.set_editor_property('const_b',2.0); lib.connect_material_expressions(length,'',scale,'A')
        inv=expression(unreal.MaterialExpressionOneMinus,-200,200)
        if not lib.connect_material_expressions(scale,'',inv,''): raise RuntimeError('径向OneMinus连接失败')
        clamp=expression(unreal.MaterialExpressionSaturate,-50,200)
        if not lib.connect_material_expressions(inv,'',clamp,''): raise RuntimeError('径向Saturate连接失败')
        power=expression(unreal.MaterialExpressionPower,100,200); power.set_editor_property('const_exponent',2.0); lib.connect_material_expressions(clamp,'',power,'Base')
        alpha=expression(unreal.MaterialExpressionMultiply,250,200)
        lib.connect_material_expressions(power,'',alpha,'A'); lib.connect_material_expressions(particle,'A',alpha,'B')
        lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material,False): raise RuntimeError('材质保存失败')
    print('MAGIC_MATERIAL_SAVED',path)
'''


def module_source(color, spell):
    """纯GPU外观表达式；核心/环绕/尾迹/命中控制不参与服务器路径或碰撞。
    低频变化、宽连续高光；没有噪声函数、粒子物理碰撞或权威伤害读取。
    """
    return f'''#if GPU_SIMULATION
float3 tint=float3({color[0]},{color[1]},{color[2]});
float a=Age; float phase=Index*2.399963+Age*2.0;
float envelope=1.0;
if(Mode==2) envelope=saturate(1.0-a/0.5);
if(Mode>=3) envelope=pow(saturate(1.0-a/0.8),1.4);
Particles.Color=float4(tint, envelope*(Mode==0 ? 0.8 : Mode==1 ? 0.25 : 0.42));
Particles.SpriteRotation=Index*137.50776;
Particles.SpriteSize=float2(Mode==0 ? 34.0 : Mode==1 ? 36.0 : Mode==2 ? 30.0 : 50.0,
                           Mode==0 ? 34.0 : Mode==1 ? 36.0 : Mode==2 ? 40.0 : 50.0);
Particles.Scale=float3(0.25,0.25,0.25)*envelope;
Particles.MeshOrientation=float4(0,0,0,1);
if(Mode==0) {{ Particles.Position=float3(0,0,0); }}
else if(Mode==1) {{ Particles.Position=float3(cos(phase)*14.0,sin(phase)*14.0,sin(phase*0.6)*9.0); }}
else if(Mode>=3) {{
    float radius=12.0+Age*80.0;
    Particles.Position=float3(cos(phase)*radius,sin(phase)*radius,sin(Index*1.7)*radius*0.6);
    Particles.Scale=float3(0.12,0.12,0.3)*envelope;
}}
// 命中采用不同层：中心闪光、连续扩张环、少量晶体，不把三个发射器做成同一散点球。
if(Mode==3) {{ Particles.Position=float3(0,0,0); Particles.SpriteSize=float2(150,150)*(0.5+a); }}
if(Mode==4) {{ float q=Index*6.2831853/32.0; float r=12.0+a*95.0; Particles.Position=float3(0,cos(q)*r,sin(q)*r); Particles.SpriteSize=float2(38,38); }}
{'''// 冰箭沿弹体本地X轴，抛光晶体核与宽柔寒气区分；不是竖直锥体。
if(Mode==0) { Particles.Scale=float3(0.15,0.15,0.65); Particles.MeshOrientation=float4(0,0.70710678,0,0.70710678); }
if(Mode==1) { Particles.Position=float3(-Index*1.8,cos(phase)*9.0,sin(phase)*9.0); Particles.SpriteSize=float2(32,32); }
''' if spell=='Frostbolt' else '''// 螺旋光带只是外观；真实弹体螺旋和碰撞由平台服务器控制。
if(Mode==1) { float u=Index/28.0; float q=u*6.2831853+Age*3.0; Particles.Position=float3(-u*55.0,cos(q)*17.0,sin(q)*17.0); Particles.SpriteSize=float2(24,24); }
''' if spell=='ArcaneMissiles' else '''// 火焰花瓣拉长沿负X方向，宽柔叠层体现体积而不靠噪点。
if(Mode==1) { Particles.Position=float3(-Index*2.0,cos(phase)*14.0,sin(phase)*14.0); Particles.SpriteSize=float2(36,46); }
'''}
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
    # Monolith create_module在已有但未加载的包内NewObject会触发UE“只加载部分”保存断言。
    # 重建前显式加载本轮全部旧对象及依赖；不创建半加载同名包，不吞保存失败或自动改资源身份。
    p.editor_python("import unreal\npaths=unreal.EditorAssetLibrary.list_assets('/DBAPresentationPack_Core/VFX/DevelopmentMagic',True,False)\nfor path in paths:\n asset=unreal.load_asset(path)\n if asset is None: raise RuntimeError('重建前真实旧资产加载失败: '+path)\nprint('MAGIC_EXISTING_ASSETS_LOADED',len(paths))")
    p.editor_python(author_materials_and_definitions_source())
    results = []
    for spell, color in SPELLS.items():
        module = DEST + "/Niagara/NM_DBA_Dev" + spell + "Visual"
        p.niagara("create_module_from_hlsl", name="Dev" + spell + "Visual", save_path=module, hlsl=module_source(color,spell),
                  inputs=[{"name": "Age", "type": "float"}, {"name": "Index", "type": "int"}, {"name": "Mode", "type": "int"}], outputs=[],
                  description="原创开发魔法纯GPU外观；低频柔和核心/环绕/尾迹/命中，不读取伤害。")
        for stage in ("Flight", "Impact"):
            system = DEST + "/Niagara/NS_DBA_Dev" + spell + stage
            emitter_specs = []
            # 连续光带靠有限重叠软粒子，而不是大量随机闪点；三种技能各自保留清晰轮廓。
            modes = [("Core", 0, 1), ("Aura", 1, 28 if spell == "ArcaneMissiles" else 24), ("Trail", 2, 120)] if stage == "Flight" else [("Flash", 3, 1), ("Halo", 4, 32), ("Fragments", 5, 8)]
            for name, mode, count in modes:
                spawn_script = "/Niagara/Modules/Emitter/SpawnRate.SpawnRate" if name == "Trail" else "/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous"
                emitter_specs.append({"name": name, "asset": "/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal",
                    "properties": {"SimTarget": "GPUComputeSim", "bLocalSpace": "False" if name == "Trail" else "True"},
                    "modules": [{"stage": "emitter_update", "script": spawn_script, "inputs": {"SpawnRate" if name == "Trail" else "Spawn Count": str(count)}},
                                {"stage": "particle_spawn", "script": module, "bindings": {"Index": "Particles.UniqueID"}, "inputs": {"Mode": str(mode),"Age":"0"}},
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
                p.niagara("set_module_input_value", asset_path=system, emitter=name, module_node="InitializeParticle", input="Lifetime", value="0.5" if name == "Trail" else "3.0" if stage == "Flight" else "0.8")
                p.niagara("set_static_switch_value", asset_path=system, emitter=name, module_node="EmitterState", input="Life Cycle Mode", value="Self")
                p.niagara("set_emitter_loop_profile", asset_path=system, emitter=name, loop_behavior="Once", loop_duration=2.5 if stage == "Flight" else 0.3)
                # 世界空间尾迹最长1600厘米/秒×0.5秒，加螺旋/软光带余量；不能沿用只容纳核心的200厘米盒。
                # 采用全轴保守界限，旋转/螺旋时仍覆盖自身粒子，不假设世界X轴恒为运动方向。
                extent=950 if name == "Trail" else 200
                p.niagara("set_fixed_bounds", asset_path=system, emitter=name, min=[-extent]*3, max=[extent]*3, enabled=True)
            extent=950 if stage == "Flight" else 250
            p.niagara("set_fixed_bounds", asset_path=system, min=[-extent]*3, max=[extent]*3, enabled=True)
            diagnostics = p.niagara("get_system_diagnostics", asset_path=system, compile_first=True)
            if diagnostics.get("error_count", 1) != 0 or diagnostics.get("warning_count",1) != 0: raise RuntimeError("Niagara编译/材质警告: " + system + " " + json.dumps(diagnostics))
            p.niagara("save_system", asset_path=system, only_if_dirty=False)
            results.append({"Asset": system, "Diagnostics": diagnostics})
    p.editor_python("import unreal\nfor spell in ['Fireball','Frostbolt','ArcaneMissiles']:\n unreal.EditorAssetLibrary.save_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Niagara/NM_DBA_Dev'+spell+'Visual',False)")
    p.editor_python(definitions_source())
    output = ROOT / "Saved/Validation/MagicHUD20261012/VFXAuthoring/Systems.json"
    output.write_text(json.dumps({"Systems": results, "ManualVisualAccepted": False, "ServerCookVerified": False}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"CompiledSystems": len(results), "Evidence": str(output)}, ensure_ascii=False))


def refine_existing():
    """显式精修只重建本轮专用生成资产，并先保留原始磁盘备份，不删除用户场景或旧技能。
    Monolith 0.23.0重复绑定已有override会破坏输出图；以同包身份一次spec重建替代逐节点重绑。
    重建后仍须关闭编辑器重新编译/读取六系统，实时编译报告不能作为保存兼容证明。
    """
    import shutil
    from datetime import datetime,timezone
    protocol()  # 先核对工具/编辑器所有权，再做无损备份。
    source=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/DevelopmentMagic'
    backup=ROOT/'Saved/Validation/MagicHUD20261012'/('VFXBackup-'+datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f'))
    if source.is_dir(): shutil.copytree(source,backup)
    print('MAGIC_VFX_ROLLBACK',str(backup),flush=True)
    author()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__); parser.add_argument("--apply", action="store_true"); parser.add_argument('--refine',action='store_true'); args = parser.parse_args()
    if args.refine: refine_existing()
    elif args.apply: author()
    else: print(json.dumps({"Assets": [DEST + "/Niagara/NS_DBA_Dev" + name + stage for name in SPELLS for stage in ("Flight", "Impact")], "ActualAssetsModified": False}))
