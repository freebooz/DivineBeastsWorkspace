"""SSLM 光标原生资源作者。导入、RHI、Cook 和人工核验分别留证。

只能经正式编辑器 Monolith editor.run_python 调用：
  author = runpy.run_path(script, init_globals={'MONOLITH_CURSOR_AUTHOR': True})
  author['import_resources']()
  requests = author['widget_requests'](dry_run=True)  # 先审核，再生成非 dry_run 请求
每个非 dry_run 的 ui.build_ui_from_spec 成功响应，须立即交给
record_widget_result(已解析的动作结果)，全部登记后才调用 finalize_widgets()/verify_native()。
唯一数据合同为相邻 CursorTheme.json。仅保存本任务资产，不绑定鼠标、不修改 INI
或共享清单；报告中的原生回读不等于重启重载、RHI、Cook 或人工验收。
"""
from pathlib import Path
import hashlib
import json
import math
import re
import shutil
import struct
import uuid
import unreal as u

ROOT = Path(__file__).resolve().parents[3]
CONFIG = Path(__file__).with_name('CursorTheme.json')
PACK = ROOT / 'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core'
OUT = ROOT / 'Game/Saved/Validation/CursorReplacement'
OWNER = 'Tools/Unreal/UI/AuthorCursorUE58.py'
LIB = u.MaterialEditingLibrary
UV_CODE = 'return UV * float2(UVScaleX, UVScaleY) + float2(UVOffsetX, UVOffsetY);'
ALPHA_CODE = 'return saturate((Alpha - Floor) / (1.0 - Floor));'


def require(condition, message):
    """原生动作或回读失败即中止，绝不以工具传输成功替代资产结果。"""
    if not condition:
        raise RuntimeError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def report(name, value):
    """仅写工程内证据；原子替换登记文件，避免半份 JSON 被当作资产归属。"""
    OUT.mkdir(parents=True, exist_ok=True)
    target = OUT / name
    temporary = target.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(target)


def native_file(path):
    require(bool(re.fullmatch(r'/DBAUIPack_Core/UI/Cursors/[A-Za-z0-9_/]+', path)),
            '资产必须位于本任务光标职责目录：' + str(path))
    return PACK / ('Content/' + path.split('/DBAUIPack_Core/', 1)[1] + '.uasset')


def fingerprints(path):
    """把真实包与引擎侧车一起登记；不存在的资产不能获得成功指纹。"""
    file = native_file(path)
    return {suffix: digest(file.with_suffix(suffix)) for suffix in ('.uasset', '.uexp', '.ubulk')
            if file.with_suffix(suffix).is_file()}


