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

    def fix_module_contract(self, module=MODULE):
        """InputMap按原生必需/曝光合同保存，防止热编译正常而冷重载断开调用引脚。"""
        self.python(f"""import unreal as u
a=u.load_asset({module!r})
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
        cfg=self.manifest['fireball_appearance']
        hlsl='\n'.join('#define '+name+' '+str(cfg[key]) for name,key in [
            ('DBA_FB_TRAIL_LENGTH','trail_length_cm'),('DBA_FB_TRAIL_WIDTH','trail_root_width_cm'),
            ('DBA_FB_FILAMENT_WIDTH','filament_width_cm'),('DBA_FB_TRAIL_MAX_LIFE','trail_max_lifetime_seconds'),
            ('DBA_FB_EMISSION_SCALE','material_emission_scale')])+'\n'+hlsl
        # 共享模块的三阶段声明相同中立参数；真正运行绑定仅发生在Projectile实例。
        motion=json.loads((ROOT/'Tools/Unreal/Combat/ProjectileFlightProfiles.json').read_text('utf-8'))
        for stage in ('Cast','Flight','Impact'):
            for parameter in motion['niagara_parameters']:
                self.niagara('add_user_parameter',asset_path=self.system(stage),**parameter)
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
        if phase == 1 and role not in (2,4):
            # 球头、外围火焰与扰动共享一个直径真源；Cast/Impact不消费飞行尺寸。
            cfg=self.manifest["fireball_appearance"]
            key="shell_diameter_cm" if role==1 else ("glow_diameter_cm" if role==5 else "head_diameter_cm")
            scale = cfg[key] / 100.0
            values["InScale"] = f"(X={scale},Y={scale},Z={scale})"
        elif is_ribbon:
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
        """材质写入归唯一作者；本协调器只派发，不复制图构建或颜色规则。"""
        source=Path(__file__).with_name("AuthorFireballMaterialsUE58.py")
        self.python("import runpy\nprint(runpy.run_path("+repr(str(source))+")[\"author_flight_materials\"]("+repr(self.manifest["fireball_appearance"])+"))")

    def configure_flight_trail(self):
        """世界历史Ribbon整条归一化；旧每米平铺UV会被0-1边缘遮罩裁掉。"""
        path, cfg = self.system("Flight"), self.manifest["fireball_appearance"]
        for usage, suffix, inputs in [
            ("particle_spawn", "/NM_DBA_TrailLinkSpawn", {"WidthCm": cfg["trail_root_width_cm"]}),
            ("particle_update", "/NM_DBA_TrailFade", {"WidthCm": cfg["trail_root_width_cm"]})]:
            raw = self.niagara("get_ordered_modules", asset_path=path, emitter="FireTrail", usage=usage)
            nodes = [n for n in json.loads(raw["result"]) if n.get("script_path", "").split(".")[0].endswith(suffix)]
            if len(nodes) != 1:
                raise RuntimeError("当前拖尾模块不唯一：" + suffix)
            for key, value in inputs.items():
                self.niagara("set_module_input_value", asset_path=path, emitter="FireTrail",
                             module_node=nodes[0]["node_guid"], input=key, value=str(value))
            if suffix.endswith("/NM_DBA_TrailFade"):
                # 淡出必须消费实际粒子寿命，不能仍按历史0.22秒裁断延长后的尾部。
                self.niagara("set_module_input_binding", asset_path=path, emitter="FireTrail",
                             module_node=nodes[0]["node_guid"], input="Life", binding="Particles.Lifetime")
        self.niagara("set_renderer_property", asset_path=path, emitter="FireTrail", renderer_index=0,
                     property="UV0Settings", value="(DistributionMode=" + cfg["trail_uv_distribution"] +
                     f",LeadingEdgeMode=Locked,TrailingEdgeMode=Locked,TilingLength=100,Offset=(X={cfg['trail_u_offset']},Y={cfg['trail_v_offset']}),Scale=(X={cfg['trail_uv_scale'][0]},Y={cfg['trail_uv_scale'][1]}))")
        self.python(f"""import unreal as u
mi=u.load_asset({(MATERIALS+'Trail')!r})
if abs(u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi,'Emission')-{cfg['trail_emission']*cfg['material_emission_scale']!r})>.0001:
 u.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi,'Emission',{cfg['trail_emission']*cfg['material_emission_scale']!r})
 assert u.EditorAssetLibrary.save_loaded_asset(mi,False)
