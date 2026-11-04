"""Monolith 内运行的只读按钮审计；扫描项目真实控件，比较样式及未改动的交互合同。"""
from pathlib import Path
import json, re
import unreal as u

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Saved/Validation/AllButtons20261104'
STATES = ('normal', 'hovered', 'pressed', 'disabled')

def read(obj, key, default=None):
    """仅用于可选反射字段；必需合同由审计结果和后续断言检查。"""
    try:
        return obj.get_editor_property(key)
    except Exception:
        return default

def stable(value):
    """去掉反射导出中的内存地址，保留字段、单位与资源路径以跨重载比较。"""
    if isinstance(value, u.Object):
        return value.get_path_name()
    return re.sub(r'\(0x[0-9A-Fa-f]+\)', '(address)', str(value))

def brush_info(brush):
    resource = read(brush, 'resource_object')
    return {'Resource':resource.get_path_name().split('.')[0] if resource else None,
            'DrawAs':str(read(brush, 'draw_as')), 'Margin':stable(read(brush,'margin')),
            'ImageSize':stable(read(brush,'image_size')), 'Tint':stable(read(brush,'tint_color'))}

def style_info(style):
    return {key:brush_info(style.get_editor_property(key)) for key in STATES}

def native_nodes(bp):
    prefix = bp.get_path_name() + ':WidgetTree.'
    return sorted((w for w in u.ObjectIterator(u.Widget) if w.get_path_name().startswith(prefix)),
                  key=lambda w:w.get_name())

def scan(label='Before'):
    """审计正式和开发页面；专门展示四状态的审核夹具单列，不改写其测试输入。"""
    actual = Path(u.Paths.convert_relative_path_to_full(u.Paths.get_project_file_path())).resolve()
    assert actual == ROOT / 'Game/DivineBeastsArena.uproject', actual
    registry = u.AssetRegistryHelpers.get_asset_registry()
    registry.wait_for_completion()
    blueprints = [a.get_asset() for a in registry.get_assets_by_class(
        u.TopLevelAssetPath('/Script/UMGEditor','WidgetBlueprint'), True)
        if str(a.package_name).startswith(('/DBA','/Game/'))]
    rows=[]
    for bp in sorted(blueprints,key=lambda x:x.get_path_name()):
        path=bp.get_path_name().split('.')[0]
        cls=u.EditorAssetLibrary.load_blueprint_class(path)
        cdo=u.get_default_object(cls)
        bindings=read(cdo,'theme_bindings')
        bs=[] if bindings is None else [
            {'Widget':str(b.get_editor_property('widget_name')),
             'Style':str(b.get_editor_property('style_id')),
             'Kind':str(b.get_editor_property('kind'))} for b in bindings]
        controls=[]
        nodes=native_nodes(bp)
        for w in nodes:
            if not isinstance(w,(u.Button,u.ComboBoxString,u.CheckBox,u.CommonButtonBase)):
                continue
            kind=w.get_class().get_name()
            style=read(w,'widget_style')
            if isinstance(w,u.ComboBoxString):style=style.get_editor_property('combo_button_style').get_editor_property('button_style')
            visual=style_info(style) if isinstance(w,(u.Button,u.ComboBoxString)) else {
                key:brush_info(style.get_editor_property(key)) for key in
                ('background_image','background_hovered_image','background_pressed_image')}
            record={'Name':w.get_name(),'Class':kind,
                    'Style':visual,
                    'BackgroundColor':stable(read(w,'background_color')),
                    'ContentColor':stable(read(w,'color_and_opacity')),
                    'Contract':{'Parent':w.get_parent().get_name() if w.get_parent() else None,
                                'Enabled':w.get_is_enabled(),'Visibility':str(w.get_visibility()),
                                'RenderOpacity':w.get_render_opacity(),
                                'Properties':{k:stable(read(w,k)) for k in ('is_focusable','click_method','touch_method','press_method','checked_state')},
                                'Slot':{k:stable(read(w.slot,k)) for k in ('layout_data','z_order','auto_size','padding','horizontal_alignment','vertical_alignment','size')} if w.slot else None},
                    'Children':[]}
            for child in nodes:
                p=child.get_parent()
                while p and p!=w:p=p.get_parent()
                if p==w:
                    record['Children'].append({'Name':child.get_name(),'Class':child.get_class().get_name(),
                        'Text':str(child.get_text()) if isinstance(child,u.TextBlock) else None,
                        'Brush':brush_info(child.get_editor_property('brush')) if isinstance(child,u.Image) else None})
            if isinstance(w,u.Button):
                record['Contract']['StyleProperties']={k:stable(read(style,k)) for k in (
                    'normal_padding','pressed_padding','clicked_slate_sound','pressed_slate_sound','hovered_slate_sound')}
            if isinstance(w,u.CheckBox):
                record['Contract']['CheckGlyphs']={k:brush_info(style.get_editor_property(k)) for k in
                    ('unchecked_image','checked_image','undetermined_image')}
            controls.append(record)
        rows.append({'Asset':path,'ParentClass':stable(read(bp,'parent_class')),
                     'SupportsThemeBindings':bindings is not None,'ThemeBindings':bs,
                     'Nodes':[{'Name':n.get_name(),'Class':n.get_class().get_name()} for n in nodes],
                     'Controls':controls})
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/(label+'.json')).write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
    counts={}
    for row in rows:
        for c in row['Controls']:counts[c['Class']]=counts.get(c['Class'],0)+1
    print(json.dumps({'Audit':label,'Widgets':len(rows),'WidgetsWithControls':sum(bool(x['Controls']) for x in rows),
                      'ControlsByClass':counts,'DirtyContent':[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]},ensure_ascii=False))
    return rows

