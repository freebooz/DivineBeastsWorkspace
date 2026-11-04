"""SSLM 唯一共享弹窗背景作者。只通过已交接的正式 UE5.8 Monolith 执行。

候选阶段保留在 Saved；发布时安装到 Tools/Unreal/UI，资源写入 DBAUIPack_Core。
不创建业务逻辑、不操作人工客户端、不改账号/服务器、不覆盖其他任务按钮和头像。
"""
from pathlib import Path
import json, hashlib, shutil, os
import unreal as u

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PopupPanel20261103'
CFG=json.loads(Path(__file__).with_name('PopupPanel.json').read_text(encoding='utf-8-sig'))
LIB=u.MaterialEditingLibrary
PACK=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core'

def report(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def context():
    assert Path(u.Paths.convert_relative_path_to_full(u.Paths.get_project_file_path())).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
    assert u.SystemLibrary.get_engine_version().startswith('5.8.')
    lease=json.loads((OUT/'EditorLease.json').read_text(encoding='utf-8-sig'))
    assert lease['pid']==os.getpid() and lease['write_authorized'], '尚未取得此编辑器独占租约'

def save(asset):
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),asset.get_path_name()

def ensure(path,kind,factory):
    if u.EditorAssetLibrary.does_asset_exist(path):
        asset=u.load_asset(path)
        assert isinstance(asset,kind),path
        return asset
    result=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],kind,factory)
    assert result,path
    return result

def native_file(path):
    mount,relative=path.lstrip('/').split('/',1)
    if mount=='DBAUIPack_Core':base=PACK/'Content'
    elif mount=='DBAArena':base=ROOT/'Game/Plugins/DivineBeasts/DBAArena/Content'
    elif mount=='Game':base=ROOT/'Game/Content'
    else:raise AssertionError(path)
    return base/(relative.split('.')[0]+'.uasset')

def backup(paths):
    context()
    entries=[]
    for path in sorted(set(paths)):
        f=native_file(path)
        if not f.is_file():continue
        rel=f.relative_to(ROOT)
        dest=OUT/'Backups'/rel
        if not dest.exists():
            dest.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(f,dest)
        entries.append({'Asset':path,'Path':str(rel),'BeforeSHA256':hashlib.sha256(dest.read_bytes()).hexdigest()})
    report('Backups.json',entries)

def textures():
    context()
    source=ROOT/CFG['source']
    assert source.is_file(),source
    records=[]
    for mask in (False,True):
        path=CFG['alpha_texture' if mask else 'color_texture']
        task=u.AssetImportTask()
        for key,val in {'filename':str(source),'destination_path':path.rsplit('/',1)[0],
                        'destination_name':path.rsplit('/',1)[1],'replace_existing':True,
                        'automated':True,'save':False}.items():task.set_editor_property(key,val)
        u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        tex=u.load_asset(path);assert isinstance(tex,u.Texture2D),path
        for key,val in {'srgb':not mask,'compression_settings':u.TextureCompressionSettings.TC_EDITOR_ICON,
                        'mip_gen_settings':u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                        'lod_group':u.TextureGroup.TEXTUREGROUP_UI,'compression_no_alpha':False,
                        'never_stream':True,'adjust_brightness':1.0,'adjust_brightness_curve':1.0,
                        'adjust_saturation':1.0,'adjust_rgb_curve':1.0,'adjust_hue':0.0,
                        'adjust_vibrance':0.0,'use_legacy_gamma':False}.items():tex.set_editor_property(key,val)
        save(tex)
        records.append({'Source':CFG['source'],'Destination':path,'Role':'AlphaMask' if mask else 'Color',
                        'SHA256':hashlib.sha256(source.read_bytes()).hexdigest(),
                        'Size':[tex.blueprint_get_size_x(),tex.blueprint_get_size_y()], 'SRGB':not mask})
    report('NativeTextures.json',records)

