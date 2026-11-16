"""通过正式Monolith执行当前头像栏作者；仅修改指定第三层UI资产，无业务状态或网络权威。

materials阶段导入已冻结原稿并创建/更新同源UI材质；layout阶段调整现有Widget树。
从旧树重建须先按CompactPlayerFrame.json的TreeMigration经Monolith ui_query迁移结构；
作者不直接伪造WidgetTree或资产字节。已迁移树仅需materials/layout，执行前须取得编辑器租约。
编译、保存、冷重载及真实渲染由调用方分阶段验证，不以脚本运行成功代替验收。
"""
from pathlib import Path
import hashlib, json
import unreal as u

ROOT=Path(__file__).resolve().parents[3]
CONFIG=json.loads(Path(__file__).with_name('CompactPlayerFrame.json').read_text(encoding='utf-8'))
OUT=ROOT/'Saved/Validation/PlayerFrame20261103'
PACK=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core'
LIB=u.MaterialEditingLibrary

def context():
    """只接受本轮专属会话与正式工程，拒绝误连其它任务。"""
    assert Path(u.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
    assert '-PlayerFrame20261103' in u.SystemLibrary.get_command_line()

def save_report(name,data):
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

def materials():
    """RGB/Alpha分离角色导入；两源文件均保持附件字节，不对原图做破坏性处理。"""
    context(); records=[]
    for suffix,asset,srgb in [('',CONFIG['ColorTexture'],True),('Matte',CONFIG['AlphaTexture'],False)]:
        source=PACK/'SourceArt/UI/Nature'/('NaturePlayerFrameReference'+suffix+'.png')
        assert hashlib.sha256(source.read_bytes()).hexdigest()==CONFIG['SourceSHA256']
        task=u.AssetImportTask()
        for key,value in {'filename':str(source),'destination_path':asset.rsplit('/',1)[0],
            'destination_name':asset.rsplit('/',1)[1],'replace_existing':True,'automated':True,'save':False}.items():
            task.set_editor_property(key,value)
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture=u.load_asset(asset); assert isinstance(texture,u.Texture2D)
        for key,value in {'srgb':srgb,'compression_settings':u.TextureCompressionSettings.TC_EDITOR_ICON,
            'mip_gen_settings':u.TextureMipGenSettings.TMGS_NO_MIPMAPS,'lod_group':u.TextureGroup.TEXTUREGROUP_UI,
            'compression_no_alpha':False,'never_stream':True,'adjust_brightness':1.0,'adjust_brightness_curve':1.0,
            'adjust_vibrance':0.0,'adjust_saturation':1.0,'adjust_rgb_curve':1.0,'adjust_hue':0.0,'use_legacy_gamma':False}.items():
            texture.set_editor_property(key,value)
        assert u.EditorAssetLibrary.save_loaded_asset(texture,False)
        records.append({'Asset':asset,'SRGB':texture.get_editor_property('srgb'),'Size':[texture.blueprint_get_size_x(),texture.blueprint_get_size_y()],'SourceSHA256':CONFIG['SourceSHA256']})
    code=Path(__file__).with_name('CompactPlayerFrame.hlsl').read_text(encoding='utf-8')
    for layer,path in enumerate(CONFIG['Materials']):
        material=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],u.Material,u.MaterialFactoryNew())
        assert isinstance(material,u.Material)
        material.set_editor_property('material_domain',u.MaterialDomain.MD_UI)
        material.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT)
        # UE5.8批量删除内部边遍历边修改表达式数组；显式遍历快照，避免旧孤立节点残留。
        for expression in list(LIB.get_material_expressions(material)):
            LIB.delete_material_expression(material,expression)
        assert not LIB.get_material_expressions(material)
        values={'UV':LIB.create_material_expression(material,u.MaterialExpressionTextureCoordinate,-600,0)}
        for index,(name,asset,srgb) in enumerate([('Source',CONFIG['ColorTexture'],True),('Matte',CONFIG['AlphaTexture'],False)]):
            node=LIB.create_material_expression(material,u.MaterialExpressionTextureObject,-600,140+index*140)
            node.set_editor_property('texture',u.load_asset(asset))
            node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_COLOR if srgb else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
            values[name]=node
        node=LIB.create_material_expression(material,u.MaterialExpressionScalarParameter,-600,420)
        node.set_editor_property('parameter_name','Layer');node.set_editor_property('default_value',float(layer));values['Layer']=node
        custom=LIB.create_material_expression(material,u.MaterialExpressionCustom,-150,0)
        custom.set_editor_property('code',code);custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
        pins=[]
        for name in values:
            pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
        custom.set_editor_property('inputs',pins)
        for name,value in values.items():assert LIB.connect_material_expressions(value,'',custom,name)
        alpha=LIB.create_material_expression(material,u.MaterialExpressionComponentMask,80,160)
        for channel in ('r','g','b'):alpha.set_editor_property(channel,False)
        alpha.set_editor_property('a',True)
        assert LIB.connect_material_expressions(custom,'',alpha,'')
        assert LIB.connect_material_property(custom,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        assert LIB.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
        LIB.recompile_material(material)
        assert u.EditorAssetLibrary.save_loaded_asset(material,False)
        records.append({'Material':path,'Layer':layer,'ShaderSHA256':hashlib.sha256(code.encode()).hexdigest()})
    save_report('Materials.Authored.json',records)
    print('PLAYER_FRAME_MATERIALS_SAVED',len(records))

def widget(name,combat=False):
    """返回原生蓝图及现有WidgetTree，保留原父类与事件绑定。"""
    path='/DBAUIPack_Core/UI/'+('Combat/' if combat else 'Components/')+'WBP_DBA_UI_'+name
    bp=u.load_asset(path);assert bp
    tree=u.find_object(bp,'WidgetTree');assert tree
    return bp,tree

def rect(w,bounds,z=0,stretch=False):
    slot=w.slot;assert isinstance(slot,u.CanvasPanelSlot)
    slot.set_anchors(u.Anchors(minimum=u.Vector2D(0,0),maximum=u.Vector2D(1 if stretch else 0,1 if stretch else 0)))
    slot.set_alignment(u.Vector2D(0,0));slot.set_auto_size(False)
    slot.set_offsets(u.Margin(left=bounds[0],top=bounds[1],right=bounds[2],bottom=bounds[3]))
    slot.set_z_order(z)

def image_brush(w,material):
    w.set_brush_from_material(u.load_asset(material))
    w.set_color_and_opacity(u.LinearColor(1,1,1,1))
    w.set_visibility(u.SlateVisibility.HIT_TEST_INVISIBLE)

def layout():
    """精确写入WoW逻辑尺寸，不更改DPI、全局文字缩放或服务器属性。"""
    context();changed=[]
    bp,tree=widget('PlayerFramePortrait')
    rect(u.find_object(tree,'PlayerFrameArt'),CONFIG['Frame'],2)
    image_brush(u.find_object(tree,'PlayerFrameArt'),CONFIG['Materials'][0])
    # 唯一可见头像直接复用登录角色身份事件管理的PortraitImage；SetBrushFromTexture保留RoundedBox样式。
    # 原生半径32的圆角画刷裁成64×64圆形，既有RenderPortrait负责真实纹理和头脸UV，不新增播放器。
    portrait=u.find_object(tree,'PortraitImage');assert portrait
    rect(portrait,CONFIG['Portrait'],1)
    brush=portrait.get_editor_property('brush')
    brush.set_editor_property('resource_object',None)
    brush.set_editor_property('draw_as',u.SlateBrushDrawType.ROUNDED_BOX)
    brush.set_editor_property('tint_color',u.SlateColor(specified_color=u.LinearColor(1,1,1,1)))
    outline=brush.get_editor_property('outline_settings')
    outline.set_editor_property('corner_radii',u.Vector4(32,32,32,32))
    outline.set_editor_property('rounding_type',u.SlateBrushRoundingType.FIXED_RADIUS)
    outline.set_editor_property('width',0.0)
    brush.set_editor_property('outline_settings',outline)
    portrait.set_brush(brush);portrait.set_color_and_opacity(u.LinearColor(1,1,1,1))
    portrait.set_visibility(u.SlateVisibility.HIDDEN)
    # 图腾/未知等级保留已有解绑合同但不显示；主头像不在此容器内。
    compatibility=u.find_object(tree,'PortraitCompatibility');assert compatibility
    compatibility.set_visibility(u.SlateVisibility.COLLAPSED)
    name=u.find_object(tree,'PlayerName');rect(name,CONFIG['Name'],4)
    name.set_text('');name.set_editor_property('justification',u.TextJustify.CENTER)
    font=name.get_editor_property('font');font.set_editor_property('size',CONFIG['FontSize']);name.set_font(font)
    name.set_shadow_offset(u.Vector2D(1,1));name.set_shadow_color_and_opacity(u.LinearColor(0,0,0,.75))
    changed.append(bp)
    for index,kind in enumerate(['Health','Energy']):
        bp,tree=widget('PlayerFrame'+kind+'Bar')
        progress=u.find_object(tree,'ProgressBar');rect(progress,[0,0,0,0],2,True)
        style=progress.get_editor_property('widget_style')
        back=style.get_editor_property('background_image');back.set_editor_property('draw_as',u.SlateBrushDrawType.NO_DRAW_TYPE);style.set_editor_property('background_image',back)
        fill=style.get_editor_property('fill_image');fill.set_editor_property('resource_object',u.load_asset(CONFIG['Materials'][index+1]))
        fill.set_editor_property('draw_as',u.SlateBrushDrawType.IMAGE);fill.set_editor_property('margin',u.Margin())
        fill.set_editor_property('tint_color',u.SlateColor(specified_color=u.LinearColor(1,1,1,1)))
        style.set_editor_property('fill_image',fill);style.set_editor_property('enable_fill_animation',False)
        progress.set_editor_property('widget_style',style);progress.set_fill_color_and_opacity(u.LinearColor(1,1,1,1))
        progress.set_percent(0);progress.set_editor_property('bar_fill_style',u.ProgressBarFillStyle.MASK)
        text=u.find_object(tree,'ResourceValueText');rect(text,[0,-2,0,-2],3,True)
        text.set_text('');text.set_editor_property('justification',u.TextJustify.CENTER)
        font=text.get_editor_property('font');font.set_editor_property('size',CONFIG['FontSize']);text.set_font(font)
        text.set_color_and_opacity(u.SlateColor(specified_color=u.LinearColor(1,1,1,1)))
        text.set_shadow_offset(u.Vector2D(1,1));text.set_shadow_color_and_opacity(u.LinearColor(0,0,0,.8))
        changed.append(bp)
    bp,tree=widget('PlayerFrameStatus')
    rect(u.find_object(tree,'HealthBar'),CONFIG['Health'],1)
    rect(u.find_object(tree,'EnergyBar'),CONFIG['Energy'],1)
    changed.append(bp)
    bp,tree=widget('CombatHUD',True)
    for key,z in [('PlayerPortrait',12),('PlayerStatus',20)]:
        rect(u.find_object(tree,key),CONFIG['HUDPosition']+CONFIG['Canvas'],z)
    # 缩小后将旧头像下方状态/队伍的起点同步上移，保留既有隐藏条件和实际业务绑定。
    for key,y in [('StatusEffects',122),('PartyRoster',122)]:
        w=u.find_object(tree,key);p=w.slot.get_position();w.slot.set_position(u.Vector2D(p.x,y))
    changed.append(bp)
    for bp in changed:
        u.BlueprintEditorLibrary.compile_blueprint(bp)
        assert u.EditorAssetLibrary.save_loaded_asset(bp,False)
    save_report('Layout.Authored.json',{'Assets':[x.get_path_name() for x in changed],'Dimensions':CONFIG})
    print('PLAYER_FRAME_LAYOUT_SAVED',len(changed))

if globals().get('PLAYER_FRAME_MODE')=='materials':materials()
elif globals().get('PLAYER_FRAME_MODE')=='layout':layout()