def verify(label='After'):
    """对真实资产验证覆盖、四态及交互/布局保真；不把脚本成功当引擎视觉验收。"""
    config=json.loads(Path(__file__).with_name('ProjectButtonBindings.json').read_text(encoding='utf-8-sig'))
    current={r['Asset']:r for r in scan(label)}
    before={r['Asset']:r for r in json.loads((OUT/'Before.json').read_text(encoding='utf-8'))}
    errors=[];count=0
    for spec in config['Assets']:
        row=current[spec['Asset']];old=before[spec['Asset']]
        if row['Nodes']!=old['Nodes']:errors.append({'Asset':spec['Asset'],'Reason':'Widget tree changed'})
        controls={c['Name']:c for c in row['Controls']};previous={c['Name']:c for c in old['Controls']}
        if set(controls)!={c['Name'] for c in spec['Controls']}:errors.append({'Asset':spec['Asset'],'Reason':'Control coverage mismatch'})
        for c in spec['Controls']:
            count+=1;actual=controls[c['Name']];prior=previous[c['Name']]
            if actual['Contract']!=prior['Contract'] or actual['Children']!=prior['Children']:
                errors.append({'Asset':spec['Asset'],'Control':c['Name'],'Reason':'Layout/input/content changed'})
            keys=zip(STATES,('Normal','Hovered','Pressed','Disabled')) if c['Kind']!='CheckBox' else zip(
                ('background_image','background_hovered_image','background_pressed_image'),('Normal','Hovered','Pressed'))
            for field,state in keys:
                expected='/DBAUIPack_Core/UI/Materials/Buttons/MI_DBA_UI_Button_'+c['Family']+'_'+state
                b=actual['Style'][field]
                if b['Resource']!=expected or 'BOX' not in b['DrawAs']:
                    errors.append({'Asset':spec['Asset'],'Control':c['Name'],'State':field,'Reason':'Old or missing style','Actual':b['Resource']})
            if c['Kind']=='Button' and row['SupportsThemeBindings']:
                bindings=[b for b in row['ThemeBindings'] if b['Widget']==c['Name']]
                if len(bindings)!=1 or bindings[0]['Style']!='UI.Style.Button.'+c['Family']:
                    errors.append({'Asset':spec['Asset'],'Control':c['Name'],'Reason':'Theme binding missing or stale'})
    result={'Passed':not errors,'Assets':len(config['Assets']),'Controls':count,'ErrorCount':len(errors),'Errors':errors,
            'HumanInputAccepted':False,'HumanVisualAccepted':False}
    (OUT/(label+'.Verification.json')).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='Errors'},ensure_ascii=False))
    return result
