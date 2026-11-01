# -*- coding: utf-8 -*-
"""SSLM标准火球纹理/材质作者，仅通过Monolith在锁定UE5.8主线程执行。

项目公共表现包拥有资源；本工具只写本轮新建的Fireball资源，不改Niagara、
Definition、Catalog和权威技能。源PNG保留原始字节，UE构建时缩放到1024。
失败保留原生诊断与已认领清单；再次执行只能重建本工具已认领的目标。
"""
from pathlib import Path
import hashlib
import json
import unreal as u

ROOT = Path(u.Paths.convert_relative_path_to_full(u.Paths.project_dir())).resolve().parent
PACK = ROOT / "Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core"
SOURCE = PACK / "SourceArt/VFX/SpellProjectiles/Fireball"
OUT = ROOT / "Saved/Validation/FireballMaterials20261031"
BASE = "/DBAPresentationPack_Core/VFX/SpellProjectiles"
TEXTURES = BASE + "/Textures/Fireball"
MATERIALS = BASE + "/Materials/Fireball"
EL, ML = u.EditorAssetLibrary, u.MaterialEditingLibrary
AT = u.AssetToolsHelpers.get_asset_tools()


def record(name, value):
    """写工程内证据；这些JSON不替代引擎资产、Cook或人工视觉签收。"""
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def check_context():
    """拒绝其它工程、引擎或共享脏包；不保存用户已有的修改。"""
    actual = Path(u.Paths.get_project_file_path()).resolve()
    if actual != ROOT / "Game/DivineBeastsArena.uproject":
        raise RuntimeError("不是SSLM主工程")
    if not u.SystemLibrary.get_engine_version().startswith("5.8."):
        raise RuntimeError("必须使用锁定UE5.8")
    if "-SSLMFireballMaterialsOwned" not in u.SystemLibrary.get_command_line():
        raise RuntimeError("不是本轮拥有的编辑器")
    dirty = [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
    own_refs={BASE+'/DA_DBA_SpellPresentationCook'}|{BASE+'/Niagara/NS_DBA_Fireball'+stage for stage in ['Cast','Flight','Impact']}
    if any(not p.startswith((TEXTURES + "/", MATERIALS + "/")) and p not in own_refs for p in dirty):
        raise RuntimeError("存在其它任务脏包，停止写入：" + str(dirty))
    manifest = json.loads((SOURCE / "SourceManifest.json").read_text(encoding="utf-8"))
    for row in manifest["textures"]+manifest.get("flight_textures",[]):
        if hashlib.sha256((SOURCE / row["file"]).read_bytes()).hexdigest() != row["sha256"]:
            raise RuntimeError("源图发生变化：" + row["file"])
    return manifest


def create(path, cls, factory):
    """认领唯一稳定路径；已存在且未在本轮清单中的资源绝不覆盖。"""
    if not path.startswith((TEXTURES + "/", MATERIALS + "/")):
        raise RuntimeError("资源超出本轮范围：" + path)
    ownership_file = OUT / "OwnedAssets.json"
    owned = json.loads(ownership_file.read_text()) if ownership_file.exists() else []
    exists = EL.does_asset_exist(path)
    if exists and path not in owned:
        raise RuntimeError("同名资源已有其它所有者：" + path)
    if path not in owned:
        owned.append(path)
        record("OwnedAssets.json", owned)
    asset = u.load_asset(path) if exists else AT.create_asset(
        path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], cls, factory)
    if not isinstance(asset, cls):
        raise RuntimeError("原生创建类型不正确：" + path)
    return asset


def save(asset):
    """逐包保存并真实回读；不调用全局save_dirty_packages。"""
    if not EL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError("原生保存失败：" + asset.get_path_name())
    if u.load_asset(asset.get_path_name()) is None:
        raise RuntimeError("原生回读失败：" + asset.get_path_name())
    return asset