assert u.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(mi,'Emission')=={cfg['trail_emission']*cfg['material_emission_scale']!r}
""")
        # 世界历史路径可能向任意方向转弯；System有界包络包含最大50m/s的0.6秒历史。
        extent = cfg["fixed_bounds_extent_cm"]
        self.niagara("set_fixed_bounds", asset_path=path,
                     min=[-extent, -extent, -extent], max=[extent, extent, extent])

    def configure_secondary_layers(self):
        """同一Flight原位补齐三丝带、柔光、少量世界火星；旧四层身份保留。"""
        path,cfg=self.system("Flight"),self.manifest["fireball_appearance"]
        spawn=BASE+"/Niagara/NM_DBA_FireballSecondarySpawn"
        source=Path(__file__).with_name("FireballSecondarySpawn.hlsl").read_text("utf-8")
        source="#define FIREBALL_FILAMENT_COUNT "+str(cfg["filament_count"])+"\n#define FIREBALL_FILAMENT_PHASE "+str(cfg["filament_phase_radians_per_second"])+"\n"+source
        exists=self.python("import unreal as u\nprint('SECONDARY_EXISTS',u.EditorAssetLibrary.does_asset_exist("+repr(spawn)+"))")
        if "SECONDARY_EXISTS True" in json.dumps(exists):
            self.niagara("set_custom_hlsl_text",script_path=spawn,hlsl=source)
        else:
            fields=[("InPosition","position"),("Index","int"),("Clock","float"),("AxisY","vec3"),("AxisZ","vec3"),("Mode","int"),("RadiusCm","float"),("WidthCm","float")]
            self.niagara("create_module_from_hlsl",name="FireballSecondarySpawn",save_path=spawn,hlsl=source,
                         inputs=[{"name":n,"type":t} for n,t in fields],outputs=[],description="火球世界空间丝带分组与少量火星出生，不拥有权威运动")
        self.niagara("save_system",asset_path=spawn);self.fix_module_contract(spawn)
        rows=self.niagara("export_system_spec",asset_path=path)["spec"]["emitters"]
        for name,original in [("Filaments","FireTrail"),("Glow","Core"),("Embers","FireTrail")]:
            if not any(e["name"]==name for e in rows):
                source_row=next(e for e in rows if e["name"]==original)
                self.niagara("add_emitter",asset_path=path,emitter_asset=source_row["asset"],name=name)
        def modules(name,usage):
            return json.loads(self.niagara("get_ordered_modules",asset_path=path,emitter=name,usage=usage)["result"])
        def ensure(name,usage,script):
            found=[m for m in modules(name,usage) if m.get("script_path","").split(".")[0]==script]
            if len(found)>1:raise RuntimeError("重复模块："+name+script)
            if found:return found[0]["node_guid"]
            ordered=modules(name,usage)
            index=next((i for i,m in enumerate(ordered) if m.get("script_path","").split(".")[0]==MODULE),len(ordered))
            return self.niagara("add_module",asset_path=path,emitter=name,usage=usage,module_script=script,index=index)["node_guid"]
        def value(name,node,key,val):
            self.niagara("set_module_input_value",asset_path=path,emitter=name,module_node=node,input=key,value=str(val))
        def binding(name,node,key,attribute):
            self.niagara("set_module_input_binding",asset_path=path,emitter=name,module_node=node,input=key,binding=attribute)
        # 单层圆球Mesh包裹内核，避免多张大Sprite叠成不规则星形轮廓。
        if "NiagaraMeshRendererProperties" not in json.dumps(self.niagara("list_renderers",asset_path=path,emitter="SurfaceFlames")):
            self.niagara("remove_renderer",asset_path=path,emitter="SurfaceFlames",renderer_index=0)
            self.niagara("add_renderer",**{"asset_path":path,"emitter":"SurfaceFlames","class":"Mesh"})
        self.niagara("set_renderer_mesh",asset_path=path,emitter="SurfaceFlames",renderer_index=0,mesh=cfg["head_mesh"],mesh_index=0,scale={"x":1,"y":1,"z":1})
        if 'NiagaraSpriteRendererProperties' not in json.dumps(self.niagara('list_renderers',asset_path=path,emitter='SurfaceFlames')):
            self.niagara('add_renderer',**{'asset_path':path,'emitter':'SurfaceFlames','class':'Sprite'})
        self.niagara('set_renderer_material',asset_path=path,emitter='SurfaceFlames',renderer_index=1,material=MATERIALS+'ShellSprite')
        for m in modules("SurfaceFlames","emitter_update"):
            if "/SpawnBurst_Instantaneous." in m.get("script_path",""):value("SurfaceFlames",m["node_guid"],"Spawn Count",1)
        # 保留模板已连接的更新栈；新模块插入适配前，重复执行只更新原节点。
        if "NiagaraMeshRendererProperties" in json.dumps(self.niagara("list_renderers",asset_path=path,emitter="Glow")):
            self.niagara("remove_renderer",asset_path=path,emitter="Glow",renderer_index=0)
            self.niagara("add_renderer",**{"asset_path":path,"emitter":"Glow","class":"Sprite"})
        self.niagara("set_emitter_property",asset_path=path,emitter="Embers",property="bLocalSpace",value="False")
        if "NiagaraRibbonRendererProperties" in json.dumps(self.niagara("list_renderers",asset_path=path,emitter="Embers")):
            self.niagara("remove_renderer",asset_path=path,emitter="Embers",renderer_index=0)
            self.niagara("add_renderer",**{"asset_path":path,"emitter":"Embers","class":"Sprite"})
        for m in modules("Embers","emitter_update"):
            if "/SpawnBurst_Instantaneous." in m.get("script_path",""):
                value("Embers",m["node_guid"],"Spawn Count",0)
        for name,rate,life,width,radius,mode in [("Filaments",0.0,.1,cfg["filament_width_cm"],cfg["filament_radius_cm"],0),
                                                ("Embers",cfg["ember_spawn_rate"],cfg["ember_lifetime_seconds"],1,12,1)]:
            node=ensure(name,"emitter_update","/Niagara/Modules/Emitter/SpawnRate");value(name,node,"SpawnRate",rate)
            node=ensure(name,"particle_spawn","/Niagara/Modules/Spawn/Initialization/V2/InitializeParticle");value(name,node,"Lifetime",life)
            if name=="Filaments":
                node=ensure(name,"particle_update",BASE+"/Niagara/NM_DBA_TrailFade")
                value(name,node,"WidthCm",width);binding(name,node,"Life","Particles.Lifetime")
            node=ensure(name,"particle_spawn",spawn)
            for key,attribute in [("InPosition","Particles.Position"),("Index","Particles.UniqueID"),("Clock","Emitter.Age"),("AxisY","Engine.Owner.SystemYAxis"),("AxisZ","Engine.Owner.SystemZAxis")]:
                binding(name,node,key,attribute)
            for key,val in [("Mode",mode),("RadiusCm",radius),("WidthCm",width)]:value(name,node,key,val)
        ensure("Embers","particle_update","/Niagara/Modules/Solvers/SolveForcesAndVelocity")
        # 原生默认RibbonID的类型来自当前进程，不硬编码RegisteredTypeIndex。
        probe=self.python("""import unreal as u,re,json
