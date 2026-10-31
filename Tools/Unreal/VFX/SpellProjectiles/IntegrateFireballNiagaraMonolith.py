# -*- coding: utf-8 -*-
"""标准火球原位接入作者，通过本机Monolith MCP串行操作锁定UE5.8。

读取唯一PresentationManifest，不创建第二套系统或播放器。只修改三项当前火球、
本轮公共参数模块、已备份的火球体积母材质及本轮材质实例；不操作后端或账号。
调用日志、阶段结果和所有权保存在工程Saved，失败原样抛出，不放宽原生门禁。
"""
from pathlib import Path
import argparse
import json
import time
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parents[4]
OUT = ROOT / "Saved/Validation/FireballNiagara20261031"
BASE = "/DBAPresentationPack_Core/VFX/SpellProjectiles"
MODULE = BASE + "/Niagara/NM_DBA_FireballMaterialParameters"
MATERIALS = BASE + "/Materials/Fireball/MI_DBA_Fireball_"


class Author:
    """一个显式编辑器的串行MCP会话；不记录凭据、不切换到其它服务。"""
    def __init__(self, port, pid):
        self.port, self.pid = port, pid
        self.manifest = json.loads(Path(__file__).with_name("PresentationManifest.json").read_text(encoding="utf-8"))

    def call(self, tool, arguments):
        """严格检查JSON-RPC及动作层失败；逐次记录已执行动作，禁止假定批量全成功。"""
        request = {"jsonrpc": "2.0", "id": 1, "method": "tools/call",
                   "params": {"name": tool, "arguments": arguments}}
        body = json.dumps(request, ensure_ascii=False).encode("utf-8")
        with urlopen(Request(f"http://127.0.0.1:{self.port}/mcp", body,
                             {"Content-Type": "application/json"}), timeout=120) as response:
            envelope = json.load(response)
        if envelope.get("error"):
            raise RuntimeError(envelope["error"])
        result = envelope.get("result", {})
        if result.get("isError"):
            raise RuntimeError(result)
        value = json.loads(next(c["text"] for c in result.get("content", []) if c.get("type") == "text"))
        with (OUT / "Actions.jsonl").open("a", encoding="utf-8") as log:
            log.write(json.dumps({"time": time.time(), "tool": tool, "arguments": arguments,
                                  "result": value}, ensure_ascii=False) + "\n")
        if value.get("success") is False or value.get("ok") is False or value.get("error"):
            raise RuntimeError(value)
        return value

    def niagara(self, action, **params):
        """全部粒子图、Renderer、编译和保存动作仍由Monolith原生执行。"""
        return self.call("niagara_query", {"action": action, "params": params})

    def python(self, source):
        """引擎反射补充在同一个受控编辑器执行，不写假资产。"""
        return self.call("editor_query", {"action": "run_python", "params": {
            "command": source, "unattended": True}})

    def system(self, stage):
        """从唯一合同解析三阶段当前身份，没有旧Review或名称排序回退。"""
        rows = [r for r in self.manifest["bindings"] if r["spell"] == "Fireball" and r["stage"] == stage]
        if len(rows) != 1:
            raise RuntimeError("阶段不是唯一绑定：" + stage)
        return rows[0]["system"]

    def guard(self):
        """核对PID/工程/引擎及脏包；其它任务资源一旦变脏即停止。"""
        self.python(f"""import unreal as u,os
from pathlib import Path
assert os.getpid()=={self.pid},'Monolith编辑器PID不符'
assert u.SystemLibrary.get_engine_version().startswith('5.8.')
assert '-SSLMFireballMaterialsOwned' in u.SystemLibrary.get_command_line()
assert Path(u.Paths.get_project_file_path()).resolve()==Path({str(ROOT / 'Game/DivineBeastsArena.uproject')!r})
allowed={BASE!r}
dirty=[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
assert all(p.startswith(allowed+'/') for p in dirty),str(dirty)
assert not u.EditorLoadingAndSavingUtils.get_dirty_map_packages()
print('FIREBALL_EDITOR_OWNED',os.getpid())
""")

    def fix_module_contract(self):
        """InputMap按原生必需/曝光合同保存，防止热编译正常而冷重载断开调用引脚。"""
        self.python(f"""import unreal as u
a=u.load_asset({MODULE!r})
assert a
cls=u.load_class(None,'/Script/NiagaraEditor.NiagaraNodeInput')
nodes=[n for n in u.ObjectIterator(cls) if n.get_path_name().startswith(a.get_path_name()+':')]
assert len(nodes)==1,len(nodes)
n=nodes[0]
# bCanAutoBind受Python保护；用UE原生结构构造器恢复完整默认合同。
# 锁定NiagaraNodeInput.h构造器明确为Exposed/Required=true、CanAutoBind/Hidden=false。
v=type(n.get_editor_property('exposure_options'))()
assert v.get_editor_property('bExposed') and v.get_editor_property('bRequired') and not v.get_editor_property('bHidden')
n.set_editor_property('exposure_options',v)
assert u.EditorAssetLibrary.save_loaded_asset(a,False)
print('FIREBALL_INPUTMAP_SAVED')
""")

    def sync_module_source(self):
        """唯一HLSL文件生成或更新同一模块身份；不创建版本化候选。"""
        hlsl = Path(__file__).with_name("FireballMaterialParameters.hlsl").read_text("utf-8")
        probe = self.python("import unreal as u\nprint('FIREBALL_MODULE_EXISTS',u.EditorAssetLibrary.does_asset_exist(" + repr(MODULE) + "))")
        exists = "FIREBALL_MODULE_EXISTS True" in json.dumps(probe)
        if exists:
            self.niagara("set_custom_hlsl_text", script_path=MODULE, hlsl=hlsl)
        else:
            inputs = [("Age", "float"), ("InColor", "color"), ("InPosition", "position"),
                      ("InScale", "vec3"), ("InSpriteSize", "vec2"), ("InRotation", "float"),
                      ("ParticleIndex", "int"), ("Phase", "int"), ("Role", "int"),
                      ("Quality", "int"), ("Elapsed", "float")]
            self.niagara("create_module_from_hlsl", name="FireballMaterialParameters", save_path=MODULE,
                         hlsl=hlsl, inputs=[{"name": n, "type": t} for n, t in inputs], outputs=[],
                         description="标准火球三阶段材质参数与尺寸适配，不拥有权威弹道或结算")
        self.niagara("save_system", asset_path=MODULE)

    def sync_cook_label(self):
        """只更新唯一标签，种子从当前映射合同派生，不改Definition或其他法术。"""
        self.python(f"""import unreal as u,sys
sys.path.insert(0,{str(Path(__file__).resolve().parent)!r})
from SpellPresentationContract import load_manifest,required_packages
data=load_manifest();label=u.load_asset(data['cook_label'])
assert isinstance(label,u.PrimaryAssetLabel)
assets=[u.load_asset(p) for p in required_packages(data) if p!=data['cook_label']]
assert all(assets),'Cook种子缺失'
label.set_editor_property('explicit_assets',assets)
assert u.EditorAssetLibrary.save_loaded_asset(label,False)
""")

    def add_adapter(self, stage, emitter, phase, role, is_sprite=False, is_ribbon=False):
        """把适配放在既有运动之后；Ribbon仅传递世界位置，不生成平行弹道。"""
        path = self.system(stage)
        modules = self.niagara("get_ordered_modules", asset_path=path, emitter=emitter, usage="particle_update")
        ordered = json.loads(modules["result"])
        current = [m for m in ordered if m.get("script_path", "").split(".")[0] == MODULE]
        if len(current) > 1:
            raise RuntimeError("重复适配模块：" + stage + "/" + emitter)
        if current and current[0]["node_guid"] != ordered[-1]["node_guid"]:
            raise RuntimeError("适配模块不在既有运动之后：" + stage + "/" + emitter)
        # 重跑沿用唯一节点，禁止复制适配；新节点由引擎接入现有更新链末尾。
        created = current[0] if current else self.niagara(
            "add_module", asset_path=path, emitter=emitter,
            usage="particle_update", module_script=MODULE)
        node = created.get("node_guid") or created.get("module_guid") or created.get("module_node") or created.get("node_name")
        if not node:
            raise RuntimeError("新增模块未返回可定位节点：" + str(created))
        bindings = {"Age": "Particles.Age", "InColor": "Particles.Color", "InPosition": "Particles.Position",
                    "ParticleIndex": "Particles.UniqueID", "Quality": "Engine.QualityLevel"}
        values = {"Phase": str(phase), "Role": str(role)}
        if is_ribbon:
            values["InScale"] = "(X=1,Y=1,Z=1)"
        else:
            bindings["InScale"] = "Particles.Scale"
        if is_sprite:
            bindings.update({"InSpriteSize": "Particles.SpriteSize", "InRotation": "Particles.SpriteRotation"})
        else:
            values.update({"InSpriteSize": "(X=1,Y=1)", "InRotation": "0"})
        if phase == 0:
            bindings["Elapsed"] = "User.ElapsedAtStartSec"
        else:
            values["Elapsed"] = "0"
        for key, binding in bindings.items():
            self.niagara("set_module_input_binding", asset_path=path, emitter=emitter,
                         module_node=node, input=key, binding=binding)
        for key, value in values.items():
            self.niagara("set_module_input_value", asset_path=path, emitter=emitter,
                         module_node=node, input=key, value=value)
        self.niagara("set_renderer_binding", asset_path=path, emitter=emitter, renderer_index=0,
                     binding_name="DynamicMaterialBinding", attribute="Particles.DynamicMaterialParameter")
        if is_sprite:
            self.niagara("set_renderer_property", asset_path=path, emitter=emitter, renderer_index=0,
                         property="SubImageSize", value="(X=1,Y=1)")
        print("FIREBALL_ADAPTER", stage, emitter, node, flush=True)

    def material(self, stage, emitter, name):
        """Renderer全部引用本轮已存在实例，不复制同功能材质。"""
        self.niagara("set_renderer_material", asset_path=self.system(stage), emitter=emitter,
                     renderer_index=0, material=MATERIALS + name)

    def add_air(self, stage, source, name):
        """热扰动在同一System中拥有独立粒子生命周期；不增加Niagara System或播放器。"""
        path = self.system(stage)
        emitters = self.niagara("export_system_spec", asset_path=path)
        rows = emitters["spec"]["emitters"]
        matches = [e for e in rows if e.get("name") == name]
        if len(matches) > 1:
            raise RuntimeError("热扰动Emitter重复：" + stage)
        if not matches:
            sources = [e for e in rows if e.get("name") == source]
            if len(sources) != 1:
                raise RuntimeError("热扰动复制来源不唯一：" + stage)
            # duplicate_emitter只复制Handle，不重建系统执行图；不能用于可运行接入。
            # add_emitter原生完整流程解除嵌入父链并调用RebuildEmitterNodes。
            asset = sources[0].get("asset_path") or sources[0].get("asset")
            if not asset:
                raise RuntimeError("来源Emitter缺少原生对象路径")
            self.niagara("add_emitter", asset_path=path, emitter_asset=asset, name=name)
        renderers = self.niagara("list_renderers", asset_path=path, emitter=name)
        if stage == "Impact" and "NiagaraSpriteRendererProperties" in json.dumps(renderers):
            self.niagara("remove_renderer", asset_path=path, emitter=name, renderer_index=0)
            self.niagara("add_renderer", **{"asset_path": path, "emitter": name, "class": "Mesh"})
        self.niagara("set_renderer_mesh", asset_path=path, emitter=name, renderer_index=0,
                     mesh="/Engine/BasicShapes/Sphere", mesh_index=0, scale={"x": 1, "y": 1, "z": 1})
        self.material(stage, name, "AirRipple")

    def configure_air_quality(self):
        """Low原生Renderer关闭，其他档复用同一热扰动材质及适配模块。"""
        self.python(f"""import unreal as u
cls=u.load_class(None,'/Script/Niagara.NiagaraMeshRendererProperties')
for stage,name in [('Cast','E_FireAirRipple'),('Flight','AirRipple'),('Impact','AirRipple')]:
 s=u.load_asset({BASE!r}+'/Niagara/NS_DBA_Fireball'+stage)
 renderers=[r for r in u.ObjectIterator(cls) if r.get_path_name().startswith(s.get_path_name()+':'+name+'_')]
 assert len(renderers)==1,(stage,len(renderers))
 r=renderers[0];platforms=r.get_editor_property('platforms')
 platforms.set_editor_property('quality_level_mask',30)
 r.set_editor_property('platforms',platforms)
 assert u.EditorAssetLibrary.save_loaded_asset(s,False)
""")

    def configure_materials(self):
        """在既有体积母材质原位调成明黄色圆球，实例25cm；不产生第二种体积实现。"""
        self.python(f"""import unreal as u
m=u.load_asset({(BASE+'/Materials/M_DBA_FireOrbVolume')!r})
c=u.find_object(m,'MaterialExpressionCustom_0');code=c.get_editor_property('code')
pairs=[('float outerRadius=.80+.26*(slow-.5)+.055*(detail-.5);','float outerRadius=.97+.05*(slow-.5)+.02*(detail-.5);'),
       ('float3 gold=float3(5.4,1.10,.035);','float3 gold=float3(5.4,3.8,.12);'),
       ('float3 warmWhite=float3(17.0,9.5,2.5);','float3 warmWhite=float3(12.0,10.5,3.5);')]
for old,new in pairs:
 assert old in code or new in code,'体积源码发生非预期变化'
 code=code.replace(old,new)
c.set_editor_property('code',code)
u.MaterialEditingLibrary.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m,False)
instance=u.load_asset({(MATERIALS+'Volume')!r})
for key,value in [('RadiusCm',25.0),('EmissionScale',2.0)]:
 u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(instance,key,value)
 assert abs(u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(instance,key)-value)<.0001
assert u.EditorAssetLibrary.save_loaded_asset(instance,False)
print('FIREBALL_ROUND_YELLOW_50CM_MATERIAL_SAVED')
""")

    def configure_air_opacity(self):
        """共享扰动母材质增加默认关闭的Alpha输入，仅火球实例显式采用生命周期淡出。"""
        self.python(f"""import unreal as u
ML=u.MaterialEditingLibrary
m=u.load_asset({(BASE+'/Materials/M_DBA_PalmAirRipple')!r})
cls=u.load_class(None,'/Script/Engine.MaterialExpression')
nodes=[o for o in u.ObjectIterator(cls) if o.get_path_name().startswith(m.get_path_name()+':')]
opacity=[o for o in nodes if o.get_class().get_name()=='MaterialExpressionScalarParameter' and str(o.get_editor_property('parameter_name'))=='P_Opacity']
pc=[o for o in nodes if o.get_class().get_name()=='MaterialExpressionParticleColor']
assert len(opacity)==1 and pc
def node(kind,desc):
 found=[o for o in nodes if o.get_editor_property('desc')==desc]
 assert len(found)<=1,desc
 if found:return found[0]
 result=ML.create_material_expression(m,kind)
 result.set_editor_property('desc',desc)
 return result
option=node(u.MaterialExpressionScalarParameter,'SSLM_OptionalParticleAlpha')
option.set_editor_property('parameter_name','P_UseParticleAlpha')
option.set_editor_property('default_value',0.0)
blend=node(u.MaterialExpressionLinearInterpolate,'SSLM_AirAlphaSelection')
blend.set_editor_property('const_a',1.0)
multiply=node(u.MaterialExpressionMultiply,'SSLM_AirOpacityLifecycle')
assert ML.connect_material_expressions(pc[0],'A',blend,'B')
assert ML.connect_material_expressions(option,'',blend,'Alpha')
assert ML.connect_material_expressions(opacity[0],'',multiply,'A')
assert ML.connect_material_expressions(blend,'',multiply,'B')
assert ML.connect_material_property(multiply,'',u.MaterialProperty.MP_OPACITY)
ML.recompile_material(m)
assert u.EditorAssetLibrary.save_loaded_asset(m,False)
mi=u.load_asset({(MATERIALS+'AirRipple')!r})
ML.set_material_instance_scalar_parameter_value(mi,'P_UseParticleAlpha',1.0)
assert ML.get_material_instance_scalar_parameter_value(mi,'P_UseParticleAlpha')==1.0
assert u.EditorAssetLibrary.save_loaded_asset(mi,False)
assert ML.get_material_property_input_node(m,u.MaterialProperty.MP_OPACITY)==multiply
print('AIR_OPACITY_OPT_IN_SAVED; legacy default=0, Fireball=1')
""")

    def apply(self):
        """三个当前System原位接入，沿用现有唯一节点，最后显式保存。"""
        self.guard()
        self.sync_module_source()
        self.fix_module_contract()
        self.configure_materials()
        self.configure_air_opacity()
        self.add_air("Flight", "Core", "AirRipple")
        self.add_air("Impact", "Flash", "AirRipple")
        mapping = {
            "Cast": [("E_FireCore", "Core", False, False), ("E_FireCurlBody", "Trail", False, False),
                     ("E_FireDetails", "Ember", True, False), ("E_FireAirRipple", "AirRipple", False, False)],
            "Flight": [("Core", "Volume", False, False), ("SurfaceFlames", "Core", True, False),
                       ("FireTrail", "Trail", False, True), ("AirRipple", "AirRipple", False, False)],
            "Impact": [("Flash", "Impact", True, False), ("Halo", "Ember", True, False),
                       ("Fragments", "Ember", False, False), ("AirRipple", "AirRipple", False, False)],
        }
        for phase, (stage, emitters) in enumerate(mapping.items()):
            for role, (emitter, name, sprite, ribbon) in enumerate(emitters):
                self.material(stage, emitter, name)
                self.add_adapter(stage, emitter, phase, role, sprite, ribbon)
            self.niagara("request_compile", asset_path=self.system(stage))
            self.niagara("save_system", asset_path=self.system(stage))
            print("FIREBALL_STAGE_SAVED", stage, flush=True)
        self.configure_air_quality()
        self.sync_cook_label()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="标准火球三阶段原位接入作者")
    parser.add_argument("--port", type=int, default=9316)
    parser.add_argument("--editor-pid", type=int, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    Author(args.port, args.editor_pid).apply()
