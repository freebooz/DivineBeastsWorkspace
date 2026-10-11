# -*- coding: utf-8 -*-
"""UE5.8 独立粒子 Shader 故障定位。

仅在/Game/Development/FallingFoliage/Probes新建Shader/系统/关卡，不修改
GamePlatformVFX正式Niagara和母材质，也不编辑角色选择/创建场景。
四路对照：Solid（实色）验证默认材质回退；UV0（渐变）验证Sprite UV；
Opacity（遮罩）验证真实梨花纹理轮廓；PBR复用现有独立母材质探针。
审核：保存资源不代表真实D3D12视觉通过，后续需读取GPU截图。
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parent
BASE = "/Game/Development/FallingFoliage/Probes"
ORIG = "/GamePlatformVFX/Weather/Niagara/NS_GP_Weather_Snow"
PEAR = "/DBAPresentationPack_Core/VFX/Environment/AmbientNature/Materials/MI_DBA_Ambient_PearPetal"
PBR = BASE + "/NS_DBA_PearPetal_WeatherMaskSamplerProbe"
MAP = BASE + "/L_DBA_FoliageShaderIsolationReview"
OUT = ROOT / "Saved/Validation/FallingFoliage/PBRAtlas20261011/GPUSceneReview/ShaderIsolationAudit.json"
ED = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
VARIANTS = ("Solid", "UV0", "Opacity")
report = {"status": "pending", "created": [], "renderers": [],
          "errors": [], "map": MAP, "gpuVisualPassed": False}

def require(condition, message):
    """条件不满足时失败，禁止把残缺资产冒充成功。"""
    if not condition:
        raise RuntimeError(message)

def expr(mat, cls, x, y):
    """用编辑器原生API创建测试材质节点。"""
    n = LIB.create_material_expression(mat, cls, x, y)
    require(n is not None, "材质节点创建失败：" + str(cls))
    return n

def edge(a, output, b, pin):
    """对真实材质图连线逐条回报失败。"""
    require(LIB.connect_material_expressions(a, output, b, pin),
            "材质输入连接失败：" + pin)

def port(a, output, property_name):
    """输出至真实Shader材质属性。"""
    require(LIB.connect_material_property(a, output, property_name),
            "材质输出连线失败：" + str(property_name))

def author_material(style, albedo, mask):
    """一材质一问题，消除PBR及地面运动的干扰。"""
    name = "M_DBA_FoliageShaderIsolation_" + style
    m = TOOLS.create_asset(name, BASE, unreal.Material, unreal.MaterialFactoryNew())
    require(isinstance(m, unreal.Material), "无法创建Niagara测试材质")
    report["created"].append(BASE + "/" + name)
    m.set_editor_property("used_with_niagara_sprites", True)
    m.set_editor_property("two_sided", True)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("opacity_mask_clip_value", .3)
    if style == "Solid":
        color = expr(m, unreal.MaterialExpressionConstant3Vector, -500, -200)
        color.set_editor_property("constant", unreal.LinearColor(.95, .05, .1, 1.0))
        port(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    elif style == "UV0":
        uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -700, -270)
        uv.set_editor_property("coordinate_index", 0)
        z = expr(m, unreal.MaterialExpressionConstant, -690, -75)
        z.set_editor_property("r", .25)
        vector = expr(m, unreal.MaterialExpressionAppendVector, -300, -200)
        edge(uv, "", vector, "A")
        edge(z, "", vector, "B")
        port(vector, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    elif style == "Opacity":
        uv = expr(m, unreal.MaterialExpressionTextureCoordinate, -900, -320)
        uv.set_editor_property("coordinate_index", 0)
        tex = expr(m, unreal.MaterialExpressionTextureSample, -600, -350)
        tex.set_editor_property("texture", albedo)
        tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        edge(uv, "", tex, "Coordinates")
        port(tex, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        alpha = expr(m, unreal.MaterialExpressionTextureSample, -600, 0)
        alpha.set_editor_property("texture", mask)
        alpha.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        edge(uv, "", alpha, "Coordinates")
        port(alpha, "R", unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        raise RuntimeError("未知测试风格：" + style)
    if style != "Opacity":
        one = expr(m, unreal.MaterialExpressionConstant, -300, 90)
        one.set_editor_property("r", 1.0)
        port(one, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    diagnostic = LIB.recompile_material(m)
    require(not any("error" in str(x).lower() for x in (diagnostic or [])),
            "Shader编译错误：" + str(diagnostic))
    require(ED.save_loaded_asset(m, only_if_is_dirty=False),
            "测试Shader没有保存")
    return m

def clone_system(style, m):
    """复制经验证可产生GPU粒子的天气Niagara，避免混入GroundMotion错误。"""
    dst = BASE + "/NS_DBA_FoliageShaderIsolation_" + style
    ns = ED.duplicate_asset(ORIG, dst)
    require(isinstance(ns, unreal.NiagaraSystem), "不能复制天气GPU粒子")
    report["created"].append(dst)
    count = 0
    for sprite in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
        if not sprite.get_path_name().startswith(ns.get_path_name() + ":"):
            continue
        sprite.set_editor_property("material", m)
        sprite.set_editor_property("sub_image_size", unreal.Vector2D(1, 1))
        require(sprite.get_editor_property("material") == m,
                "Sprite渲染器实际材质与待测对象不一致")
        count += 1
    require(count == 2, "近远景必须有2个SpriteRenderer，实际：" + str(count))
    require(ED.save_loaded_asset(ns, only_if_is_dirty=False),
            "Niagara系统没保存")
    report["renderers"].append({
        "system": dst, "material": m.get_path_name(), "count": count, "subImages": "1x1"})
    return ns

def author_map(systems):
    """地图生成由UE5.8编辑器执行，不生成文本伪造的.umap。"""
    level = unreal.EditorLevelLibrary
    require(level.new_level(MAP), "试验地图创建失败")
    report["created"].append(MAP)
    def place(cls, label, xyz, rotation=None):
        obj = level.spawn_actor_from_class(cls, unreal.Vector(*xyz),
                                           rotation or unreal.Rotator(0, 0, 0))
        require(obj is not None, "地图Actor创建失败：" + label)
        obj.set_actor_label("ShaderProbe_" + label)
        return obj
    ground = place(unreal.StaticMeshActor, "Ground", (0, 0, -5))
    mesh = ED.load_asset("/Engine/BasicShapes/Plane")
    require(mesh is not None, "官方UE测试平面网格缺失")
    ground.get_component_by_class(unreal.StaticMeshComponent).set_static_mesh(mesh)
    ground.set_actor_scale3d(unreal.Vector(38, 38, 1))
    for name, ns, y in zip(("Solid", "UV0", "Opacity", "PBR"),
                           systems, (-600, -200, 200, 600)):
        actor = place(unreal.NiagaraActor, name, (0, y, 45))
        comp = actor.get_component_by_class(unreal.NiagaraComponent)
        require(comp is not None, "测试Actor缺Niagara组件")
        comp.set_asset(ns)
        comp.set_editor_property("auto_activate", True)
        comp.set_variable_float("User.WeatherIntensity", 1.0)
        comp.set_variable_float("User.WeatherNearSpawnRate", 145.0)
        comp.set_variable_float("User.WeatherFarSpawnRate", 20.0)
        actor.set_actor_scale3d(unreal.Vector(1.5, 1.5, 1.5))
    light_actor = place(unreal.DirectionalLight, "Sun", (250, 100, 1000),
                        unreal.Rotator(pitch=-55, yaw=30, roll=0))
    light_actor.get_component_by_class(unreal.DirectionalLightComponent).set_editor_property(
        "intensity", 40.0)
    place(unreal.SkyLight, "Sky", (0, 0, 700))
    place(unreal.SkyAtmosphere, "Atmosphere", (0, 0, 0))
    view = unreal.Rotator(pitch=-9, yaw=0, roll=0)
    place(unreal.PlayerStart, "PlayerStart", (-1450, 0, 250), view)
    place(unreal.CameraActor, "Camera", (-1450, 0, 250), view)
    require(level.save_current_level(), "独立真实关卡未保存")
    report["expectedLeftToRight"] = [
        "Solid红色实心四边形", "UV0二维RG渐变", "Opacity真实梨花形状及透明边缘",
        "PBR真实受光梨花形状"]

try:
    project = str(unreal.Paths.get_project_file_path()).replace("\\", "/")
    require(project.endswith("/DivineBeastsArena.uproject"), "不是神兽联盟正式工程")
    require(str(unreal.SystemLibrary.get_engine_version()).startswith("5.8."),
            "不是锁定UE5.8引擎")
    paths = [BASE + "/M_DBA_FoliageShaderIsolation_" + k for k in VARIANTS]
    paths += [BASE + "/NS_DBA_FoliageShaderIsolation_" + k for k in VARIANTS]
    paths += [MAP]
    for path in paths:
        require(not ED.does_asset_exist(path), "测试目标存在，拒绝覆盖：" + path)
    mi = ED.load_asset(PEAR)
    pbr = ED.load_asset(PBR)
    require(isinstance(mi, unreal.MaterialInstanceConstant), "梨花MI源资产无效")
    require(isinstance(pbr, unreal.NiagaraSystem), "既有PBR对照系统缺失")
    require(isinstance(ED.load_asset(ORIG), unreal.NiagaraSystem), "天气源系统缺失")
    albedo = LIB.get_material_instance_texture_parameter_value(
        mi, unreal.Name("LeafAlbedo"))
    opacity = LIB.get_material_instance_texture_parameter_value(
        mi, unreal.Name("LeafOpacity"))
    require(isinstance(albedo, unreal.Texture2D) and isinstance(opacity, unreal.Texture2D),
            "真实材质实例贴图绑定缺失")
    require(opacity.get_editor_property("compression_settings") ==
            unreal.TextureCompressionSettings.TC_MASKS and
            not opacity.get_editor_property("srgb"),
            "Opacity材质纹理必须使用线性TC_MASKS")
    report["textures"] = {"albedo": albedo.get_path_name(),
                          "opacity": opacity.get_path_name(), "maskChannel": "R"}
    systems = [clone_system(style, author_material(style, albedo, opacity))
               for style in VARIANTS]
    systems.append(pbr)
    author_map(systems)
    report["status"] = "assets_created_visual_pending"
    print("FOLIAGE_SHADER_ISOLATION_CREATED", len(report["created"]), MAP, flush=True)
except Exception as exc:
    report["status"] = "failed"
    report["errors"].append(str(exc))
    print("FOLIAGE_SHADER_ISOLATION_ERROR", str(exc), flush=True)
    raise
finally:
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf8")