r=u.new_object(u.load_class(None,'/Script/Niagara.NiagaraRibbonRendererProperties'))
t=r.get_editor_property('RibbonIdBinding').export_text()
typ=re.search(r'ParamMapVariable=.*?RegisteredTypeIndex=(\\d+)',t).group(1)
t=re.sub(r'BoundVariable=\\(VarData=,Name="[^"]*",TypeDefHandle=\\(RegisteredTypeIndex=\\d+\\)\\)',
         'BoundVariable=(VarData=,Name="Particles.RibbonID",TypeDefHandle=(RegisteredTypeIndex='+typ+'))',t)
t=t.replace('bBindingExistsOnSource=False','bBindingExistsOnSource=True')
print('TYPED_RIBBON_BINDING:'+json.dumps(t))
""")
        line=next(x['output'] for x in probe['output'] if 'TYPED_RIBBON_BINDING:' in x.get('output',''))
        typed=json.loads(line.split('TYPED_RIBBON_BINDING:',1)[1].strip())
        self.niagara('set_renderer_property',asset_path=path,emitter='Filaments',renderer_index=0,property='RibbonIdBinding',value=typed)
        self.niagara("set_renderer_property",asset_path=path,emitter="Filaments",renderer_index=0,property="UV0Settings",
                     value="(DistributionMode=ScaledUsingRibbonSegmentLength,LeadingEdgeMode=Locked,TrailingEdgeMode=Locked,Offset=(X=0,Y=0),Scale=(X=1,Y=1))")
        self.niagara("reorder_emitters",asset_path=path,order=["Core","SurfaceFlames","FireTrail","Filaments","Glow","Embers","AirRipple"])

    def configure_flight_motion(self):
        """原生按距离生成历史点；实际Owner速度只决定表现寿命，不改变服务器运动。"""
        path,cfg=self.system('Flight'),self.manifest['fireball_appearance']
        module=BASE+'/Niagara/NM_DBA_FireballTrailLifetime'
        # 兼容输入保留；不再用出生帧瞬时Owner速度决定整条尾迹寿命，避免网络阶跃形成8米长尾。
        code='Particles.Lifetime='+str(cfg['trail_max_lifetime_seconds'])+'; Particles.DynamicMaterialParameter=float4(0,0,-1,1);'
        probe=self.python('import unreal as u\nprint("EXISTS",u.EditorAssetLibrary.does_asset_exist('+repr(module)+'))')
        if 'EXISTS True' in json.dumps(probe):self.niagara('set_custom_hlsl_text',script_path=module,hlsl=code)
        else:self.niagara('create_module_from_hlsl',name='FireballTrailLifetime',save_path=module,hlsl=code,
                          inputs=[{'name':'Velocity','type':'vec3'},{'name':'LengthCm','type':'float'}],outputs=[])
        self.fix_module_contract(module);self.niagara('save_system',asset_path=module)
        for name in ('FireTrail','Filaments'):
            updates=json.loads(self.niagara('get_ordered_modules',asset_path=path,emitter=name,usage='particle_update')['result'])
            for old in updates:
                if old.get('script_path','').split('.')[0]==BASE+'/Niagara/NM_DBA_TrailFade':
                    self.niagara('set_module_enabled',asset_path=path,emitter=name,module_node=old['node_guid'],enabled=False)
            for usage,script in [('emitter_update','/Niagara/Modules/Emitter/SpawnPerUnit'),('particle_spawn',module)]:
                nodes=json.loads(self.niagara('get_ordered_modules',asset_path=path,emitter=name,usage=usage)['result'])
                found=[n for n in nodes if n.get('script_path','').split('.')[0]==script]
                assert len(found)<=1
                node=found[0] if found else self.niagara('add_module',asset_path=path,emitter=name,usage=usage,module_script=script)
                guid=node['node_guid']
                if usage=='particle_spawn':
                    self.niagara('set_module_input_binding',asset_path=path,emitter=name,module_node=guid,input='Velocity',binding='Engine.Owner.Velocity')
                    self.niagara('set_module_input_value',asset_path=path,emitter=name,module_node=guid,input='LengthCm',value=str(cfg['trail_length_cm']))
                else:
                    inputs=self.niagara('get_module_inputs',asset_path=path,emitter=name,module_node=guid)
                    (OUT/(name+'.SpawnPerUnit.inputs.json')).write_text(json.dumps({'guid':guid,'inputs':inputs},indent=2),'utf-8')
                    spacing=20.0 if name=='FireTrail' else 25.0/cfg['filament_count']
                    for key,val in [('Spawn Spacing',spacing),('Use Movement Tolerance','false'),('Use Max Movement Threshold','false')]:
                        self.niagara('set_module_input_value',asset_path=path,emitter=name,module_node=guid,input=key,value=str(val))
                    self.niagara('set_module_input_binding',asset_path=path,emitter=name,module_node=guid,input='Velocity Vector',binding='Engine.Owner.Velocity')
                    for n in nodes:
                        if n.get('script_path','').split('.')[0]=='/Niagara/Modules/Emitter/SpawnRate':
                            self.niagara('set_module_input_value',asset_path=path,emitter=name,module_node=n['node_guid'],input='SpawnRate',value='0')
        self.niagara('set_renderer_mesh',asset_path=path,emitter='Core',renderer_index=0,mesh=cfg['head_mesh'],mesh_index=0,scale={'x':1,'y':1,'z':1})
        system_nodes=json.loads(self.niagara('get_ordered_modules',asset_path=path,emitter='',usage='system_update')['result'])
        system_state=next(n for n in system_nodes if n.get('script_path','').split('.')[0].endswith('/SystemState'))
        self.niagara('set_static_switch_value',asset_path=path,emitter='',module_node=system_state['node_guid'],input='Inactive Response',value='NewEnumerator0')
        persistent=('Core','SurfaceFlames','Glow','AirRipple')
        for name in ('Core','SurfaceFlames','FireTrail','Filaments','Glow','Embers','AirRipple'):
            nodes=json.loads(self.niagara('get_ordered_modules',asset_path=path,emitter=name,usage='emitter_update')['result'])
            state=next(n for n in nodes if n.get('script_path','').split('.')[0].endswith('/EmitterState'))
            if nodes[0]['node_guid']!=state['node_guid']:
                self.niagara('move_module',asset_path=path,emitter=name,module_node=state['node_guid'],new_index=0)
            self.niagara('set_static_switch_value',asset_path=path,emitter=name,module_node=state['node_guid'],input='Life Cycle Mode',value='Self')
            for n in nodes:
                script=n.get('script_path','').split('.')[0]
                if script.endswith('/SpawnBurst_Instantaneous') and name in persistent:
                    for key,val in [('Spawn Count','1'),('Loop Count Limit','1')]:
                        self.niagara('set_module_input_value',asset_path=path,emitter=name,module_node=n['node_guid'],input=key,value=val)
                    self.niagara('set_static_switch_value',asset_path=path,emitter=name,module_node=n['node_guid'],input='UseLoopCountLimit',value='true')
                    self.niagara('set_module_input_binding',asset_path=path,emitter=name,module_node=n['node_guid'],input='Age',binding='Emitter.Age')
                if script.endswith('/EmitterState'):
                    self.niagara('set_static_switch_value',asset_path=path,emitter=name,module_node=n['node_guid'],input='Inactive Response',value='NewEnumerator1' if name in persistent else 'NewEnumerator0')
            if name not in persistent:continue
            spawn_nodes=json.loads(self.niagara('get_ordered_modules',asset_path=path,emitter=name,usage='particle_spawn')['result'])
            init=next(n for n in spawn_nodes if n.get('script_path','').split('.')[0].endswith('/InitializeParticle'))
            self.niagara('set_module_input_value',asset_path=path,emitter=name,module_node=init['node_guid'],input='Lifetime',value='10000')
        self.niagara('set_emitter_property',asset_path=path,emitter='Glow',property='SimTarget',value='CPUSim')
        if 'NiagaraLightRendererProperties' not in json.dumps(self.niagara('list_renderers',asset_path=path,emitter='Glow')):
            self.niagara('add_renderer',**{'asset_path':path,'emitter':'Glow','class':'Light'})
        self.python(f"""import unreal as u
