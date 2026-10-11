# -*- coding: utf-8 -*-
"""UE5.8真实群落资产作者：显式FBX厘米单位/线性RGBA，PBR纹理及粒子局部变形母材质。"""
from pathlib import Path
import json,hashlib,unreal
ROOT=Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).parent
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/Fauna'
GP='/GamePlatformVFX/Environment/Fauna'
DBA='/DBAPresentationPack_Core/VFX/Environment/Fauna'
SPECIES=('ButterflyAzure','FishSilver')
assets=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
lib=unreal.MaterialEditingLibrary
def require(value,message):
    if not value: raise RuntimeError(message)
    return value
def import_textures():
    result={}
    for species in SPECIES:
        result[species]={}
        for role in ('BaseColor','Roughness','Normal'):
            source=SOURCE/'Textures'/('T_DBA_'+species+'_'+role+'_Source.png')
            require(source.is_file(),'纹理源缺失：'+str(source))
            name='T_DBA_'+species+'_'+role;path=DBA+'/Textures/'+name
            if not assets.does_asset_exist(path):
                task=unreal.AssetImportTask();task.set_editor_property('filename',str(source))
                task.set_editor_property('destination_path',DBA+'/Textures');task.set_editor_property('destination_name',name)
                task.set_editor_property('automated',True);task.set_editor_property('save',False)
                tools.import_asset_tasks([task])
            tex=require(assets.load_asset(path),'纹理导入失败：'+path)
            tex.set_editor_property('srgb',role=='BaseColor')
            tex.set_editor_property('compression_settings',
                unreal.TextureCompressionSettings.TC_NORMALMAP if role=='Normal' else
                unreal.TextureCompressionSettings.TC_MASKS if role=='Roughness' else unreal.TextureCompressionSettings.TC_DEFAULT)
            if role=='Normal': tex.set_editor_property('flip_green_channel',True)
            tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP if role=='Normal' else unreal.TextureGroup.TEXTUREGROUP_WORLD)
            require(assets.save_loaded_asset(tex,only_if_is_dirty=False),'纹理保存失败')
            result[species][role]=tex
    return result
def neutral_mask():
    # 平台默认值纹理是1像素线性数值数据，不引用第三层物种图片。
    import struct,zlib
    source=ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformVFX/SourceArt/Fauna/NeutralRoughness.png'
    def chunk(name,data): return struct.pack('>I',len(data))+name+data+struct.pack('>I',zlib.crc32(name+data)&0xffffffff)
    png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(bytes([0,166,166,166])))+chunk(b'IEND',b'')
    if not source.exists(): source.write_bytes(png)
    path=GP+'/Materials/T_GP_Fauna_NeutralRoughness'
    if not assets.does_asset_exist(path):
        task=unreal.AssetImportTask();task.set_editor_property('filename',str(source))
        task.set_editor_property('destination_path',GP+'/Materials');task.set_editor_property('destination_name','T_GP_Fauna_NeutralRoughness')
        task.set_editor_property('automated',True);task.set_editor_property('save',False);tools.import_asset_tasks([task])
    texture=require(assets.load_asset(path),'平台中立数值纹理导入失败')
    texture.set_editor_property('srgb',False);texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_MASKS)
    require(assets.save_loaded_asset(texture,only_if_is_dirty=False),'平台数值纹理保存失败')
    return texture
