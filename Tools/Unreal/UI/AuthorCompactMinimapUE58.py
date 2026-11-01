"""经正式Monolith执行的项目小地图材质/底图作者；不创建运行框架或修改玩法。"""
import hashlib,json
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
        for index,(name,value) in enumerate((('Diameter',206.0),('FrameWidth',4.0),('FillCenter',0.0),('Brightness',1.0))):
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
      ('MI_DBA_MinimapFrame',{'Diameter':206.0,'FrameWidth':4.0,'FillCenter':0.0,'Brightness':1.0}),
      ('MI_DBA_MinimapButton',{'Diameter':22.0,'FrameWidth':1.4,'FillCenter':1.0,'Brightness':1.0})):
        asset=directory+'/'+name
        instance=u.load_asset(asset) if u.EditorAssetLibrary.does_asset_exist(asset) else None
        if instance:
            assert instance.get_editor_property('parent')==material,'已有实例的父材质不同，不覆盖'
        else:
            instance=u.AssetToolsHelpers.get_asset_tools().create_asset(name,directory,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
            assert instance;LIB.set_material_instance_parent(instance,material)
            for parameter,value in parameters.items():assert LIB.set_material_instance_scalar_parameter_value(instance,parameter,value)
            assert u.EditorAssetLibrary.save_loaded_asset(instance,False)
        instances.append(asset)
    write('MaterialAssets.json',{'Material':path,'Instances':instances,'CodeSHA256':hashlib.sha256(code.encode('utf-8')).hexdigest(),'Domain':'UI','TextureSamples':0})
    print('COMPACT_MINIMAP_MATERIALS_SAVED',len(instances)+1)

def import_map():
    context();source=OUT/'VillageMap.png';assert source.is_file()
    # 外部像素审查先写入通过证据；源图灰色、无地形或未完成时不覆盖正式底图。
    stats=json.loads((OUT/'CapturePixels.json').read_text(encoding='utf-8-sig'))
    assert stats['Passed'] and stats['ColoredSamples']>40 and stats['SampleColors']>100
    asset=CONFIG['Texture'];task=u.AssetImportTask()
    for name,value in {'filename':str(source),'destination_path':asset.rsplit('/',1)[0],
                       'destination_name':asset.rsplit('/',1)[1],'replace_existing':True,'automated':True,'save':False}.items():
        task.set_editor_property(name,value)
    u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task]);texture=u.load_asset(asset);assert isinstance(texture,u.Texture2D)
    texture.set_editor_property('srgb',True)
    texture.set_editor_property('compression_no_alpha',True)
    texture.set_editor_property('compression_settings',u.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property('lod_group',u.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property('mip_gen_settings',u.TextureMipGenSettings.TMGS_SIMPLE_AVERAGE)
    assert u.EditorAssetLibrary.save_loaded_asset(texture,False)
    write('MapImport.json',{'Asset':asset,'Source':str(source),'Size':[texture.blueprint_get_size_x(),texture.blueprint_get_size_y()],
                          'SRGB':True,'CompressionNoAlpha':True,'MipMode':'SimpleAverage','RealtimeCapture':False})
    print('COMPACT_MINIMAP_REAL_MAP_SAVED')