s=u.load_asset({path!r});cls=u.load_class(None,'/Script/Niagara.NiagaraLightRendererProperties')
lights=[r for r in u.ObjectIterator(cls) if r.get_path_name().startswith(s.get_path_name()+':Glow_')]
assert len(lights)==1
r=lights[0];r.set_editor_property('bAlphaScalesBrightness',False)
r.set_editor_property('bAffectsTranslucency',False);r.set_editor_property('RadiusScale',1.)
r.set_editor_property('bUseInverseSquaredFalloff',False);r.set_editor_property('DefaultExponent',2.)
r.set_editor_property('bOverrideInverseExposureBlend',False);r.set_editor_property('InverseExposureBlend',0.)
r.set_editor_property('ColorAdd',type(r.get_editor_property('ColorAdd'))(60.,35.,3.))
platform=r.get_editor_property('platforms');platform.set_editor_property('quality_level_mask',28);r.set_editor_property('platforms',platform)
for class_name,emitter in [('NiagaraRibbonRendererProperties','Filaments'),('NiagaraSpriteRendererProperties','Embers')]:
 for rr in u.ObjectIterator(u.load_class(None,'/Script/Niagara.'+class_name)):
  if rr.get_path_name().startswith(s.get_path_name()+':'+emitter+'_'):
   pp=rr.get_editor_property('platforms');pp.set_editor_property('quality_level_mask',30);rr.set_editor_property('platforms',pp)