def material_and_style():
    context()
    material=u.load_asset(CFG['material']);assert isinstance(material,u.Material)
    material.set_editor_property('material_domain',u.MaterialDomain.MD_UI)
    material.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT)
    LIB.delete_all_material_expressions(material)
    nodes={}
    inputs=[]
    for name,mask in [('PanelColor',False),('PanelAlpha',True)]:
        node=LIB.create_material_expression(material,u.MaterialExpressionTextureObjectParameter,-500,200*len(nodes))
        node.set_editor_property('parameter_name',name)
        node.set_editor_property('texture',u.load_asset(CFG['alpha_texture' if mask else 'color_texture']))
        node.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if mask else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        nodes[name]=node
    nodes['UV']=LIB.create_material_expression(material,u.MaterialExpressionTextureCoordinate,-500,450)
    for name,xy,y in [('SourceUVMin',CFG['source_uv_rect'][:2],650),
                      ('SourceUVMax',CFG['source_uv_rect'][2:],800)]:
        rect=LIB.create_material_expression(material,u.MaterialExpressionVectorParameter,-500,y)
        rect.set_editor_property('parameter_name',name)
        rect.set_editor_property('default_value',u.LinearColor(xy[0],xy[1],0,1))
        nodes[name]=rect
    custom=LIB.create_material_expression(material,u.MaterialExpressionCustom,0,0)
    custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('description','当前木框底板：原RGB直通，九宫格由Slate负责，内衬实心；旧资源身份仅为兼容引用。')
    custom.set_editor_property('code', '''float2 P=lerp(SourceUVMin.rg,SourceUVMax.rg,UV);
float3 RGB=Texture2DSample(PanelColor,PanelColorSampler,P).rgb;
float A=Texture2DSample(PanelAlpha,PanelAlphaSampler,P).a;
// 生成PNG内衬含极轻微Alpha抖动：按实心窗口合同在轮廓内部显式覆盖，保留外缘抗锯齿。
float Solid=step(.035,UV.x)*step(UV.x,.965)*step(.025,UV.y)*step(UV.y,.975);
return float4(RGB,max(A,Solid));''')
    for name in nodes:
        ci=u.CustomInput();ci.set_editor_property('input_name',name);inputs.append(ci)
    custom.set_editor_property('inputs',inputs)
    for name,node in nodes.items():assert LIB.connect_material_expressions(node,'',custom,name),name
    alpha=LIB.create_material_expression(material,u.MaterialExpressionComponentMask,300,180)
    for c in ('r','g','b'):alpha.set_editor_property(c,False)
    alpha.set_editor_property('a',True)
    assert LIB.connect_material_expressions(custom,'',alpha,'')
    assert LIB.connect_material_property(custom,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert LIB.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(material);save(material)
    style_class=u.EditorAssetLibrary.load_blueprint_class(CFG['style']);assert style_class
    cdo=u.get_default_object(style_class)
    brush=cdo.get_editor_property('background')
    brush.set_editor_property('resource_object',material)
    brush.set_editor_property('draw_as',u.SlateBrushDrawType.BOX)
    brush.set_editor_property('image_type',u.SlateBrushImageType.FULL_COLOR)
    brush.set_editor_property('margin',u.Margin(*CFG['margin']))
    size=brush.get_editor_property('image_size')
    size.set_editor_property('x',float(CFG['image_size'][0]));size.set_editor_property('y',float(CFG['image_size'][1]))
    brush.set_editor_property('image_size',size)
    tint=brush.get_editor_property('tint_color');tint.set_editor_property('specified_color',u.LinearColor(1,1,1,1))
    brush.set_editor_property('tint_color',tint)
    cdo.set_editor_property('background',brush)
    bp=u.load_asset(CFG['style']);u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    report('NativeSharedStyle.json',{'Style':CFG['style'],'Material':CFG['material'],
                                   'ImageSize':CFG['image_size'],'Margin':CFG['margin'],
                                   'ColorTexture':CFG['color_texture'],'AlphaTexture':CFG['alpha_texture'],
                                   'SourceUVRect':CFG['source_uv_rect'],'RGBAdjustments':False})

def register_color_sources():
    context()
    path=PACK/'Docs/UIColorImportManifest.json'
    data=json.loads(path.read_text(encoding='utf-8-sig'))
    entries={row['Destination']:row for row in data['Files']}
    assert len(entries)==len(data['Files'])
    for row in json.loads((OUT/'NativeTextures.json').read_text(encoding='utf-8')):
        entry={key:row[key] for key in ('Source','Destination','Role','SHA256')}
        entry['DescriptionZh']='共享木框弹窗；RGB原色与线性A分开导入，内衬按实心窗口合同处理。'
        entries[row['Destination']]=entry
    data['Files']=list(entries.values())
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def verify_shared():
    context()
    cdo=u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(CFG['style']))
    brush=cdo.get_editor_property('background')
    assert brush.get_editor_property('draw_as')==u.SlateBrushDrawType.BOX,brush
    assert brush.get_editor_property('resource_object').get_path_name().split('.')[0]==CFG['material']
    for mask in (False,True):
        tex=u.load_asset(CFG['alpha_texture' if mask else 'color_texture'])
        assert tex.get_editor_property('srgb')== (not mask)
        assert tex.get_editor_property('compression_settings')==u.TextureCompressionSettings.TC_EDITOR_ICON
    report('NativeSharedVerified.json',{'Passed':True,'Style':CFG['style'],
                                       'DirtyContent':[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]})
    print('POPUP_SHARED_NATIVE_VERIFIED')

