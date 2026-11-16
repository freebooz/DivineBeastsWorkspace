"""角色材质适配复用；通过Monolith调用，保留原算法、参数和PBR。非运行框架。"""
import hashlib
import json
import os
import re
import shutil
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/BlueprintMaterialOptimization20261027'
RECIPES=json.loads(Path(__file__).with_name('CharacterDissolveSharedRecipes.json').read_text(encoding='utf-8'))
PREFIX='CharacterDeathDissolve.'
MATERIALS=[
 '/DBAContentPack_Common/Materials/M_DBA_PrototypeCharacterColor',
 '/DBAContentPack_Common/Mannequins/Standard/Materials/M_Mannequin',
 '/DBAContentPack_Common/Characters/NPC/Arissa/Arissa_MAT',
 '/DBAContentPack_Common/Characters/KachujinGRosales/kachujin_MAT',
 '/DBAContentPack_Common/Characters/KachujinGRosales/kachujin_MAT_',
 '/DBAWorldPack_Village/Creatures/Wolf/Wolf',
 '/DBAWorldPack_Village/Creatures/VampireALusth/Vampire_MAT1',
 '/DBAWorldPack_Village/Creatures/VampireALusth/Vampire_MAT_Transparent']

def ue():
    import unreal
    assert Path(unreal.Paths.get_project_file_path()).resolve()==ROOT/'Game/DivineBeastsArena.uproject'
    assert unreal.SystemLibrary.get_engine_version().startswith('5.8.')
    return unreal

def write(name,data):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')

def normalized(code):return re.sub(r'\s+','',code)

def file_for(path):
    mount,relative=path.strip('/').split('/',1)
    descriptors=list((ROOT/'Game/Plugins').rglob(mount+'.uplugin'))
    assert len(descriptors)==1,mount
    return descriptors[0].parent/'Content'/(relative+'.uasset')

def expression_nodes(material):return list(ue().MaterialEditingLibrary.get_material_expressions(material))

def shader_statistics(material):
    # UE5.8 GetStatistics会FinishCompilation；这是Shader静态计数，不是GPU耗时。
    stats=ue().MaterialEditingLibrary.get_statistics(material)
    names=('num_vertex_shader_instructions','num_pixel_shader_instructions','num_samplers',
           'num_vertex_texture_samples','num_pixel_texture_samples','num_virtual_texture_samples',
           'num_uv_scalars','num_interpolator_scalars')
    result={name:int(getattr(stats,name)) for name in names}
    assert result['num_vertex_shader_instructions']>0 or result['num_pixel_shader_instructions']>0,material.get_path_name()
    return result

def wrappers(material):
    result={}
    for node in expression_nodes(material):
        desc=str(node.get_editor_property('desc'))
        for label in RECIPES:
            if desc==PREFIX+label:
                assert label not in result,(material.get_name(),label)
                result[label]=node
    assert set(result)==set(RECIPES),(material.get_path_name(),list(result))
    return result

def output_name(consumer,source):
    # UE Python将bool+out字符串映射为str或None；空字符串是有效的默认输出。
    name=ue().MaterialEditingLibrary.get_input_node_output_name_for_material_expression(consumer,source)
    assert name is not None,(consumer.get_name(),source.get_name())
    return str(name)

def input_links(material,node):
    ml=ue().MaterialEditingLibrary
    names=list(ml.get_material_expression_input_names(node))
    sources=list(ml.get_inputs_for_material_expression(material,node))
    assert len(names)==len(sources),(node.get_name(),names,len(sources))
    return [(str(name),source,output_name(node,source) if source else '') for name,source in zip(names,sources)]