assert u.EditorAssetLibrary.save_loaded_asset(s,False)
""")
        self.niagara('request_compile',asset_path=path);self.niagara('save_system',asset_path=path)

    def verify_native(self):
        """Graph.GetNodes证据绑定当前进程与保存包SHA，再调用独立只读门禁。"""
        import hashlib
        proof={'pid':self.pid,'stages':{}}
        content=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content'
        for stage in ('Cast','Flight','Impact'):
            path=self.system(stage)
            diagnostics=self.niagara('get_system_diagnostics',asset_path=path,compile_first=False)
            assert diagnostics['error_count']==0 and diagnostics['warning_count']==0,diagnostics
            assert all(script['compile_status']=='UpToDate' for emitter in diagnostics['emitter_stats'] for script in emitter['scripts']),diagnostics
            graph=self.niagara('get_module_graph',script_path=path+'.'+path.rsplit('/',1)[1]+':SystemSpawnScript',links=True)
            asset=content/(path.split('/',2)[2]+'.uasset')
            proof['stages'][stage]={'graph':graph,'sha256':hashlib.sha256(asset.read_bytes()).hexdigest(),'diagnostics':diagnostics}
        file=OUT/'Native.SystemGraphs.json';file.write_text(json.dumps(proof,ensure_ascii=False,indent=2),'utf-8')
        self.python('import runpy\nprint(runpy.run_path('+repr(str(ROOT/'Tests/Assets/ValidateFireballNiagaraUE58.py'))+')["validate"]('+repr(str(OUT/'Native.GREEN.json'))+',native_graph_report='+repr(str(file))+'))')

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
        self.configure_secondary_layers()
        mapping = {
            "Cast": [("E_FireCore", "Core", False, False), ("E_FireCurlBody", "Trail", False, False),
                     ("E_FireDetails", "Ember", True, False), ("E_FireAirRipple", "AirRipple", False, False)],
            "Flight": [("Core", "CoreSurface", False, False), ("SurfaceFlames", "Shell", False, False),
                       ("FireTrail", "Trail", False, True), ("AirRipple", "AirRipple", False, False),
                       ("Filaments", "Filament", False, True),("Glow","Glow",True,False),("Embers","FlightEmber",True,False)],
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
        self.configure_flight_trail()
        self.configure_flight_motion()
        self.sync_cook_label()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="标准火球三阶段原位接入作者")
    parser.add_argument("--port", type=int, default=9316)
    parser.add_argument("--editor-pid", type=int, required=True)
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    Author(args.port, args.editor_pid).apply()