def import_textures(manifest):
    """保留源RGBA，线性MASKS采样；原生构建缩至1024，不用填边破坏4×4分格。"""
    for row in manifest["textures"]:
        path = TEXTURES + "/" + Path(row["file"]).stem
        ownership_file = OUT / "OwnedAssets.json"
        owned = json.loads(ownership_file.read_text()) if ownership_file.exists() else []
        if EL.does_asset_exist(path) and path not in owned:
            raise RuntimeError("拒绝覆盖已有纹理：" + path)
        if path not in owned:
            owned.append(path)
            record("OwnedAssets.json", owned)
        if not EL.does_asset_exist(path):
            task = u.AssetImportTask()
            task.filename = str(SOURCE / row["file"])
            task.destination_path = TEXTURES
            task.destination_name = Path(row["file"]).stem
            task.automated, task.save, task.replace_existing = True, False, False
            AT.import_asset_tasks([task])
        texture = u.load_asset(path)
        if not isinstance(texture, u.Texture2D):
            raise RuntimeError("未导入Texture2D：" + path)
        settings = {
            "srgb": False,
            "compression_settings": u.TextureCompressionSettings.TC_MASKS,
            "compression_no_alpha": False,
            "address_x": u.TextureAddress.TA_CLAMP,
            "address_y": u.TextureAddress.TA_CLAMP,
            "filter": u.TextureFilter.TF_BILINEAR,
            "power_of_two_mode": u.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION,
            "resize_during_build_x": 1024,
            "resize_during_build_y": 1024,
            # 翻页素材不允许低级Mip混合相邻帧；后续性能审核可专门制作隔离Mip。
            "mip_gen_settings": u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
            "never_stream": True,
        }
        for key, value in settings.items():
            texture.set_editor_property(key, value)
        save(texture)


def expression(material, cls, x=0, y=0, **properties):
    """所有表达式由引擎生成；反射字段或创建失败立即中止。"""
    result = ML.create_material_expression(material, cls, x, y)
    for key, value in properties.items():
        result.set_editor_property(key, value)
    return result


def connect(source, output_pin, target, input_pin):
    """显式检查引脚连接结果，避免生成未接线的空材质。"""
    if not ML.connect_material_expressions(source, output_pin, target, input_pin):
        raise RuntimeError("材质引脚连接失败：" + input_pin)