def material_properties():
    u=ue()
    # UE5.8此原生查询对不支持的枚举不检查空指针；只访问实际存在的Root输入。
    names=['MP_EMISSIVE_COLOR','MP_OPACITY','MP_OPACITY_MASK','MP_BASE_COLOR','MP_METALLIC',
           'MP_SPECULAR','MP_ROUGHNESS','MP_ANISOTROPY','MP_NORMAL','MP_TANGENT',
           'MP_WORLD_POSITION_OFFSET','MP_DISPLACEMENT','MP_SUBSURFACE_COLOR',
           'MP_CUSTOM_DATA_0','MP_CUSTOM_DATA_1','MP_AMBIENT_OCCLUSION','MP_REFRACTION',
           'MP_MATERIAL_ATTRIBUTES','MP_PIXEL_DEPTH_OFFSET','MP_SHADING_MODEL',
           'MP_SURFACE_THICKNESS','MP_FRONT_MATERIAL']
    names += ['MP_CUSTOMIZED_UVS_'+str(i) for i in range(8)]
    return [(name,getattr(u.MaterialProperty,name)) for name in names if hasattr(u.MaterialProperty,name)]

def snapshot(material):
    u=ue();ml=u.MaterialEditingLibrary;targets=wrappers(material)
    aliases={node.get_name():label for label,node in targets.items()}
    def identity(node):return aliases.get(node.get_name(),node.get_name()) if node else None
    def pin(source,value):return 'Result' if source and source.get_name() in aliases else value
    edges={}
    for node in expression_nodes(material):
        edges[identity(node)]=[[name,identity(source),pin(source,out)] for name,source,out in input_links(material,node)]
    roots={}
    for name,prop in material_properties():
        node=ml.get_material_property_input_node(material,prop)
        roots[name]=[identity(node),pin(node,str(ml.get_material_property_input_node_output_name(material,prop))) if node else '']
    parameters={}
    for kind,names,getter in (
      ('scalar',ml.get_scalar_parameter_names,ml.get_material_default_scalar_parameter_value),
      ('vector',ml.get_vector_parameter_names,ml.get_material_default_vector_parameter_value),
      ('texture',ml.get_texture_parameter_names,ml.get_material_default_texture_parameter_value),
      ('switch',ml.get_static_switch_parameter_names,ml.get_material_default_static_switch_parameter_value)):
        values={}
        for name in names(material):
            value=getter(material,name)
            if kind=='vector':value=[value.r,value.g,value.b,value.a]
            elif kind=='texture':value=value.get_path_name() if value else None
            values[str(name)]=value
        parameters[kind]=values
    return {'edges':edges,'outputs':roots,'parameters':parameters,
            'settings':{n:str(material.get_editor_property(n)) for n in ('blend_mode','shading_model','two_sided','use_material_attributes','opacity_mask_clip_value')}}

def audit(name='NativeBefore.json'):
    u=ue();rows=[]
    for path in MATERIALS:
        material=u.load_asset(path);assert isinstance(material,u.Material),path
        nodes=wrappers(material);kinds={}
        for label,node in nodes.items():
            recipe=RECIPES[label]
            if isinstance(node,u.MaterialExpressionCustom):
                assert normalized(node.get_editor_property('code'))==normalized(recipe['code']),(path,label,'人工代码不同，不覆盖')
                assert [n for n,_,_ in input_links(material,node)]==[n for n,_ in recipe['inputs']]
                assert not list(node.get_editor_property('additional_outputs')),(path,label,'不覆盖多输出人工节点')
                kinds[label]='inline'
            else:
                assert isinstance(node,u.MaterialExpressionMaterialFunctionCall)
                assert node.get_editor_property('material_function').get_path_name().split('.')[0]==recipe['asset']
                kinds[label]='shared'
        rows.append({'material':path,'kinds':kinds,'contract':snapshot(material),'sha256':hashlib.sha256(file_for(path).read_bytes()).hexdigest()})
    result={'Materials':rows,'Count':len(rows)};write(name,result);print('DISSOLVE_REUSE_AUDIT',len(rows));return result

