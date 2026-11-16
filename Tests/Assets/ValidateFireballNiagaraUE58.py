# -*- coding: utf-8 -*-
"""标准火球接入只读原生门禁：检查真实保存依赖、唯一绑定与当前视觉合同。

仅由Monolith在SSLM UE5.8执行，不导入作者、不改资产、不替代实际粒子尺寸、
CPU/GPU编译、Cook、联机或人工视觉验证。先记录未接线的RED，再检查保存后的GREEN。
"""
import json
import os,hashlib
from pathlib import Path
import sys
import unreal as u

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools/Unreal/VFX/SpellProjectiles"))
from SpellPresentationContract import load_manifest


def validate(report_path=None, raise_on_error=True, native_graph_report=None):
    """返回真实原生错误集合；缺资产、旧绑定或尺寸错误都不能用跳过检查变绿。"""
    if Path(u.Paths.get_project_file_path()).resolve() != ROOT / "Game/DivineBeastsArena.uproject":
        raise RuntimeError("不是SSLM主工程")
    if not u.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("不是锁定UE5.8")
    base = "/DBAPresentationPack_Core/VFX/SpellProjectiles"
    material_root = base + "/Materials/Fireball/MI_DBA_Fireball_"
    stage_materials = {
        "Cast": ["Core", "Trail", "Ember", "AirRipple"],
        "Flight": ["CoreSurface", "Shell", "ShellSprite", "Trail", "AirRipple", "Filament", "Glow", "FlightEmber"],
        "Impact": ["Impact", "Ember", "AirRipple"],
    }
    registry = u.AssetRegistryHelpers.get_asset_registry()
    options = u.AssetRegistryDependencyOptions(include_hard_package_references=True,
                                              include_soft_package_references=True)
    errors, stages = [], []
    data = load_manifest()
    proof=json.loads(Path(native_graph_report).read_text('utf-8')) if native_graph_report else {}
    if proof.get('pid')!=os.getpid(): errors.append('缺少同进程真实执行图证据')
    content=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'

    for stage, materials in stage_materials.items():
        rows = [row for row in data["bindings"] if row["spell"] == "Fireball" and row["stage"] == stage]
        if len(rows) != 1:
            errors.append("当前火球阶段没有唯一合同：" + stage)
            continue
        row = rows[0]
        system, definition = u.load_asset(row["system"]), u.load_asset(row["definition"])
        if not isinstance(system, u.NiagaraSystem) or definition is None:
            errors.append("当前资产缺失：" + stage)
            continue
        reference = definition.get_editor_property("niagara_system")
        if not reference or reference.get_path_name().split(".")[0] != row["system"]:
            errors.append("Definition不再引用当前唯一系统：" + stage)
        dependencies = {str(p) for p in registry.get_dependencies(u.Name(row["system"]), options)}
        for name in materials:
            if material_root + name not in dependencies:
                errors.append(stage + "没有保存材质依赖：" + name)
        if base + "/Niagara/NM_DBA_FireballMaterialParameters" not in dependencies:
            errors.append(stage + "没有公共材质参数适配模块")
        # 编译通过不能证明Emitter已接进系统Spawn/Update图；本次真实缺接线回归。
        graph=proof.get('stages',{}).get(stage,{})
        asset=content/(row['system'].split('/',2)[2]+'.uasset')
        if graph.get('sha256')!=hashlib.sha256(asset.read_bytes()).hexdigest():errors.append(stage+'执行图证据SHA不匹配')
        nodes=[n for n in graph.get('graph',{}).get('nodes',[]) if n['class']=='NiagaraNodeEmitter']
        expected_nodes=14 if stage=="Flight" else 8
        if len(nodes) != expected_nodes:
            errors.append(stage + "系统执行图Emitter数量不符：" + str(len(nodes)))
        if stage == "Flight":
            renderer_class=u.load_class(None,"/Script/Niagara.NiagaraRibbonRendererProperties")
            renderers=[r for r in u.ObjectIterator(renderer_class)
                       if r.get_path_name().startswith(system.get_path_name()+":FireTrail_")]
            if len(renderers)!=1:
                errors.append("当前火球没有唯一Ribbon Renderer")
            else:
                # 该枚举未完整导出到UE Python；使用原生Struct导出文本而非PythonizeEnumEntry。
                actual=renderers[0].get_editor_property("UV0Settings").export_text()
                if "DistributionMode="+data["fireball_appearance"]["trail_uv_distribution"] not in actual:
                    errors.append("Ribbon UV必须整条归一化，不能平铺后被材质裁掉："+actual)
                uv=renderers[0].get_editor_property("UV0Settings")
                scale,offset=uv.get_editor_property("Scale"),uv.get_editor_property("Offset")
                cfg=data["fireball_appearance"]
                expected=(cfg["trail_uv_scale"][0],cfg["trail_uv_scale"][1],cfg["trail_u_offset"],cfg["trail_v_offset"])
                if any(abs(a-b)>.0001 for a,b in zip((scale.x,scale.y,offset.x,offset.y),expected)):
                    errors.append("Ribbon根部采样不符连接与等宽合同："+actual)
        stages.append({"stage": stage, "system": row["system"], "dependencies": sorted(dependencies)})
    cfg=data['fireball_appearance'];ML=u.MaterialEditingLibrary
    core=u.load_asset(material_root+'CoreSurface');parent=core.get_editor_property('parent')
    if parent.get_editor_property('blend_mode')!=u.BlendMode.BLEND_OPAQUE:errors.append('核心不是完整Opaque球面')
    if abs(ML.get_material_instance_scalar_parameter_value(core,'FireIntensity')-cfg['head_emission']*cfg['material_emission_scale'])>.001:errors.append('核心发光不符当前合同')
    mesh=u.load_asset(cfg['head_mesh'])
    if not isinstance(mesh,u.StaticMesh) or mesh.get_static_mesh_description(0).get_triangle_count()!=3968:errors.append('圆滑球体几何错误')
    for suffix,source in [('Trail','TrailBroad'),('Shell','ShellRing'),('Filament','TrailFilament')]:
        mi=u.load_asset(material_root+suffix);tex=ML.get_material_instance_texture_parameter_value(mi,'FlameTexture')
        if tex.get_path_name().split('.')[0]!=base+'/Textures/Fireball/T_DBA_Fireball_'+source:errors.append('残留错误纹理覆盖:'+suffix)
    for row in data['resources']:
        if '/Textures/Fireball/' in row['path']:
            t=u.load_asset(row['path'])
            if not isinstance(t,u.Texture2D) or t.get_editor_property('srgb')!=row['srgb']:errors.append('纹理颜色空间错误:'+row['path'])
    result = {"Passed": not errors, "Errors": errors, "Stages": stages,
              "PhysicalParticleSizeVerified": False, "CookVerified": False,
              "RuntimeVerified": False, "ManualVisualAccepted": False}
    if report_path:
        path = Path(report_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    if errors and raise_on_error:
        raise RuntimeError("火球原生接入门禁失败：" + "; ".join(errors))
    return result


if __name__ == "__main__":
    print(json.dumps(validate(ROOT / "Saved/Validation/FireballNiagara20261031/NativeGate.json"), ensure_ascii=False))