def color_from_srgb(hex_rgb):
    """HEX是屏幕sRGB参考，材质参数按IEC转换为线性颜色。"""
    values = [int(hex_rgb[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    linear = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in values]
    return u.LinearColor(*linear, 1.0)


def author_master():
    """唯一翻页/拖尾发光母材质；RGB和Opacity分开输出，避免Alpha重复相乘。"""
    mat = create(MATERIALS + "/M_DBA_Fireball_Flame", u.Material, u.MaterialFactoryNew())
    ML.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode", u.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    for flag in ("used_with_niagara_sprites", "used_with_niagara_ribbons", "used_with_niagara_mesh_particles"):
        mat.set_editor_property(flag, True)
    uv = expression(mat, u.MaterialExpressionTextureCoordinate, -1200, -300)
    particle = expression(mat, u.MaterialExpressionParticleColor, -1200, -100)
    dynamic = expression(mat, u.MaterialExpressionDynamicParameter, -1200, 120,
                         param_names=["AgeSeconds", "FrameOffset", "Dissolve", "IntensityScale"],
                         default_value=u.LinearColor(0, 0, 0, 1))
    clock = expression(mat, u.MaterialExpressionTime, -1200, 350)
    tex = expression(mat, u.MaterialExpressionTextureObjectParameter, -1200, 530,
                     parameter_name="FlameTexture", sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS,
                     texture=u.load_asset(TEXTURES + "/T_DBA_Fireball_CoreFlipbook"))
    inputs = {"UV": (uv, ""), "PC": (particle, "RGB"), "PA": (particle, "A"),
              "D": (dynamic, "RGBA"), "ClockSeconds": (clock, ""), "Atlas": (tex, "")}
    defaults = {"Columns": 4, "Rows": 4, "FrameCount": 16, "FramesPerSecond": 24,
                "Loop": 1, "ManualFrame": -1, "UseNiagaraAge": 1, "Emission": 3.0,
                "OpacityScale": 1, "Dissolve": 0, "AlphaPower": 1, "EdgePadding": .04,
                "WarpStrength": .006, "StartOffsetSeconds": 0}
    for index, (name, value) in enumerate(defaults.items()):
        scalar = expression(mat, u.MaterialExpressionScalarParameter, -800, index * 80,
                            parameter_name=name, default_value=float(value), group="Fireball")
        inputs[name] = (scalar, "")
    for index, (name, value) in enumerate({"OuterColor": "F16B46", "FlameColor": "FFA31A", "HotColor": "FFF3C4"}.items()):
        vector = expression(mat, u.MaterialExpressionVectorParameter, -500, 900 + index * 100,
                            parameter_name=name, default_value=color_from_srgb(value), group="Fireball")
        inputs[name] = (vector, "RGB")
    code = r'''
// 一次请求的AgeSeconds来自Niagara，不能用全局时间代替独立的一次性命中生命周期。
float columns=max(1.0,floor(Columns)), rows=max(1.0,floor(Rows));
float count=clamp(floor(FrameCount),1.0,columns*rows);
float age=max(0.0,lerp(ClockSeconds,D.x,saturate(UseNiagaraAge))+StartOffsetSeconds);
float frame=ManualFrame>=0.0 ? ManualFrame : age*max(0.0,FramesPerSecond)+D.y;
frame=Loop>.5 ? fmod(max(0.0,frame),count) : clamp(frame,0.0,count-1.0);
float i=floor(frame), j=Loop>.5 ? fmod(i+1.0,count) : min(i+1.0,count-1.0);
float2 q=UV+WarpStrength*float2(sin(UV.y*15.0-age*4.0),sin(UV.x*13.0+age*3.0));
q=clamp(q,.005,.995);
float2 cell0=float2(fmod(i,columns),floor(i/columns));
float2 cell1=float2(fmod(j,columns),floor(j/columns));
float4 a=Texture2DSampleLevel(Atlas,AtlasSampler,(q+cell0)/float2(columns,rows),0);
float4 b=Texture2DSampleLevel(Atlas,AtlasSampler,(q+cell1)/float2(columns,rows),0);
float4 texel=lerp(a,b,frac(frame));
float energy=saturate(dot(texel.rgb,float3(.333333,.333333,.333334)));
float pad=clamp(EdgePadding,.001,.2);
float2 edges=smoothstep(0.0,pad,UV)*smoothstep(0.0,pad,1.0-UV);
float threshold=saturate(max(Dissolve,D.z));
float mask=pow(saturate(texel.a),max(.05,AlphaPower))*edges.x*edges.y;
mask*=smoothstep(threshold,min(1.0,threshold+.12),energy)*(1.0-step(.9999,threshold));
float3 tint=lerp(OuterColor,FlameColor,smoothstep(.08,.60,energy));
tint=lerp(tint,HotColor,smoothstep(.62,.99,energy));
float3 emission=tint*max(0.0,Emission)*(.18+.82*energy)*PC;
return float4(emission,saturate(mask*PA*OpacityScale*max(0.0,D.w)));
'''
    shader = expression(mat, u.MaterialExpressionCustom, 0, 100, code=code,
                        description="火球灰度翻页与独立生命周期", output_type=u.CustomMaterialOutputType.CMOT_FLOAT4)
    # UE5.8的CustomInput不是可带关键字构造的BlueprintType，需通过反射设置字段。
    pins = []
    for name in inputs:
        pin = u.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    shader.set_editor_property("inputs", pins)
    for name, (source, pin) in inputs.items():
        connect(source, pin, shader, name)
    rgb = expression(mat, u.MaterialExpressionComponentMask, 320, 0, r=True, g=True, b=True, a=False)
    alpha = expression(mat, u.MaterialExpressionComponentMask, 320, 220, r=False, g=False, b=False, a=True)
    connect(shader, "", rgb, "")
    connect(shader, "", alpha, "")
    depth = expression(mat, u.MaterialExpressionDepthFade, 550, 220, fade_distance_default=12.0)
    distance = expression(mat, u.MaterialExpressionScalarParameter, 320, 420,
                          parameter_name="SoftIntersectionCm", default_value=12.0, group="Fireball")
    connect(alpha, "", depth, "Opacity")
    connect(distance, "", depth, "FadeDistance")
    if not ML.connect_material_property(rgb, "", u.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Emissive未连接")
    if not ML.connect_material_property(depth, "", u.MaterialProperty.MP_OPACITY):
        raise RuntimeError("Opacity未连接")
    ML.recompile_material(mat)
    ML.layout_material_expressions(mat)
    return save(mat)


def instance(name, parent, scalars, texture=None):
    """实例只覆盖内容参数；体积和空气扰动复用既有唯一母材质。"""
    mat = create(MATERIALS + "/" + name, u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    ML.set_material_instance_parent(mat, parent)
    available = {str(key) for key in ML.get_scalar_parameter_names(parent)}
    for key, value in scalars.items():
        if key not in available:
            raise RuntimeError("实例参数未找到：" + key)
        # 锁定引擎的setter源码始终返回false；以真实参数存在性和写后回读判断结果。
        ML.set_material_instance_scalar_parameter_value(mat, key, float(value))
        if abs(ML.get_material_instance_scalar_parameter_value(mat, key) - value) > .0001:
            raise RuntimeError("实例参数回读不一致：" + key)
    if texture:
        if "FlameTexture" not in {str(key) for key in ML.get_texture_parameter_names(parent)}:
            raise RuntimeError("纹理实例参数未找到")
        target_texture = u.load_asset(TEXTURES + "/" + texture)
        ML.set_material_instance_texture_parameter_value(mat, "FlameTexture", target_texture)
        if ML.get_material_instance_texture_parameter_value(mat, "FlameTexture") != target_texture:
            raise RuntimeError("实例纹理回读不一致")
    ML.update_material_instance(mat)
    return save(mat)


def author_flight_textures(cfg):
    """唯一飞行纹理导入：原稿原字节保留，线性数据与sRGB颜色分开。"""
    manifest=check_context()
    rows=manifest.get("flight_textures",[])
    if len(rows)!=6:raise RuntimeError("飞行纹理必须是已登记的六项")
    result=[]
    for row in rows:
        source=SOURCE/row["file"]
        if hashlib.sha256(source.read_bytes()).hexdigest()!=row["sha256"]:raise RuntimeError("飞行源图SHA不符")
        path=TEXTURES+"/"+source.stem
        owned=json.loads((OUT/"OwnedAssets.json").read_text())
        if EL.does_asset_exist(path) and path not in owned:raise RuntimeError("飞行纹理已有其他所有者："+path)
        if path not in owned:owned.append(path);record("OwnedAssets.json",owned)
        if not EL.does_asset_exist(path):
            task=u.AssetImportTask();task.filename=str(source);task.destination_path=TEXTURES
            task.destination_name=source.stem;task.automated=True;task.save=False;task.replace_existing=False
            AT.import_asset_tasks([task])
        tex=u.load_asset(path)
        if not isinstance(tex,u.Texture2D):raise RuntimeError("飞行纹理原生导入失败")
        size=row.get("requested_build_size",row["size"])
        wrap=not source.stem.endswith(("ShellRing","HeatRG"))
        for key,value in {"srgb":row["srgb"],"compression_settings":u.TextureCompressionSettings.TC_DEFAULT if row["srgb"] else u.TextureCompressionSettings.TC_MASKS,
                          "compression_no_alpha":False,"address_x":u.TextureAddress.TA_WRAP if wrap else u.TextureAddress.TA_CLAMP,
                          "address_y":u.TextureAddress.TA_WRAP if source.stem.endswith(("CoreFlow","NoisePack")) else u.TextureAddress.TA_CLAMP,
                          "power_of_two_mode":u.TexturePowerOfTwoSetting.RESIZE_TO_SPECIFIC_RESOLUTION,
                          "resize_during_build_x":size[0],"resize_during_build_y":size[1],
                          "mip_gen_settings":u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE,"never_stream":False}.items():tex.set_editor_property(key,value)
        save(tex)
        result.append({"path":path,"sRGB":tex.get_editor_property("srgb"),"actual_size":[tex.blueprint_get_size_x(),tex.blueprint_get_size_y()]})
    record("FlightTextures.Native.json",result)
    return result


def author_core_surface(cfg):
    """建立唯一Opaque球面材质；先用于原生材质审核，再由既有NS接入。"""
    mat=create(MATERIALS+"/M_DBA_Fireball_CoreSurface",u.Material,u.MaterialFactoryNew())
    ML.delete_all_material_expressions(mat)
    mat.set_editor_property("blend_mode",u.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("shading_model",u.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("used_with_niagara_mesh_particles",True)
    nodes={
        "UV":expression(mat,u.MaterialExpressionTextureCoordinate,-900,-300),
        "Clock":expression(mat,u.MaterialExpressionTime,-900,-100),
        "D":expression(mat,u.MaterialExpressionDynamicParameter,-900,100,default_value=u.LinearColor(0,0,0,1)),
        "NormalWS":expression(mat,u.MaterialExpressionVertexNormalWS,-900,300),
        "PixelWS":expression(mat,u.MaterialExpressionWorldPosition,-900,500),
        "CameraWS":expression(mat,u.MaterialExpressionCameraPositionWS,-900,700),
        "FlowAtlas":expression(mat,u.MaterialExpressionTextureObjectParameter,-900,900,parameter_name="CoreFlowTexture",sampler_type=u.MaterialSamplerType.SAMPLERTYPE_MASKS,texture=u.load_asset(TEXTURES+"/T_DBA_Fireball_CoreFlow")),
        "FireIntensity":expression(mat,u.MaterialExpressionScalarParameter,-550,100,parameter_name="FireIntensity",default_value=cfg["head_emission"]),
        "UseNiagaraAge":expression(mat,u.MaterialExpressionScalarParameter,-550,250,parameter_name="UseNiagaraAge",default_value=1.),
        "Gold":expression(mat,u.MaterialExpressionVectorParameter,-550,450,parameter_name="Gold",default_value=color_from_srgb("FFD234")),
        "WarmWhite":expression(mat,u.MaterialExpressionVectorParameter,-550,650,parameter_name="WarmWhite",default_value=color_from_srgb("FFF5C9"))}
    axis=expression(mat,u.MaterialExpressionConstant3Vector,-900,1100,constant=u.LinearColor(1,0,0,1))
    forward=expression(mat,u.MaterialExpressionTransform,-550,1100,
                       transform_source_type=u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                       transform_type=u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    connect(axis,"",forward,"");nodes["ForwardWS"]=forward
    custom=expression(mat,u.MaterialExpressionCustom,0,0,code=Path(__file__).with_name("FireballCoreSurface.hlsl").read_text("utf-8"),output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
    pins=[]
    for key in nodes:
        pin=u.CustomInput();pin.set_editor_property("input_name",key);pins.append(pin)
    custom.set_editor_property("inputs",pins)
    for key,node in nodes.items():connect(node,"RGBA" if key=="D" else ("RGB" if key in ("Gold","WarmWhite") else ""),custom,key)
    if not ML.connect_material_property(custom,"",u.MaterialProperty.MP_EMISSIVE_COLOR):raise RuntimeError("圆球Emissive未接线")
    ML.recompile_material(mat);ML.layout_material_expressions(mat);save(mat)
    # 原生迁移原实例身份，保留已有NS引用；未建立第二套可竞争的当前核心。
    old=MATERIALS+"/MI_DBA_Fireball_Volume";new=MATERIALS+"/MI_DBA_Fireball_CoreSurface"
    owned=json.loads((OUT/"OwnedAssets.json").read_text())
    if not EL.does_asset_exist(new) and EL.does_asset_exist(old):
        if old not in owned:raise RuntimeError("旧核心实例不在本轮所有权中")
        rename=u.AssetRenameData();rename.set_editor_property("asset",u.load_asset(old))
        rename.set_editor_property("new_package_path",MATERIALS);rename.set_editor_property("new_name","MI_DBA_Fireball_CoreSurface")
        if not AT.rename_assets([rename]):raise RuntimeError("核心实例原生迁移失败")
        owned.remove(old);owned.append(new);record("OwnedAssets.json",owned)
    mi=instance("MI_DBA_Fireball_CoreSurface",mat,{"FireIntensity":cfg["head_emission"],"UseNiagaraAge":1.})
    return {"material":mat.get_path_name(),"instance":mi.get_path_name(),"blend":str(mat.get_editor_property("blend_mode"))}


def author_flight_materials(cfg):
    """当前飞行五类材质的唯一写入入口；源RGB、线性数据和Alpha各守一份合同。"""
    check_context()
    result={"core":author_core_surface(cfg)}
    # 收敛此前同一协调器已创建的两项实例，不接受任意同名资产。
    owned=json.loads((OUT/'OwnedAssets.json').read_text())
    for name,expected in [('Filament',MATERIALS+'/M_DBA_Fireball_Flame'),('Glow','/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials/M_DBA_DevMagicGlow')]:
        path=MATERIALS+'/MI_DBA_Fireball_'+name
        if EL.does_asset_exist(path) and path not in owned:
            old=u.load_asset(path)
            assert isinstance(old,u.MaterialInstanceConstant) and old.get_editor_property('parent').get_path_name().split('.')[0]==expected
            owned.append(path)
    record('OwnedAssets.json',owned)
    for family,source,is_shell in [('Shell','ShellRing',1),('Trail','TrailBroad',0)]:
        mat=create(MATERIALS+'/M_DBA_Fireball_'+family,u.Material,u.MaterialFactoryNew())
        ML.delete_all_material_expressions(mat)
        for key,value in {'blend_mode':u.BlendMode.BLEND_ADDITIVE,'shading_model':u.MaterialShadingModel.MSM_UNLIT,'two_sided':True,
                          'used_with_niagara_sprites':True,'used_with_niagara_ribbons':True,'used_with_niagara_mesh_particles':True}.items():mat.set_editor_property(key,value)
        pc=expression(mat,u.MaterialExpressionParticleColor,-900,0)
        nodes={'UV':(expression(mat,u.MaterialExpressionTextureCoordinate,-900,-200),''),'PC':(pc,'RGB'),'PA':(pc,'A'),
               'D':(expression(mat,u.MaterialExpressionDynamicParameter,-900,200,default_value=u.LinearColor(0,0,0,1)),'RGBA'),
               'Clock':(expression(mat,u.MaterialExpressionTime,-900,400),''),
               'Flame':(expression(mat,u.MaterialExpressionTextureObjectParameter,-900,600,parameter_name='FlameTexture',sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR,texture=u.load_asset(TEXTURES+'/T_DBA_Fireball_'+source)),'')}
        for i,(key,value) in enumerate({'Emission':3.0 if is_shell else cfg['trail_emission'],'OpacityScale':.7 if is_shell else 1.,'UseNiagaraAge':1.,'IsShell':float(is_shell),'CoolingStrength':cfg['trail_cooling_strength']}.items()):
            nodes[key]=(expression(mat,u.MaterialExpressionScalarParameter,-600,800+i*80,parameter_name=key,default_value=float(value)),'')
        custom=expression(mat,u.MaterialExpressionCustom,0,0,code=Path(__file__).with_name('FireballFlightFlame.hlsl').read_text('utf-8'),output_type=u.CustomMaterialOutputType.CMOT_FLOAT4)
        pins=[]
        for key in nodes:
            pin=u.CustomInput();pin.set_editor_property('input_name',key);pins.append(pin)
        custom.set_editor_property('inputs',pins)
        for key,(node,pin) in nodes.items():connect(node,pin,custom,key)
        rgb=expression(mat,u.MaterialExpressionComponentMask,350,0,r=True,g=True,b=True,a=False)
        alpha=expression(mat,u.MaterialExpressionComponentMask,350,180,r=False,g=False,b=False,a=True)
        connect(custom,'',rgb,'');connect(custom,'',alpha,'')
        assert ML.connect_material_property(rgb,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        assert ML.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
        if is_shell:
            axis=expression(mat,u.MaterialExpressionConstant3Vector,-1100,1100,constant=u.LinearColor(1,0,0,1))
            forward=expression(mat,u.MaterialExpressionTransform,-800,1100,transform_source_type=u.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,transform_type=u.MaterialVectorCoordTransform.TRANSFORM_WORLD)
            connect(axis,'',forward,'')
            wpins={'UV':nodes['UV'],'Clock':nodes['Clock'],'IsShell':nodes['IsShell'],
                   'NormalWS':(expression(mat,u.MaterialExpressionVertexNormalWS,-800,1300),''),'ForwardWS':(forward,'')}
            wpo=expression(mat,u.MaterialExpressionCustom,0,600,code=Path(__file__).with_name('FireballShellSurface.hlsl').read_text('utf-8'),output_type=u.CustomMaterialOutputType.CMOT_FLOAT3)
            fields=[]
            for key in wpins:
                pin=u.CustomInput();pin.set_editor_property('input_name',key);fields.append(pin)
            wpo.set_editor_property('inputs',fields)
            for key,(node,pin) in wpins.items():connect(node,pin,wpo,key)
            assert ML.connect_material_property(wpo,'',u.MaterialProperty.MP_WORLD_POSITION_OFFSET)
        ML.recompile_material(mat);ML.layout_material_expressions(mat);save(mat)
        instance('MI_DBA_Fireball_'+family,mat,{'UseNiagaraAge':1.},'T_DBA_Fireball_'+source)
        if is_shell:
            instance('MI_DBA_Fireball_ShellSprite',mat,{'UseNiagaraAge':1.,'IsShell':2.,'OpacityScale':.28,'Emission':2.},'T_DBA_Fireball_ShellRing')
        if not is_shell:
            instance('MI_DBA_Fireball_Filament',mat,{'Emission':4.0,'OpacityScale':.85,'CoolingStrength':cfg['filament_cooling_strength']},'T_DBA_Fireball_TrailFilament')
            instance('MI_DBA_Fireball_FlightEmber',mat,{'Emission':2.0,'OpacityScale':.65},'T_DBA_Fireball_TrailFilament')
        result[family]=mat.get_path_name()
    glow=u.load_asset('/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials/M_DBA_DevMagicGlow')
    instance('MI_DBA_Fireball_Glow',glow,{})
    configure_emission_scale(cfg)
    record('FlightMaterials.Native.json',result)
    return result


def configure_emission_scale(cfg):
    """按唯一合同写绝对发光值，重复运行不叠乘；不改原稿RGB或共享母材质。"""
    gain=float(cfg['material_emission_scale'])
    assert 0<=gain<=1
    owned=json.loads((OUT/'OwnedAssets.json').read_text())
    values={'Core':('Emission',3.2),'CoreSurface':('FireIntensity',cfg['head_emission']),
            'Trail':('Emission',cfg['trail_emission']),'Shell':('Emission',3.),
            'ShellSprite':('Emission',2.),'Filament':('Emission',4.),
            'FlightEmber':('Emission',2.),'Impact':('Emission',4.),'Ember':('Emission',1.7)}
    rows=[]
    for name,(key,nominal) in values.items():
        path=MATERIALS+'/MI_DBA_Fireball_'+name
        assert path in owned,path
        mi=u.load_asset(path);assert isinstance(mi,u.MaterialInstanceConstant)
        value=float(nominal)*gain
        assert key in {str(n) for n in ML.get_scalar_parameter_names(mi.get_editor_property('parent'))},(path,key)
        # 与instance()相同：锁定UE的setter返回false，以真实参数及写后回读判定。
        # 已达到绝对合同值时不重复写包，避免其它编辑器索引刚保存资产时的读取锁。
        changed=abs(ML.get_material_instance_scalar_parameter_value(mi,key)-value)>1e-5
        if changed:ML.set_material_instance_scalar_parameter_value(mi,key,value)
        actual=ML.get_material_instance_scalar_parameter_value(mi,key)
        assert abs(actual-value)<1e-5,(path,actual,value)
        if changed:save(mi)
        rows.append({'asset':path,'parameter':key,'nominal':nominal,'scale':gain,'actual':actual})
    record('Emission.Native.json',rows)
    return rows


def author():
    """只导入三纹理、一个母材质和六个实例；不生成或绑定第二套Niagara。"""
    manifest = check_context()
    visual = json.loads(Path(__file__).with_name("PresentationManifest.json").read_text("utf-8"))["fireball_appearance"]
    import_textures(manifest)
    parent = author_master()
    instance("MI_DBA_Fireball_Core", parent, {"Emission": 3.2, "Loop": 1})
    instance("MI_DBA_Fireball_Trail", parent,
             {"Columns": 1, "Rows": 1, "FrameCount": 1, "Emission": visual["trail_emission"], "WarpStrength": .012}, "T_DBA_Fireball_Trail")
    instance("MI_DBA_Fireball_Impact", parent,
             {"Loop": 0, "FramesPerSecond": 26, "Emission": 4.0}, "T_DBA_Fireball_ImpactFlipbook")
    instance("MI_DBA_Fireball_Ember", parent,
             {"Columns": 1, "Rows": 1, "FrameCount": 1, "Emission": 1.7, "OpacityScale": .65}, "T_DBA_Fireball_Trail")
    author_flight_textures(visual)
    author_flight_materials(visual)
    ripple = u.load_asset(BASE + "/Materials/M_DBA_PalmAirRipple")
    instance("MI_DBA_Fireball_AirRipple", ripple, {"P_Intensity": .008, "P_Opacity": .06})
    validate("NativeSaved.json")


def validate(report_name="NativeReloaded.json"):
    """按唯一源图/映射合同检查实际保存资产，拒绝旧覆盖与错误颜色空间。"""
    manifest=check_context();owned=json.loads((OUT/'OwnedAssets.json').read_text())
    source_rows={TEXTURES+'/'+Path(r['file']).stem:r for r in manifest['textures']+manifest.get('flight_textures',[])}
    parents={'Core':'Flame','Impact':'Flame','Ember':'Flame','CoreSurface':'CoreSurface','Trail':'Trail','Shell':'Shell','ShellSprite':'Shell','Filament':'Trail','FlightEmber':'Trail'}
    rows=[]
    for path in owned:
        asset=u.load_asset(path)
        if asset is None:raise RuntimeError('已认领资源丢失:'+path)
        row={'path':path,'class':asset.get_class().get_name()}
        if isinstance(asset,u.Texture2D):
            spec=source_rows[path];size=spec.get('requested_build_size',spec.get('size',[1024,1024]))
            row.update(srgb=asset.get_editor_property('srgb'),actual_size=[asset.blueprint_get_size_x(),asset.blueprint_get_size_y()])
            if row['srgb']!=spec.get('srgb',False) or row['actual_size']!=size:raise RuntimeError('纹理颜色空间/构建尺寸错误:'+str(row))
        elif isinstance(asset,u.MaterialInstanceConstant):
            name=path.rsplit('_',1)[1];parent=asset.get_editor_property('parent').get_path_name().split('.')[0]
            expected=MATERIALS+'/M_DBA_Fireball_'+parents[name] if name in parents else (BASE+'/Materials/M_DBA_PalmAirRipple' if name=='AirRipple' else '/DBAPresentationPack_Core/VFX/DevelopmentMagic/Materials/M_DBA_DevMagicGlow')
            if parent!=expected:raise RuntimeError('实例父类不符唯一职责合同:'+path)
            row['parent']=parent
            if 'UseNiagaraAge' in {str(n) for n in ML.get_scalar_parameter_names(asset)} and ML.get_material_instance_scalar_parameter_value(asset,'UseNiagaraAge')!=1:raise RuntimeError('残留材质预览时间覆盖')
        rows.append(row)
    if not set(source_rows).issubset(owned):raise RuntimeError('已登记源纹理尚未全部原生导入')
    record(report_name,{'engine':u.SystemLibrary.get_engine_version(),'assets':rows,'cook_verified':False,'gpu_budget_verified':False,'manual_accepted':False})
    print('FIREBALL_NATIVE_VERIFIED',len(rows),report_name)


if __name__ == "__main__":
    author()