def custom(owner,code,inputs,output,function=False):
    u=ue();ml=u.MaterialEditingLibrary
    create=ml.create_material_expression_in_function if function else ml.create_material_expression
    node=create(owner,u.MaterialExpressionCustom,-100,0)
    node.set_editor_property('code',code)
    node.set_editor_property('output_type',u.CustomMaterialOutputType.CMOT_FLOAT3 if output=='V3' else u.CustomMaterialOutputType.CMOT_FLOAT1)
    pins=[]
    for name in inputs:
        pin=u.CustomInput();pin.set_editor_property('input_name',name);pins.append(pin)
    node.set_editor_property('inputs',pins)
    for name,(source,pin) in inputs.items():assert ml.connect_material_expressions(source,pin,node,name)
    return node

def ensure_functions(create_missing=True):
    u=ue();ml=u.MaterialEditingLibrary;functions={}
    for label,recipe in RECIPES.items():
        # 冷命令行启动时AssetRegistry可能尚未扫描；直接原生加载，不能以目录查询当作资产缺失。
        path=recipe['asset'];function=u.load_asset(path)
        if function:
            nodes=ml.get_material_function_expressions(function)
            codes=[n.get_editor_property('code') for n in nodes if isinstance(n,u.MaterialExpressionCustom)]
            assert len(codes)==1 and normalized(codes[0])==normalized(recipe['code']),path
            actual={str(n.get_editor_property('input_name')) for n in nodes if isinstance(n,u.MaterialExpressionFunctionInput)}
            assert actual=={n for n,_ in recipe['inputs']},path
        else:
            assert create_missing,(path,'核验不允许补建缺失资产')
            function=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit('/',1)[1],path.rsplit('/',1)[0],u.MaterialFunction,u.MaterialFunctionFactoryNew())
            assert function
            function.set_editor_property('description',recipe['description'])
            function.set_editor_property('expose_to_library',True)
            inputs={}
            for index,(name,kind) in enumerate(recipe['inputs']):
                node=ml.create_material_expression_in_function(function,u.MaterialExpressionFunctionInput,-600,index*150)
                node.set_editor_property('input_name',name)
                node.set_editor_property('description',recipe['input_descriptions'][name])
                node.set_editor_property('input_type',u.FunctionInputType.FUNCTION_INPUT_VECTOR3 if kind=='V3' else u.FunctionInputType.FUNCTION_INPUT_SCALAR)
                node.set_editor_property('sort_priority',index)
                inputs[name]=(node,'')
            body=custom(function,recipe['code'],inputs,recipe['output'],True)
            result=ml.create_material_expression_in_function(function,u.MaterialExpressionFunctionOutput,250,0)
            result.set_editor_property('output_name','Result');assert ml.connect_material_expressions(body,'',result,'')
            ml.update_material_function(function);assert u.EditorAssetLibrary.save_loaded_asset(function,False)
        nodes=list(ml.get_material_function_expressions(function))
        pins=sorted((n for n in nodes if isinstance(n,u.MaterialExpressionFunctionInput)),key=lambda n:n.get_editor_property('sort_priority'))
        bodies=[n for n in nodes if isinstance(n,u.MaterialExpressionCustom)]
        outputs=[n for n in nodes if isinstance(n,u.MaterialExpressionFunctionOutput)]
        assert [str(n.get_editor_property('input_name')) for n in pins]==[n for n,_ in recipe['inputs']],path
        assert len(bodies)==len(outputs)==1,path
        assert bodies[0].get_editor_property('output_type')==(u.CustomMaterialOutputType.CMOT_FLOAT3 if recipe['output']=='V3' else u.CustomMaterialOutputType.CMOT_FLOAT1),path
        assert str(outputs[0].get_editor_property('output_name'))=='Result',path
        assert list(ml.get_inputs_for_material_function_expression(function,bodies[0]))==pins,path
        assert list(ml.get_inputs_for_material_function_expression(function,outputs[0]))==bodies,path
        for node,(_,kind) in zip(pins,recipe['inputs']):
            assert node.get_editor_property('input_type')==(u.FunctionInputType.FUNCTION_INPUT_VECTOR3 if kind=='V3' else u.FunctionInputType.FUNCTION_INPUT_SCALAR),path
        functions[label]=function
    return functions