def context():
    """每个公开阶段重新验证主工程、精确引擎版本、PIE 和源图合同。"""
    require(globals().get('MONOLITH_CURSOR_AUTHOR') is True, '必须经 Monolith run_python 显式调用作者')
    require(Path(u.Paths.convert_relative_path_to_full(u.Paths.get_project_file_path())).resolve()
            == ROOT / 'Game/DivineBeastsArena.uproject',
            '当前不是本工作空间正式主工程')
    require(str(u.SystemLibrary.get_engine_version()).split('-', 1)[0] == '5.8.0', '必须使用 UE5.8.0')
    require(u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world() is None, 'PIE 运行时禁止制作')
    cfg = json.loads(CONFIG.read_text(encoding='utf-8-sig'))
    require(cfg['schema_version'] == 1 and len(cfg['states']) == 10, '光标合同版本或状态数量错误')
    require(cfg['widget_parent'] == '/Script/GamePlatformUIClient.GamePlatformWidgetBase'
            and u.load_class(None, cfg['widget_parent']) is not None, '平台 UI 父类缺失或合同越层；禁止降级')
    require(len(cfg['display_size']) == 2 and all(type(v) is int and 0 < v <= 64 for v in cfg['display_size']),
            '光标画布须为不超过64逻辑像素的有效尺寸')
    require(math.isfinite(cfg['opacity_floor']) and 0 <= cfg['opacity_floor'] < 1, 'Alpha 阈值非法')
    source = (ROOT / cfg['source']).resolve()
    require(source.is_relative_to(ROOT) and source.is_file(), '图集源路径越界或不存在')
    require(digest(source) == cfg['source_sha256'], '图集源 SHA256 与合同不符')
    header = source.read_bytes()[:33]
    require(header[:8] == b'\x89PNG\r\n\x1a\n' and header[12:16] == b'IHDR'
            and list(struct.unpack('>II', header[16:24])) == cfg['source_size']
            and header[24:26] == bytes((8, 6)), '源图必须为合同尺寸的 8 位 RGBA PNG')
    paths = [cfg['texture'], cfg['material']]
    ids = set()
    for state in cfg['states']:
        require(state['id'] not in ids, '状态 ID 重复')
        ids.add(state['id'])
        x, y, w, h = state['rect_px']
        hx, hy = state['hotspot_px']
        require(all(isinstance(v, int) for v in (x, y, w, h)) and min(x, y) >= 0
                and w > 1 and h > 1 and x + w <= cfg['source_size'][0]
                and y + h <= cfg['source_size'][1], '图集裁切范围非法：' + state['id'])
        require(all(math.isfinite(v) for v in (hx, hy)) and 0 <= hx < w and 0 <= hy < h,
                '热点不在对应单元内部：' + state['id'])
        paths.extend((state['material'], state['widget']))
    require(len(paths) == len(set(paths)), '合同出现重复资产路径')
    for path in paths:
        native_file(path)
    registry_path = OUT / 'Ownership.json'
    registry = json.loads(registry_path.read_text(encoding='utf-8')) if registry_path.exists() else {
        'Owner': OWNER, 'Assets': {}, 'Requests': {}}
    require(registry['Owner'] == OWNER, '现有登记不属于本作者')
    return cfg, registry, paths


