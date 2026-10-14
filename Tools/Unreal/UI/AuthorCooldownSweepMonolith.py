# -*- coding: utf-8 -*-
"""SSLM公共UI顺时针半透明冷却材质作者，由正式UE5.8 Monolith执行。

平台RadialCooldown拥有角度机制，项目材质仅装配方向、时间和50%遮罩视觉。
由GAS事件一次设置本实例截止秒数，GPU使用Slate RealTime播放，无Widget Tick或网络查询。
本工具只创建缺失的指定MD_UI材质并引擎保存；Widget树由Monolith ui_query单独调整。
失败抛错并停止发布；不直接写uasset，不修改图标RGB或已有通用函数。
"""
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
ASSET = '/DBAUIPack_Core/UI/Materials/Combat/M_DBA_CooldownSweep'


def author():
    """受控创建唯一项目材质；重复执行只读返回已有资产，不重建用户材质图。"""
    import unreal
    if Path(unreal.Paths.get_project_file_path()).resolve() != WORKSPACE / 'Game/DivineBeastsArena.uproject':
        raise RuntimeError('必须使用正式编辑器')
    if '-SpellMomentumRepairOwned' not in unreal.SystemLibrary.get_command_line():
        raise RuntimeError('必须使用本轮受控Monolith编辑器')
    existing = unreal.load_asset(ASSET)
    if existing:
        return existing.get_path_name()
    function = unreal.load_asset('/GamePlatformUI/Materials/Functions/MF_GP_UI_RadialCooldown')
    if not function:
        raise RuntimeError('平台角度函数缺失，禁止另造重复机制')
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_DBA_CooldownSweep', ASSET.rsplit('/', 1)[0], unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('MaterialDomain', unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('BlendMode', unreal.BlendMode.BLEND_TRANSLUCENT)
    lib = unreal.MaterialEditingLibrary

    def node(cls, x, y):
        """创建真实引擎表达式，不拼写资产文本占位。"""
        return lib.create_material_expression(mat, cls, x, y)

    def scalar(name, default, x, y):
        item = node(unreal.MaterialExpressionScalarParameter, x, y)
        item.set_editor_property('ParameterName', name); item.set_editor_property('DefaultValue', default)
        return item

    def custom(code, names, output, x, y):
        item = node(unreal.MaterialExpressionCustom, x, y)
        inputs = []
        for name in names:
            value = unreal.CustomInput(); value.set_editor_property('InputName', name); inputs.append(value)
        item.set_editor_property('Inputs', inputs); item.set_editor_property('Code', code); item.set_editor_property('OutputType', output)
        return item

    uv = node(unreal.MaterialExpressionTextureCoordinate, -1000, -300)
    clock = node(unreal.MaterialExpressionTime, -1000, 100); clock.set_editor_property('bIgnorePause', True)
    ends = scalar('CooldownEndsAt', 0., -1000, 250)
    duration = scalar('CooldownDuration', 1., -1000, 400)
    rotated = custom('return float2(UV.y,1.0-UV.x);', ['UV'], unreal.CustomMaterialOutputType.CMOT_FLOAT2, -700, -300)
    elapsed = custom('return saturate(1.0-max(EndsAt-Clock,0.0)/max(Duration,0.001));', ['EndsAt', 'Clock', 'Duration'], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -700, 100)
    center = node(unreal.MaterialExpressionConstant2Vector, -700, -450); center.set_editor_property('R', .5); center.set_editor_property('G', .5)
    clockwise = node(unreal.MaterialExpressionConstant, -700, -550); clockwise.set_editor_property('R', 1.)
    radial = node(unreal.MaterialExpressionMaterialFunctionCall, -350, -250); radial.set_editor_property('MaterialFunction', function)
    opacity = custom('return (EndsAt>Clock && Duration>0.0) ? (1.0-saturate(ClearedMask))*0.5 : 0.0;', ['ClearedMask', 'EndsAt', 'Clock', 'Duration'], unreal.CustomMaterialOutputType.CMOT_FLOAT1, 0, 50)
    color = node(unreal.MaterialExpressionConstant3Vector, 0, -250); color.set_editor_property('Constant', unreal.LinearColor(.005, .025, .018, 1.))
    connections = [(uv,'',rotated,'UV'),(ends,'',elapsed,'EndsAt'),(clock,'',elapsed,'Clock'),(duration,'',elapsed,'Duration'),
                   (rotated,'',radial,'UV'),(center,'',radial,'CenterUV'),(elapsed,'',radial,'Progress'),(clockwise,'',radial,'Clockwise'),
                   (radial,'Mask',opacity,'ClearedMask'),(ends,'',opacity,'EndsAt'),(clock,'',opacity,'Clock'),(duration,'',opacity,'Duration')]
    for source, output, target, pin in connections:
        if not lib.connect_material_expressions(source, output, target, pin):
            raise RuntimeError('材质连接失败：' + pin)
    if not lib.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR) or not lib.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY):
        raise RuntimeError('最终UI输出连接失败')
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError('材质保存失败')
    return mat.get_path_name()