def linear(rgb):
    def channel(x):
        v=x/255.0
        return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
    return u.LinearColor(*(channel(v) for v in rgb),1)

def text_styles():
    """浅色纸面语义在现有主题中声明，不更改登录/HUD/按钮所用的全局正文色。"""
    context()
    theme=u.load_asset(CFG['theme']);assert theme
    entries=list(theme.get_editor_property('texts'))
    base=next(e for e in entries if str(e.get_editor_property('rule').get_editor_property('style_id'))=='UI.Style.Text.Body')
    records=[]
    for role,rgb in [('Body',CFG['body_srgb']),('Error',CFG['error_srgb']),('Heading',CFG['heading_srgb'])]:
        style_id='UI.Style.Text.Panel'+role
        path='/DBAUIPack_Core/UI/Styles/BP_DBA_TextStyle_Panel'+role
        if not u.EditorAssetLibrary.does_asset_exist(path):
            assert u.EditorAssetLibrary.duplicate_asset('/DBAUIPack_Core/UI/Styles/BP_DBA_TextStyle_Body',path)
        bp=u.load_asset(path)
        cls=u.EditorAssetLibrary.load_blueprint_class(path);assert cls
        cdo=u.get_default_object(cls)
        cdo.set_editor_property('color',linear(rgb))
        cdo.set_editor_property('uses_drop_shadow',False)
        u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
        cls=u.EditorAssetLibrary.load_blueprint_class(path)
        entry=base.copy()
        rule=entry.get_editor_property('rule').copy()
        rule.set_editor_property('style_id',style_id)
        entry.set_editor_property('rule',rule)
        entry.set_editor_property('style_class',cls)
        entries=[e for e in entries if str(e.get_editor_property('rule').get_editor_property('style_id'))!=style_id]
        entries.append(entry)
        records.append({'StyleId':style_id,'Asset':path,'sRGB':rgb})
    theme.set_editor_property('texts',entries);save(theme)
    report('NativeTextStyles.json',records)

def nodes(bp):
    prefix=bp.get_path_name()+':WidgetTree.'
    return {w.get_name():w for w in u.ObjectIterator(u.Widget) if w.get_path_name().startswith(prefix)}

def contract(bp,background_name):
    """仅记录本任务不能改变的真实控件合同；不把内存地址误当布局差异。"""
    result=[]
    for name,w in sorted(nodes(bp).items()):
        row={'Name':name,'Class':w.get_class().get_name(),'Parent':w.get_parent().get_name() if w.get_parent() else None,
             'Enabled':w.get_is_enabled(),'Opacity':w.get_render_opacity()}
        if name!=background_name:row['Visibility']=str(w.get_visibility())
        if isinstance(w,u.TextBlock):row['FontSize']=w.get_editor_property('font').get_editor_property('size')
        if isinstance(w.slot,u.CanvasPanelSlot):
            s=w.slot;a=s.get_anchors();p=s.get_position();size=s.get_size();align=s.get_alignment()
            row['Canvas']={'Anchors':[a.minimum.x,a.minimum.y,a.maximum.x,a.maximum.y],
                           'Position':[p.x,p.y],'Size':[size.x,size.y],'Alignment':[align.x,align.y],
                           'AutoSize':s.get_auto_size(),'ZOrder':s.get_z_order()}
        result.append(row)
    return result

def assign_binding(bindings,name,key,kind):
    matches=[i for i,b in enumerate(bindings) if str(b.get_editor_property('widget_name'))==name]
    assert len(matches)<=1,(name,matches)
    binding=bindings[matches[0]].copy() if matches else u.GamePlatformUIWidgetStyleBinding()
    binding.set_editor_property('widget_name',name)
    binding.set_editor_property('style_id',key)
    binding.set_editor_property('kind',kind)
    binding.set_editor_property('allow_parent_fallback',False)
    binding.set_editor_property('preserve_font_size',True)
    binding.set_editor_property('optional',True)
    if matches:bindings[matches[0]]=binding
    else:bindings.append(binding)

def prepare_surface(bp,item):
    found=nodes(bp)
    if 'create_background' in item:
        tree=u.find_object(bp,'WidgetTree');spec=item['create_background']
        if item['background'] not in found:
            bg=u.new_object(u.CommonBorder,outer=tree,name=item['background'])
            slot=found[spec['parent']].add_child_to_canvas(bg)
            slot.set_anchors(u.Anchors(minimum=u.Vector2D(.5,.5),maximum=u.Vector2D(.5,.5)))
            slot.set_alignment(u.Vector2D(.5,.5));slot.set_position(u.Vector2D(*spec['position']))
            slot.set_size(u.Vector2D(*spec['size']));slot.set_z_order(1)
        for name in spec['raise']:found[name].slot.set_z_order(2)
        found=nodes(bp)
    for name,pos in item.get('canvas_positions',{}).items():
        assert isinstance(found[name].slot,u.CanvasPanelSlot)
        found[name].slot.set_position(u.Vector2D(*pos))
    return found