def preflight(registry, paths):
    """修改前全量检查；未登记资产、外部修改和任何目标脏包均拒绝覆盖。"""
    dirty = {str(p.get_name()) for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    for path in paths:
        actual = fingerprints(path)
        exists = u.EditorAssetLibrary.does_asset_exist(path)
        require(path not in dirty, '目标包尚有未保存修改：' + path)
        require(exists == ('.uasset' in actual), '资产注册与磁盘包不一致：' + path)
        if exists or actual or path in registry['Assets']:
            require(path in registry['Assets'] and registry['Assets'][path]['Files'] == actual,
                    '目标未登记或其指纹已被外部改变：' + path)


def backup(path):
    """每次修改前保留旧原生包；同哈希备份只复用，不覆盖历史备份。"""
    for suffix, sha in fingerprints(path).items():
        destination = OUT / 'Backups' / sha / native_file(path).with_suffix(suffix).name
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            require(digest(destination) == sha, '已有备份损坏：' + str(destination))
        else:
            shutil.copy2(native_file(path).with_suffix(suffix), destination)


def register(path, registry):
    files = fingerprints(path)
    require('.uasset' in files, '原生包未保存：' + path)
    registry['Assets'][path] = {'Files': files, 'ContractSHA256': digest(CONFIG)}
    report('Ownership.json', registry)


def save(asset, registry):
    path = asset.get_path_name().split('.', 1)[0]
    require(u.EditorAssetLibrary.save_loaded_asset(asset, False), '原生保存失败：' + path)
    register(path, registry)


def ensure_asset(path, kind, factory):
    asset = u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else \
        u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/', 1)[1], path.rsplit('/', 1)[0], kind, factory)
    require(isinstance(asset, kind), '原生资产类型错误：' + path)
    return asset


def uv_parameters(cfg, state):
    """UV 以内侧纹素中心为边界，防止双线性采样串到相邻光标单元。"""
    x, y, w, h = state['rect_px']
    width, height = cfg['source_size']
    return dict(UVScaleX=(w - 1) / width, UVScaleY=(h - 1) / height,
                UVOffsetX=(x + .5) / width, UVOffsetY=(y + .5) / height)


def import_resources():
    """导入一张图集，创建唯一 UI 母材质和十个数据实例；不设置运行时光标。"""
    cfg, registry, paths = context()
    preflight(registry, paths)
    backup(cfg['texture'])
    task = u.AssetImportTask()
    for key, value in dict(filename=str(ROOT / cfg['source']), destination_path=cfg['texture'].rsplit('/', 1)[0],
                           destination_name=cfg['texture'].rsplit('/', 1)[1], replace_existing=True,
                           automated=True, save=False).items():
        task.set_editor_property(key, value)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = u.load_asset(cfg['texture'])
    require(isinstance(texture, u.Texture2D), '图集没有导入为真实 Texture2D')
    for key, value in texture_settings().items():
        texture.set_editor_property(key, value)
    save(texture, registry)
    backup(cfg['material'])
    material = ensure_asset(cfg['material'], u.Material, u.MaterialFactoryNew())
    material.set_editor_property('material_domain', u.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT)
    LIB.delete_all_material_expressions(material)

    def node(kind, **properties):
        expression = LIB.create_material_expression(material, kind, 0, 0)
        require(expression is not None, '材质节点创建失败')
        for key, value in properties.items():
            expression.set_editor_property(key, value)
        return expression

    def connect(source, output, target, input_name):
        require(LIB.connect_material_expressions(source, output, target, input_name),
                '材质连线失败：' + input_name)

    def custom(code, output_type, inputs):
        expression = node(u.MaterialExpressionCustom, code=code, output_type=output_type)
        pins = []
        for name in inputs:
            pin = u.CustomInput()
            pin.set_editor_property('input_name', name)
            pins.append(pin)
        expression.set_editor_property('inputs', pins)
        for name, (source, output) in inputs.items():
            connect(source, output, expression, name)
        return expression

    inputs = {'UV': (node(u.MaterialExpressionTextureCoordinate), '')}
    for name, value in dict(UVScaleX=1., UVScaleY=1., UVOffsetX=0., UVOffsetY=0.).items():
        inputs[name] = (node(u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value), '')
    uv = custom(UV_CODE, u.CustomMaterialOutputType.CMOT_FLOAT2, inputs)
    sample = node(u.MaterialExpressionTextureSampleParameter2D, parameter_name='AtlasTexture',
                  texture=texture, sampler_type=u.MaterialSamplerType.SAMPLERTYPE_COLOR)
    require(LIB.connect_material_expressions(uv, '', sample, 'Coordinates')
            or LIB.connect_material_expressions(uv, '', sample, 'UVs'), '图集 UV 连线失败')
    alpha = custom(ALPHA_CODE, u.CustomMaterialOutputType.CMOT_FLOAT1, {
        'Alpha': (sample, 'A'), 'Floor': (node(u.MaterialExpressionConstant, r=cfg['opacity_floor']), '')})
    require(LIB.connect_material_property(sample, 'RGB', u.MaterialProperty.MP_EMISSIVE_COLOR), 'RGB 输出失败')
    require(LIB.connect_material_property(alpha, '', u.MaterialProperty.MP_OPACITY), 'Alpha 输出失败')
    LIB.recompile_material(material)
    save(material, registry)
    for state in cfg['states']:
        backup(state['material'])
        instance = ensure_asset(state['material'], u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(instance, material)
        LIB.set_material_instance_texture_parameter_value(instance, 'AtlasTexture', texture)
        require(LIB.get_material_instance_texture_parameter_value(instance, 'AtlasTexture') == texture, '图集参数写入失败')
        for name, value in uv_parameters(cfg, state).items():
            LIB.set_material_instance_scalar_parameter_value(instance, name, value)
            require(abs(LIB.get_material_instance_scalar_parameter_value(instance, name) - value) < 1e-6, name + ' 写入失败')
        LIB.update_material_instance(instance)
        save(instance, registry)
    manifest = {'Files': [{'Source': cfg['source'], 'Destination': cfg['texture'], 'Role': 'Color',
                           'SHA256': cfg['source_sha256']}], 'GPUVerified': False, 'ManualAcceptance': False}
    report('NativeTextureImport.json', manifest)
    return manifest


def texture_settings():
    return dict(srgb=True, compression_settings=u.TextureCompressionSettings.TC_EDITOR_ICON,
                mip_gen_settings=u.TextureMipGenSettings.TMGS_NO_MIPMAPS, lod_group=u.TextureGroup.TEXTUREGROUP_UI,
                compression_no_alpha=False, never_stream=True, adjust_brightness=1., adjust_brightness_curve=1.,
                adjust_saturation=1., adjust_vibrance=0., adjust_rgb_curve=1., adjust_hue=0., use_legacy_gamma=False)


def widget_requests(dry_run=True):
    """仅返回缺失 WBP 的原生创建请求；成功回执必须逐件登记后再继续。"""
    cfg, registry, paths = context()
    preflight(registry, paths)
    require(all(registry['Assets'].get(p, {}).get('ContractSHA256') == digest(CONFIG)
                for p in [cfg['texture'], cfg['material']] + [s['material'] for s in cfg['states']]),
            '请先按当前合同完成资源导入')
    requests = []
    width, height = cfg['display_size']
    for state in cfg['states']:
        path = state['widget']
        if path in registry['Assets'] and native_file(path).exists():
            widget_nodes(cfg, state)
            continue
        request_id = str(uuid.uuid4())
        if not dry_run:
            backup(path)
            registry['Requests'][request_id] = {'Path': path, 'ContractSHA256': digest(CONFIG)}
        spec = {'version': 1, 'name': path.rsplit('/', 1)[1], 'parentClass': cfg['widget_parent'],
                'rootWidget': {'type': 'SizeBox', 'id': 'CursorSize', 'style': {
                    'width': width, 'height': height, 'visibility': 'HitTestInvisible'}, 'children': [{
                        'type': 'Image', 'id': 'CursorImage', 'slot': {'hAlign': 'Fill', 'vAlign': 'Fill'},
                        'style': {'visibility': 'HitTestInvisible'}, 'content': {'brushPath': state['material']}}]}}
        requests.append({'Tool': 'ui_query', 'Arguments': {'action': 'build_ui_from_spec', 'params': {
            'asset_path': path, 'spec': spec, 'mode': 'rebuild',
            'overwrite': False, 'dry_run': bool(dry_run), 'treat_warnings_as_errors': True, 'request_id': request_id}}})
    if not dry_run:
        report('Ownership.json', registry)
    return requests


def record_widget_result(result):
    """消费已核对源码的 PackResponse 形状；传输 envelope 须由调用者先解析。"""
    cfg, registry, _ = context()
    pending = registry['Requests'].get(result.get('request_id'))
    require(pending and pending['ContractSHA256'] == digest(CONFIG), '没有匹配的作者创建请求')
    require(result.get('bSuccess') is True and result.get('error_count') == 0
            and result.get('warning_count') == 0 and result.get('asset_path') == pending['Path']
            and not result.get('diff', {}).get('dry_run', False), 'Monolith 创建动作未真实成功')
    require(pending['Path'] not in {str(p.get_name()) for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()},
            '成功回执之后目标仍有未保存修改，拒绝登记')
    if pending['Path'] in registry['Assets']:
        require(registry['Assets'][pending['Path']]['Files'] == fingerprints(pending['Path']), '重复回执的资产已被外部改动')
    state = next(s for s in cfg['states'] if s['widget'] == pending['Path'])
    widget_nodes(cfg, state)
    register(state['widget'], registry)
    del registry['Requests'][result['request_id']]
    report('Ownership.json', registry)


def widget_nodes(cfg, state):
    bp = u.load_asset(state['widget'])
    require(isinstance(bp, u.WidgetBlueprint), '不是原生 WidgetBlueprint：' + state['widget'])
    require(u.BlueprintEditorLibrary.get_blueprint_parent_class(bp) == u.load_class(None, cfg['widget_parent']),
            '光标未继承合同平台父类')
    tree = u.find_object(bp, 'WidgetTree')
    require(tree is not None, '缺少原生 WidgetTree')
    nodes = {w.get_name(): w for w in u.ObjectIterator(u.Widget) if w.get_outer() == tree}
    require(set(nodes) == {'CursorSize', 'CursorImage'} and isinstance(nodes['CursorSize'], u.SizeBox)
            and isinstance(nodes['CursorImage'], u.Image), '控件树不是本合同两节点结构')
    size, image = nodes['CursorSize'], nodes['CursorImage']
    require(size.get_parent() is None and image.get_parent() == size
            and size.get_children_count() == 1, '光标根容器或父子关系错误')
    require(image.get_editor_property('brush').get_editor_property('resource_object') == u.load_asset(state['material']),
            '光标画刷未绑定本状态材质')
    return bp, size, image


def hotspot(cfg, state):
    return [cfg['display_size'][i] * (.5 - state['hotspot_px'][i] / state['rect_px'][i + 2]) for i in range(2)]


def finalize_widgets():
    """十个 WBP 全部登记且控件树通过后，统一写入尺寸、白色乘色和热点。"""
    cfg, registry, paths = context()
    preflight(registry, paths)
    require(all(s['widget'] in registry['Assets'] for s in cfg['states']), '十个 WBP 尚未全部登记')
    widgets = [(state, widget_nodes(cfg, state)) for state in cfg['states']]
    width, height = cfg['display_size']
    for state, (bp, size, image) in widgets:
        backup(state['widget'])
        size.set_width_override(float(width))
        size.set_height_override(float(height))
        for widget in (size, image):
            widget.set_visibility(u.SlateVisibility.HIT_TEST_INVISIBLE)
            widget.set_render_opacity(1.)
        image.set_color_and_opacity(u.LinearColor(1, 1, 1, 1))
        brush = image.get_editor_property('brush')
        brush.set_editor_property('draw_as', u.SlateBrushDrawType.IMAGE)
        brush_size = brush.get_editor_property('image_size')
        brush_size.import_text(f'(X={width},Y={height})')
        brush.set_editor_property('image_size', brush_size)
        tint = brush.get_editor_property('tint_color')
        tint.set_editor_property('specified_color', u.LinearColor(1, 1, 1, 1))
        brush.set_editor_property('tint_color', tint)
        image.set_brush(brush)
        image.set_render_translation(u.Vector2D(*hotspot(cfg, state)))
        require(u.BlueprintEditorLibrary.compile_blueprint(bp), '光标蓝图编译失败：' + state['widget'])
        save(bp, registry)
    return verify_native()


def verify_native():
    """只读真实资源和原生包；不声称重启重载、GPU、Cook、运行绑定或人工签收。"""
    cfg, registry, paths = context()
    result = {'NativeReadback': False, 'GPUVerified': False, 'CookVerified': False,
              'IndependentReloadVerified': False, 'RuntimeBound': False, 'ManualAcceptance': False,
              'ContractSHA256': digest(CONFIG), 'Assets': []}
    try:
        preflight(registry, paths)
        require(all(registry['Assets'].get(p, {}).get('ContractSHA256') == digest(CONFIG) for p in paths),
                '存在尚未按当前合同制作/登记的原生资产')
        texture = u.load_asset(cfg['texture'])
        require(isinstance(texture, u.Texture2D), '图集类型错误')
        for key, expected in texture_settings().items():
            require(texture.get_editor_property(key) == expected, '纹理设置不符：' + key)
        require([texture.blueprint_get_size_x(), texture.blueprint_get_size_y()] == cfg['source_size'], '图集尺寸不符')
        material = u.load_asset(cfg['material'])
        require(material.get_editor_property('material_domain') == u.MaterialDomain.MD_UI
                and material.get_editor_property('blend_mode') == u.BlendMode.BLEND_TRANSLUCENT, '母材质域或混合模式错误')
        expressions = LIB.get_material_expressions(material)
        samples = [e for e in expressions if isinstance(e, u.MaterialExpressionTextureSampleParameter2D)]
        codes = {str(e.get_editor_property('code')): e for e in expressions if isinstance(e, u.MaterialExpressionCustom)}
        constants = [e for e in expressions if isinstance(e, u.MaterialExpressionConstant)]
        scalars = {str(e.get_editor_property('parameter_name')): e for e in expressions
                   if isinstance(e, u.MaterialExpressionScalarParameter)}
        require(len(expressions) == 9 and len(samples) == 1 and set(codes) == {UV_CODE, ALPHA_CODE}
                and set(scalars) == {'UVScaleX', 'UVScaleY', 'UVOffsetX', 'UVOffsetY'}
                and len(constants) == 1 and abs(constants[0].get_editor_property('r') - cfg['opacity_floor']) < 1e-6,
                '母材质采样、UV 参数或 Alpha 阈值节点不符')
        sample = samples[0]
        require(str(sample.get_editor_property('parameter_name')) == 'AtlasTexture'
                and sample.get_editor_property('texture') == texture
                and sample.get_editor_property('sampler_type') == u.MaterialSamplerType.SAMPLERTYPE_COLOR,
                '母材质图集采样合同不符')
        require(LIB.get_material_property_input_node(material, u.MaterialProperty.MP_EMISSIVE_COLOR) == sample
                and LIB.get_material_property_input_node_output_name(material, u.MaterialProperty.MP_EMISSIVE_COLOR) == 'RGB'
                and LIB.get_material_property_input_node(material, u.MaterialProperty.MP_OPACITY) == codes[ALPHA_CODE],
                '母材质 RGB/Alpha 输出不符')
        uv_inputs = list(LIB.get_inputs_for_material_expression(material, codes[UV_CODE]))
        require(list(LIB.get_material_expression_input_names(codes[UV_CODE]))
                == ['UV', 'UVScaleX', 'UVScaleY', 'UVOffsetX', 'UVOffsetY'] and len(uv_inputs) == 5
                and isinstance(uv_inputs[0], u.MaterialExpressionTextureCoordinate)
                and uv_inputs[1:] == [scalars[n] for n in ('UVScaleX', 'UVScaleY', 'UVOffsetX', 'UVOffsetY')]
                and LIB.get_inputs_for_material_expression(material, sample)[0] == codes[UV_CODE]
                and list(LIB.get_inputs_for_material_expression(material, codes[ALPHA_CODE])) == [sample, constants[0]]
                and LIB.get_input_node_output_name_for_material_expression(codes[ALPHA_CODE], sample) == 'A',
                '母材质 UV 或 Alpha 输入连接不符')
        width, height = cfg['display_size']
        for state in cfg['states']:
            instance = u.load_asset(state['material'])
            require(isinstance(instance, u.MaterialInstanceConstant) and instance.get_editor_property('parent') == material,
                    '材质实例父类错误')
            require(LIB.get_material_instance_texture_parameter_value(instance, 'AtlasTexture') == texture, '实例图集错误')
            for name, value in uv_parameters(cfg, state).items():
                require(abs(LIB.get_material_instance_scalar_parameter_value(instance, name) - value) < 1e-6, name + ' 回读错误')
            bp, size, image = widget_nodes(cfg, state)
            require(size.get_editor_property('bOverride_WidthOverride') and size.get_editor_property('bOverride_HeightOverride')
                    and size.get_editor_property('width_override') == width
                    and size.get_editor_property('height_override') == height, '控件尺寸不符')
            cls = u.EditorAssetLibrary.load_blueprint_class(state['widget'])
            require(cls is not None and not u.get_default_object(cls).get_editor_property('window_drag').get_editor_property('enabled'),
                    '光标不应启用平台窗口拖动')
            for widget in (size, image):
                require(widget.get_visibility() == u.SlateVisibility.HIT_TEST_INVISIBLE
                        and widget.get_render_opacity() == 1., '光标命中测试或透明度不符')
            brush = image.get_editor_property('brush')
            brush_size = brush.get_editor_property('image_size')
            expected_size = brush_size.copy()
            expected_size.import_text(f'(X={width},Y={height})')
            require(image.get_editor_property('color_and_opacity') == u.LinearColor(1, 1, 1, 1)
                    and brush.get_editor_property('tint_color').get_editor_property('specified_color') == u.LinearColor(1, 1, 1, 1)
                    and brush.get_editor_property('draw_as') == u.SlateBrushDrawType.IMAGE
                    and brush_size.export_text() == expected_size.export_text(),
                    '光标白色乘色、画刷尺寸或绘制方式不符')
            translation = image.get_editor_property('render_transform').translation
            require(all(abs(a - b) < 1e-4 for a, b in zip((translation.x, translation.y), hotspot(cfg, state))), '热点位移不符')
        result['Assets'] = [{'Path': p, 'Files': fingerprints(p)} for p in paths]
        result['NativeReadback'] = True
    except Exception as error:
        result['Error'] = str(error)
        raise
    finally:
        report('NativeVerification.json', result)
    return result
