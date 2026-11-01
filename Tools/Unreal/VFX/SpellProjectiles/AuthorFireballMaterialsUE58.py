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
    if any(not p.startswith((TEXTURES + "/", MATERIALS + "/")) for p in dirty):
        raise RuntimeError("存在其它任务脏包，停止写入：" + str(dirty))
    manifest = json.loads((SOURCE / "SourceManifest.json").read_text(encoding="utf-8"))
    for row in manifest["textures"]:
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
    volume = u.load_asset(BASE + "/Materials/M_DBA_FireOrbVolume")
    ripple = u.load_asset(BASE + "/Materials/M_DBA_PalmAirRipple")
    if not isinstance(volume, u.Material) or not isinstance(ripple, u.Material):
        raise RuntimeError("既有体积/扰动母材质缺失，禁止复制替代")
    instance("MI_DBA_Fireball_Volume", volume,
             {"RadiusCm": visual["head_diameter_cm"]*.5, "FormProgress": 1, "EmissionScale": visual["head_emission"], "DensityScale": visual["head_density"], "HeadFullness": visual["head_fullness"]})
    instance("MI_DBA_Fireball_AirRipple", ripple, {"P_Intensity": .008, "P_Opacity": .06})
    validate("NativeSaved.json")


def validate(report_name="NativeReloaded.json"):
    """检查保存资源的实际类型、纹理构建设置和实例父类；不声称Cook或GPU通过。"""
    manifest = check_context()
    owned = json.loads((OUT / "OwnedAssets.json").read_text())
    rows = []
    for path in owned:
        asset = u.load_asset(path)
        if asset is None:
            raise RuntimeError("保存后资源丢失：" + path)
        row = {"path": path, "class": asset.get_class().get_name()}
        if isinstance(asset, u.Texture2D):
            row.update({"sRGB": asset.get_editor_property("srgb"),
                        "build_size": [asset.get_editor_property("resize_during_build_x"), asset.get_editor_property("resize_during_build_y")],
                        "actual_size": [asset.blueprint_get_size_x(), asset.blueprint_get_size_y()],
                        "compression": str(asset.get_editor_property("compression_settings")),
                        "mips": str(asset.get_editor_property("mip_gen_settings"))})
            if row["sRGB"] or row["actual_size"] != [1024, 1024]:
                raise RuntimeError("实际构建尺寸或线性设置不正确：" + str(row))
        elif isinstance(asset, u.MaterialInstanceConstant):
            row["parent"] = asset.get_editor_property("parent").get_path_name()
            name = path.rsplit("/", 1)[1]
            if name not in ("MI_DBA_Fireball_Volume", "MI_DBA_Fireball_AirRipple"):
                if not row["parent"].startswith(MATERIALS + "/M_DBA_Fireball_Flame."):
                    raise RuntimeError("实例未复用唯一火焰母材质：" + path)
                row["scalars"] = {key: ML.get_material_instance_scalar_parameter_value(asset, key)
                                  for key in ("Columns", "Rows", "FrameCount", "Loop", "ManualFrame", "UseNiagaraAge")}
                row["texture"] = ML.get_material_instance_texture_parameter_value(asset, "FlameTexture").get_path_name()
                expected_grid = 1 if name.endswith(("Trail", "Ember")) else 4
                if any(row["scalars"][key] != expected_grid for key in ("Columns", "Rows")):
                    raise RuntimeError("翻页网格合同错误：" + path)
                if row["scalars"]["FrameCount"] != expected_grid ** 2:
                    raise RuntimeError("实际帧数合同错误：" + path)
                if row["scalars"]["ManualFrame"] != -1 or row["scalars"]["UseNiagaraAge"] != 1:
                    raise RuntimeError("残留预览覆盖或未使用独立粒子年龄：" + path)
                if name.endswith("Impact") and row["scalars"]["Loop"] != 0:
                    raise RuntimeError("命中错误地循环播放")
            elif name.endswith("Volume") and not row["parent"].startswith(BASE + "/Materials/M_DBA_FireOrbVolume."):
                raise RuntimeError("未复用既有体积母材质")
            elif name.endswith("AirRipple") and not row["parent"].startswith(BASE + "/Materials/M_DBA_PalmAirRipple."):
                raise RuntimeError("未复用既有空气扰动母材质")
        elif isinstance(asset, u.Material):
            row["blend"] = str(asset.get_editor_property("blend_mode"))
            row["scalars"] = [str(n) for n in ML.get_scalar_parameter_names(asset)]
        rows.append(row)
    if len(rows) != 10 or len(manifest["textures"]) != 3:
        raise RuntimeError("本轮资源数量不是3纹理+1母材质+6实例")
    record(report_name, {"engine": u.SystemLibrary.get_engine_version(), "assets": rows,
                         "niagara_modified": False, "cook_verified": False, "gpu_budget_verified": False})
    print("FIREBALL_NATIVE_VERIFIED", len(rows), report_name)


if __name__ == "__main__":
    author()