def new_shared_call(material,label,functions,position=(-1500,0)):
    u=ue();node=u.MaterialEditingLibrary.create_material_expression(material,u.MaterialExpressionMaterialFunctionCall,*position)
    assert node.set_material_function(functions[label])
    node.set_editor_property('desc',PREFIX+label)
    return node

def replace_one(material,old,new):
    u=ue();ml=u.MaterialEditingLibrary
    incoming=input_links(material,old);consumers=[];properties=[]
    assert all(source is not None for _,source,_ in incoming),'原输入不完整'
    for node in expression_nodes(material):
        if node in (old,new):continue
        for name,source,pin in input_links(material,node):
            if source==old:consumers.append((node,name))
    for _,prop in material_properties():
        if ml.get_material_property_input_node(material,prop)==old:properties.append(prop)
    for name,source,pin in incoming:assert ml.connect_material_expressions(source,pin,new,name)
    for node,name in consumers:assert ml.connect_material_expressions(new,'Result',node,name)
    for prop in properties:assert ml.connect_material_property(new,'Result',prop)
    ml.delete_material_expression(material,old)

def optimize_one(index):
    u=ue();ml=u.MaterialEditingLibrary;path=MATERIALS[index]
    material=u.load_asset(path);nodes=wrappers(material)
    assert all(isinstance(n,u.MaterialExpressionCustom) for n in nodes.values()),'只处理完整未转换材质'
    dirty={p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert path not in dirty,'目标材质有未保存人工修改，保留现场'
    before=snapshot(material);stats_before=shader_statistics(material);source=file_for(path);backup=OUT/'Backup'/source.name
    backup.parent.mkdir(parents=True,exist_ok=True)
    if backup.exists():
        assert source.read_bytes()==backup.read_bytes(),'现有备份与当前原材质不同，不覆盖'
    else:shutil.copy2(source,backup)
    functions=ensure_functions()
    for label in RECIPES:
        old=wrappers(material)[label]
        assert normalized(old.get_editor_property('code'))==normalized(RECIPES[label]['code'])
        new=new_shared_call(material,label,functions,ml.get_material_expression_node_position(old));replace_one(material,old,new)
    after=snapshot(material);assert before==after,(path,'参数、PBR或连线合同变化，未保存')
    ml.recompile_material(material);assert u.EditorAssetLibrary.save_loaded_asset(material,False)
    stats_after=shader_statistics(material)
    row={'material':path,'before':before,'after':after,'shader_before':stats_before,'shader_after':stats_after,
         'backup':str(backup),'sha256':hashlib.sha256(source.read_bytes()).hexdigest()}
    write('Material_'+str(index)+'.json',row);print('DISSOLVE_SHARED_SAVED',index,path);return row

def verify(name='Verification.json',audit_name='NativeAfter.json'):
    ensure_functions(False)
    result=audit(audit_name)
    for row in result['Materials']:assert set(row['kinds'].values())=={'shared'}
    for index,row in enumerate(result['Materials']):
        saved=json.loads((OUT/('Material_'+str(index)+'.json')).read_text(encoding='utf-8'))
        assert row['contract']==saved['before']==saved['after'],row['material']
        assert row['sha256']==saved['sha256'],row['material']
    report={'Passed':True,'Materials':len(result['Materials']),'SharedFunctions':len(RECIPES),
            'ReplacedInlineExpressions':3*len(result['Materials']),'PID':os.getpid(),
            'Engine':ue().SystemLibrary.get_engine_version(),'Project':ue().Paths.get_project_file_path()}
    write(name,report)
    print('DISSOLVE_REUSE_VERIFIED',len(result['Materials']))
    return report