def master_material():
    path=GP+'/Materials/M_GP_Fauna_Master'
    neutral=neutral_mask()
    if assets.does_asset_exist(path):
        mat=require(assets.load_asset(path),'母材质类型无效')
        require(assets.get_metadata_tag(mat,'FaunaAuthor')=='1','拒绝覆盖非本工具母材质')
        lib.delete_all_material_expressions(mat)
    else:
        mat=require(tools.create_asset('M_GP_Fauna_Master',GP+'/Materials',unreal.Material,unreal.MaterialFactoryNew()),'创建母材质失败')
        assets.set_metadata_tag(mat,'FaunaAuthor','1')
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('used_with_niagara_mesh_particles',True)
    def node(cls,x,y): return lib.create_material_expression(mat,cls,x,y)
    def connect(src,pin,dst,name): require(lib.connect_material_expressions(src,pin,dst,name),'材质连线失败：'+pin+'→'+name)
    def output(src,pin,prop): require(lib.connect_material_property(src,pin,prop),'材质输出连接失败')
    for role,y in [('BaseColor',-650),('Roughness',-420),('Normal',-200)]:
        tex=node(unreal.MaterialExpressionTextureSampleParameter2D,-1100,y)
        tex.set_editor_property('parameter_name',unreal.Name(role))
        tex.set_editor_property('texture',neutral if role=='Roughness' else assets.load_asset('/Engine/EngineMaterials/DefaultNormal' if role=='Normal' else '/Engine/EngineResources/DefaultTexture'))
        if role=='Normal': tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        elif role=='Roughness':
            # 运行实例使用线性Masks纹理；默认只提供中立常量，避免平台依赖项目纹理。
            tex.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        output(tex,'R' if role=='Roughness' else 'RGB',
               {'BaseColor':unreal.MaterialProperty.MP_BASE_COLOR,'Roughness':unreal.MaterialProperty.MP_ROUGHNESS,'Normal':unreal.MaterialProperty.MP_NORMAL}[role])
    spec=node(unreal.MaterialExpressionConstant,-500,-550);spec.set_editor_property('r',0.35)
    output(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    pos=node(unreal.MaterialExpressionWorldPosition,-1600,250)
    pos.set_editor_property('world_position_shader_offset',unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    local=node(unreal.MaterialExpressionTransformPosition,-1330,250)
    local.set_editor_property('transform_source_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_WORLD)
    local.set_editor_property('transform_type',unreal.MaterialPositionTransformSource.TRANSFORMPOSSOURCE_INSTANCE)
    connect(pos,'',local,'')
    dynamic=node(unreal.MaterialExpressionDynamicParameter,-1600,600)
    # GetOutputs初始化动态参数的真实命名输出；Connect接口只查已缓存Outputs。
    lib.get_material_expression_output_names(dynamic)
    color=node(unreal.MaterialExpressionVertexColor,-1600,870)
    kind=node(unreal.MaterialExpressionScalarParameter,-1300,1000)
    kind.set_editor_property('parameter_name','FaunaKind');kind.set_editor_property('default_value',0.0)
    custom=node(unreal.MaterialExpressionCustom,-920,360)
    custom.set_editor_property('description','粒子局部蝶翼旋转/鱼尾侧摆：厘米和归一化周期')
    custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs=[]
    for name in ('P','Phase','Flight','Weight','Kind'):
        item=unreal.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
    custom.set_editor_property('inputs',inputs)
    code="""float phase=Phase*6.28318530718;
float w=saturate(Weight);
if(Kind<0.5)
{
    float side=P.y>=0.0?1.0:-1.0;
    float angle=side*lerp(1.24,0.08+0.78*sin(phase),saturate(Flight));
    float y=P.y-side*0.25;
    float z=P.z-0.12;
    float c=cos(angle),s=sin(angle);
    return float3(0.0,(y*c-z*s)-y,(y*s+z*c)-z)*w;
}
float tail=pow(w,1.35);
float lateral=sin(phase-(9.4-P.x)*0.37)*1.65*tail;
return float3(0.0,lateral,0.10*sin(phase*0.5)*tail);"""
    custom.set_editor_property('code',code)
    connect(local,'',custom,'P');connect(dynamic,'Param1',custom,'Phase')
    connect(dynamic,'Param2',custom,'Flight');connect(color,'A',custom,'Weight');connect(kind,'',custom,'Kind')
    world=node(unreal.MaterialExpressionTransform,-420,360)
    world.set_editor_property('transform_source_type',unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_INSTANCE)
    world.set_editor_property('transform_type',unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    connect(custom,'',world,'');output(world,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    errors=list(lib.recompile_material(mat))
    require(not any('error' in str(e).lower() for e in errors),'群落母材质编译错误：'+str(errors))
    require(assets.save_loaded_asset(mat,only_if_is_dirty=False),'母材质保存失败')
    return mat
def instances(master,textures):
    result={}
    for index,species in enumerate(SPECIES):
        name='MI_DBA_'+species;path=DBA+'/Materials/'+name
        mi=assets.load_asset(path) if assets.does_asset_exist(path) else tools.create_asset(name,DBA+'/Materials',unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
        require(mi,'创建实例失败');lib.set_material_instance_parent(mi,master)
        # UE5.8此setter源码返回未赋值的false；以真实参数回读判定写入，不依赖该错误返回值。
        for role,texture in textures[species].items():
            lib.set_material_instance_texture_parameter_value(mi,role,texture)
            actual=lib.get_material_instance_texture_parameter_value(mi,role)
            require(actual and actual.get_path_name()==texture.get_path_name(),'纹理参数回读不一致：'+role)
        lib.set_material_instance_scalar_parameter_value(mi,'FaunaKind',float(index))
        require(abs(lib.get_material_instance_scalar_parameter_value(mi,'FaunaKind')-float(index))<0.0001,'物种运动模式回读不一致')
        lib.update_material_instance(mi);require(assets.save_loaded_asset(mi,only_if_is_dirty=False),'材质实例保存失败')
        result[species]=mi
    return result
def meshes(materials):
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.SystemLibrary.execute_console_command(world,'Interchange.FeatureFlags.Import.FBX 0')
    report=[]
    for species in SPECIES:
        name='SM_DBA_'+species;source=SOURCE/'Meshes'/(name+'.fbx');path=DBA+'/Meshes/'+name
        require(source.is_file(),'模型源缺失')
        if not assets.does_asset_exist(path):
            settings=unreal.FbxImportUI();settings.set_editor_property('import_mesh',True)
            settings.set_editor_property('import_as_skeletal',False);settings.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
            settings.set_editor_property('import_materials',False);settings.set_editor_property('import_textures',False)
            options=settings.get_editor_property('static_mesh_import_data')
            for key,value in [('combine_meshes',True),('import_uniform_scale',1.0),('convert_scene_unit',True),('convert_scene',True),('auto_generate_collision',False)]:
                options.set_editor_property(key,value)
            options.set_editor_property('vertex_color_import_option',unreal.VertexColorImportOption.REPLACE)
            options.set_editor_property('normal_import_method',unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            task=unreal.AssetImportTask();task.set_editor_property('filename',str(source))
            task.set_editor_property('destination_path',DBA+'/Meshes');task.set_editor_property('destination_name',name)
            task.set_editor_property('factory',unreal.FbxFactory());task.set_editor_property('options',settings)
            task.set_editor_property('automated',True);task.set_editor_property('save',False)
            tools.import_asset_tasks([task])
        mesh=require(assets.load_asset(path),'网格导入失败')
        importer=mesh.get_editor_property('asset_import_data')
        require(isinstance(importer,unreal.FbxStaticMeshImportData),'必须使用明确FBX导入器')
        require(importer.get_editor_property('convert_scene_unit'),'未转换厘米单位')
        require(importer.get_editor_property('vertex_color_import_option')==unreal.VertexColorImportOption.REPLACE,'未保留源RGBA')
        extent=mesh.get_bounds().box_extent; maximum=max(extent.x,extent.y,extent.z)
        expected=6.3 if species=='ButterflyAzure' else 11.7996
        require(abs(maximum-expected)<0.08,'模型单位/轴向异常：'+str(extent))
        mesh.set_material(0,materials[species])
        require(assets.save_loaded_asset(mesh,only_if_is_dirty=False),'保存网格失败')
        report.append({'species':species,'mesh':path,'maxExtentCm':maximum,'sourceSha256':hashlib.sha256(source.read_bytes()).hexdigest(),'vertexColor':'ReplaceSourceLinearRGBA'})
    output=ROOT/'Saved/Validation/Fauna/AssetImport.json';output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps({'status':'passed','meshes':report,'humanAcceptance':'pending'},ensure_ascii=False,indent=2),encoding='utf-8')
    print('FAUNA_REAL_ASSETS_IMPORTED',json.dumps(report,ensure_ascii=False))
def main():
    require('-FaunaOwnedAuthoring' in unreal.SystemLibrary.get_command_line(),'只能操作本任务编辑器')
    require('DivineBeastsWorkspace' in str(ROOT),'不是授权工程')
    textures=import_textures()
    master=master_material()
    meshes(instances(master,textures))
main()
