"""经正式Monolith执行的项目小地图材质/底图作者；不创建运行框架或修改玩法。"""
import hashlib,json,runpy,struct
from pathlib import Path
import unreal as u

ROOT=Path(__file__).resolve().parents[3]
CONFIG=json.loads(Path(__file__).with_name('CompactMinimap.json').read_text(encoding='utf-8'))
OUT=ROOT/'Saved/Validation/Minimap20261101'
LIB=u.MaterialEditingLibrary

def context():
    assert Path(u.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
    assert u.SystemLibrary.get_engine_version().startswith('5.8.')

def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

def material_assets():
    context()
    directory=CONFIG['MaterialDirectory'];path=directory+'/M_DBA_MinimapMetal'
    code=Path(__file__).with_name('CompactMinimapMetal.hlsl').read_text(encoding='utf-8')
    material=u.load_asset(path) if u.EditorAssetLibrary.does_asset_exist(path) else None
    if material:
        bodies=[n.get_editor_property('code') for n in LIB.get_material_expressions(material) if isinstance(n,u.MaterialExpressionCustom)]
        assert bodies==[code],'已有材质与当前合同不同，拒绝覆盖'
    else:
        material=u.AssetToolsHelpers.get_asset_tools().create_asset('M_DBA_MinimapMetal',directory,u.Material,u.MaterialFactoryNew())
        assert material
        material.set_editor_property('material_domain',u.MaterialDomain.MD_UI)
        material.set_editor_property('blend_mode',u.BlendMode.BLEND_TRANSLUCENT)
        uv=LIB.create_material_expression(material,u.MaterialExpressionTextureCoordinate,-650,-200)
        values={'UV':uv}
        for index,(name,value) in enumerate((('Diameter',float(CONFIG['CircleSize'])),('FrameWidth',float(CONFIG['FrameWidth'])),('FillCenter',0.0),('Brightness',1.0))):
            node=LIB.create_material_expression(material,u.MaterialExpressionScalarParameter,-650,index*130)
            node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',value)
            node.set_editor_property('group','Minimap');values[name]=node
        for index,(name,color) in enumerate((('Body',(0.030,0.042,0.051,1.0)),('Trim',(0.45,0.29,0.135,1.0)))):
            node=LIB.create_material_expression(material,u.MaterialExpressionVectorParameter,-650,580+index*130)
            node.set_editor_property('parameter_name',name);node.set_editor_property('default_value',u.LinearColor(*color))
            node.set_editor_property('group','Minimap');values[name]=node
        custom=LIB.create_material_expression(material,u.MaterialExpressionCustom,-200,0)
        custom.set_editor_property('code',code);custom.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT4)
        pins=[]
        for name in values:
            pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
        custom.set_editor_property('inputs',pins)
        for name,node in values.items():assert LIB.connect_material_expressions(node,'',custom,name)
        alpha=LIB.create_material_expression(material,u.MaterialExpressionComponentMask,50,160)
        for channel in ('r','g','b'):alpha.set_editor_property(channel,False)
        alpha.set_editor_property('a',True)
        assert LIB.connect_material_expressions(custom,'',alpha,'')
        assert LIB.connect_material_property(custom,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        assert LIB.connect_material_property(alpha,'',u.MaterialProperty.MP_OPACITY)
        LIB.recompile_material(material)
        assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    instances=[]
    for name,parameters in (
      ('MI_DBA_MinimapFrame',{'Diameter':float(CONFIG['CircleSize']),'FrameWidth':float(CONFIG['FrameWidth']),'FillCenter':0.0,'Brightness':1.0}),
      ('MI_DBA_MinimapButton',{'Diameter':float(CONFIG['ButtonSize']),'FrameWidth':1.4,'FillCenter':1.0,'Brightness':1.0})):
        asset=directory+'/'+name
        instance=u.load_asset(asset) if u.EditorAssetLibrary.does_asset_exist(asset) else None
        if instance:
            assert instance.get_editor_property('parent')==material,'已有实例的父材质不同，不覆盖'
        else:
            instance=u.AssetToolsHelpers.get_asset_tools().create_asset(name,directory,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
            assert instance;LIB.set_material_instance_parent(instance,material)
            for parameter,value in parameters.items():
                LIB.set_material_instance_scalar_parameter_value(instance,parameter,value)
                assert abs(LIB.get_material_instance_scalar_parameter_value(instance,parameter)-value)<.0001
            LIB.update_material_instance(instance)
            assert u.EditorAssetLibrary.save_loaded_asset(instance,False)
        instances.append(asset)
    write('MaterialAssets.json',{'Material':path,'Instances':instances,'CodeSHA256':hashlib.sha256(code.encode('utf-8')).hexdigest(),'Domain':'UI','TextureSamples':0})
    print('COMPACT_MINIMAP_MATERIALS_SAVED',len(instances)+1)

def import_map(source=None, save=True):
    context()
    project=ROOT/'Game'
    source=Path(source).resolve() if source else project/'Saved/Validation/AnimationHUD/VillageMinimapCapture.png'
    assert source.is_relative_to(project/'Saved') and source.is_file()
    capture=runpy.run_path(str(ROOT/'Tools/Unreal/World/CaptureVillageMinimap.py'))
    x,y,width=capture['map_rect'](project)
    metadata=json.loads(source.with_suffix('.json').read_text(encoding='utf-8'))
    pixels=source.read_bytes()
    assert len(pixels)>12000 and pixels[:8]==b'\x89PNG\r\n\x1a\n'
    assert struct.unpack_from('>II',pixels,16)==(1024,1024)
    assert metadata['level'].startswith('/DBAWorldPack_Village/Maps/L_Village_Start.')
    assert metadata['sha256']==hashlib.sha256(pixels).hexdigest()
    assert metadata['center_centimeters']==[x,y] and metadata['extent_centimeters']==width
    assert metadata['north']=='+X' and metadata['east']=='+Y'
    asset=CONFIG['Texture'];task=u.AssetImportTask()
    for name,value in {'filename':str(source),'destination_path':asset.rsplit('/',1)[0],
                       'destination_name':asset.rsplit('/',1)[1],'replace_existing':True,
                       'replace_existing_settings':False,'automated':True,'save':False}.items():
        task.set_editor_property(name,value)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=u.load_asset(asset);assert isinstance(texture,u.Texture2D)
    texture.set_editor_property('srgb',True)
    texture.set_editor_property('compression_no_alpha',True)
    texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings',u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    if save:
        assert u.EditorAssetLibrary.save_loaded_asset(texture,False)
        write('MapImport.json',{'Asset':asset,'Source':str(source),'Size':metadata['size'],
                              'SRGB':True,'CompressionNoAlpha':True,'MipMode':'SimpleAverage','RealtimeCapture':False,
                              'CaptureSHA256':metadata['sha256'],'WorldRectCm':[x,y,width]})
        print('COMPACT_MINIMAP_REAL_MAP_SAVED')
    return texture

def layout_assets():
    """当前唯一布局作者：按CompactMinimap合同调整现有资产，不重建树或缩放字体。"""
    context()
    widget=u.load_asset(CONFIG['Widget']);host=u.load_asset(CONFIG['Host'])
    assert widget and host
    tree=u.find_object(widget,'WidgetTree');host_tree=u.find_object(host,'WidgetTree')
    def node(name):
        value=u.find_object(tree,name)
        assert value,name
        return value
    def rect(value,position,size):
        assert isinstance(value.slot,u.CanvasPanelSlot),value.get_name()
        value.slot.set_position(u.Vector2D(*position))
        value.slot.set_size(u.Vector2D(*size))
    rect(node('MapCanvas'),CONFIG['MapPosition'],[CONFIG['MapSize']]*2)
    map_image=node('MapImage');brush=map_image.get_editor_property('brush')
    assert brush.get_editor_property('resource_object').get_path_name().split('.')[0]==CONFIG['Texture']
    image_size=brush.get_editor_property('image_size')
    image_size.import_text('(X=%f,Y=%f)'%(CONFIG['MapSize'],CONFIG['MapSize']))
    brush.set_editor_property('image_size',image_size)
    outline=brush.get_editor_property('outline_settings')
    outline.set_editor_property('corner_radii',u.Vector4(*([CONFIG['MapSize']/2]*4)))
    brush.set_editor_property('outline_settings',outline);map_image.set_brush(brush)
    rect(node('NatureMinimapFrame'),CONFIG['CirclePosition'],[CONFIG['CircleSize']]*2)
    frame=node('NatureMinimapFrame');brush=frame.get_editor_property('brush')
    instance=u.load_asset(CONFIG['MaterialDirectory']+'/MI_DBA_MinimapFrame')
    assert brush.get_editor_property('resource_object')==instance
    image_size=brush.get_editor_property('image_size')
    image_size.import_text('(X=%f,Y=%f)'%(CONFIG['CircleSize'],CONFIG['CircleSize']))
    brush.set_editor_property('image_size',image_size)
    frame.set_brush(brush)
    for parameter,value in (('Diameter',CONFIG['CircleSize']),('FrameWidth',CONFIG['FrameWidth'])):
        LIB.set_material_instance_scalar_parameter_value(instance,parameter,float(value))
        assert abs(LIB.get_material_instance_scalar_parameter_value(instance,parameter)-value)<.0001
    LIB.update_material_instance(instance)
    rect(node('MinimapHeading'),[0,0],[CONFIG['CircleSize'],20])
    rect(node('NorthCompassBadge'),[CONFIG['CirclePosition'][0]+CONFIG['CircleSize']/2-10,CONFIG['CirclePosition'][1]+6],[20,16])
    for name,button in CONFIG['Buttons'].items():
        rect(node(name),button['position'],[CONFIG['ButtonSize']]*2)
    mounted=u.find_object(host_tree,CONFIG['HostWidget']);assert mounted
    rect(mounted,[-CONFIG['ScreenMargin'][0],CONFIG['ScreenMargin'][1]],CONFIG['WidgetSize'])
    mounted.slot.set_auto_size(False)
    # 锚点、交互委托、地图UV、标记归一化锚点及固定字体均保留现有值。
    for blueprint in (widget,host):u.BlueprintEditorLibrary.compile_blueprint(blueprint)
    for asset in (instance,widget,host):assert u.EditorAssetLibrary.save_loaded_asset(asset,False)
    print('COMPACT_MINIMAP_LAYOUT_SAVED',CONFIG['MapSize'],CONFIG['WidgetSize'])