def surfaces(assets=None):
    """仅执行明确盘点后的逐窗清单。保留父类、控件身份、尺寸、按钮与业务事件。"""
    context()
    assert CFG['surfaces'],'必须先用真实 WidgetTree 确定覆盖清单'
    style_class=u.EditorAssetLibrary.load_blueprint_class(CFG['style']);assert style_class
    brush=u.get_default_object(style_class).get_editor_property('background')
    records=[]
    for item in CFG['surfaces']:
        if assets is not None and item['asset'] not in assets:continue
        bp=u.load_asset(item['asset']);assert isinstance(bp,u.WidgetBlueprint)
        native_nodes=prepare_surface(bp,item)
        bg=native_nodes[item['background']]
        before=contract(bp,item['background'])
        cls=u.EditorAssetLibrary.load_blueprint_class(item['asset']);cdo=u.get_default_object(cls)
        bindings=list(cdo.get_editor_property('theme_bindings'))
        if isinstance(bg,u.Border):
            if 'content_padding' in item:bg.set_padding(u.Margin(*item['content_padding']))
            if isinstance(bg,u.CommonBorder):bg.set_style(style_class)
            bg.set_editor_property('background',brush)
            bg.set_brush_color(u.LinearColor(1,1,1,1))
            assign_binding(bindings,bg.get_name(),'UI.Style.Border.Panel',u.GamePlatformUIStyleKind.BORDER)
            # 边框若包含真实内容，不得使其子控件失去命中；装饰本身不吃鼠标。
            bg.set_visibility(u.SlateVisibility.SELF_HIT_TEST_INVISIBLE if bg.get_children_count() else u.SlateVisibility.HIT_TEST_INVISIBLE)
        elif isinstance(bg,u.Image):
            bg.set_editor_property('brush',brush)
            bg.set_color_and_opacity(u.LinearColor(1,1,1,1))
            bg.set_visibility(u.SlateVisibility.HIT_TEST_INVISIBLE)
            if isinstance(bg.slot,u.OverlaySlot):
                bg.slot.set_horizontal_alignment(u.HorizontalAlignment.H_ALIGN_FILL)
                bg.slot.set_vertical_alignment(u.VerticalAlignment.V_ALIGN_FILL)
        else:raise AssertionError((item['asset'],bg.get_class().get_name()))
        for name in item.get('clear_fills',[]):native_nodes[name].set_brush_color(u.LinearColor(1,1,1,0))
        for role in ('body','error','heading'):
            for name in item.get(role,[]):
                text=native_nodes[name];assert isinstance(text,u.TextBlock)
                parent=text.get_parent()
                while parent:
                    assert not isinstance(parent,u.Button),'背景优化不得改按钮标签: '+name
                    parent=parent.get_parent()
                font=text.get_editor_property('font')
                if isinstance(text,u.CommonTextBlock):text.set_style(None)
                text.set_font(font)
                text.set_color_and_opacity(u.SlateColor(specified_color=linear(CFG[role+'_srgb'])))
                text.set_shadow_offset(u.Vector2D(0,0))
                text.set_shadow_color_and_opacity(u.LinearColor(0,0,0,0))
                assign_binding(bindings,name,'UI.Style.Text.Panel'+role.capitalize(),u.GamePlatformUIStyleKind.TEXT)
        cdo.set_editor_property('theme_bindings',bindings)
        assert contract(bp,item['background'])==before,'改变了控件布局/字号/输入合同: '+item['asset']
        u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
        assert contract(bp,item['background'])==before,'编译改变了控件合同: '+item['asset']
        records.append({'Asset':item['asset'],'Background':item['background'],'Class':bg.get_class().get_name(),
                        'Body':item.get('body',[]),'Error':item.get('error',[]),'Contract':before,'ContractUnchanged':True})
    if assets is not None and (OUT/'NativeSurfaces.json').exists():
        previous=json.loads((OUT/'NativeSurfaces.json').read_text(encoding='utf-8'))
        changed={r['Asset'] for r in records}
        records=[r for r in previous if r['Asset'] not in changed]+records
    report('NativeSurfaces.json',records)
    print('POPUP_SURFACES_SAVED',len(records))
