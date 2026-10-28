"""仅经Monolith在UE5.8中接入真实角色材质；保留原PBR、颜色和原Masked输入。"""
import json,hashlib,shutil,importlib.util
from pathlib import Path
import unreal as u

ROOT=Path(u.Paths.project_dir()).resolve().parent
OUT=ROOT/'Saved/Validation/CharacterDeathDissolve20261024'
_spec=importlib.util.spec_from_file_location('dissolve_shared',Path(__file__).with_name('CharacterDissolveSharedUE58.py'))
SHARED=importlib.util.module_from_spec(_spec);_spec.loader.exec_module(SHARED)
PREFIX=SHARED.PREFIX
FUNCTION='/DBAPresentationPack_Core/MaterialFunctions/Character/MF_DBA_CharacterDissolve'
NOISE='/DBAPresentationPack_Core/VFX/Library/00_Core/Textures/PortalTeleport/T_VFX_Teleport_DissolveNoise_01'
MATERIALS=SHARED.MATERIALS
LIB=u.MaterialEditingLibrary


def connections(m):
    return {str(p):[n.get_name() if n else None,LIB.get_material_property_input_node_output_name(m,p)]
        for p in [u.MaterialProperty.MP_BASE_COLOR,u.MaterialProperty.MP_NORMAL,u.MaterialProperty.MP_ROUGHNESS,
                  u.MaterialProperty.MP_METALLIC,u.MaterialProperty.MP_SPECULAR,u.MaterialProperty.MP_AMBIENT_OCCLUSION,
                  u.MaterialProperty.MP_WORLD_POSITION_OFFSET]
        for n in [LIB.get_material_property_input_node(m,p)]}


def author():
    assert u.SystemLibrary.get_engine_version().startswith('5.8.')
    assert u.Paths.get_project_file_path().replace('\\','/').endswith('/Game/DivineBeastsArena.uproject')
    OUT.mkdir(parents=True,exist_ok=True);backup=OUT/'MaterialBackups';backup.mkdir(exist_ok=True)
    dirty={p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assert not dirty.intersection(MATERIALS),'目标材质有人工未保存修改，保留现场'
    function=u.load_asset(FUNCTION);noise=u.load_asset(NOISE);assert function and noise
    shared_functions=SHARED.ensure_functions()
    reports=[]
    for path in MATERIALS:
        m=u.load_asset(path);assert isinstance(m,u.Material),path
        if any(str(n.get_editor_property('desc')).startswith(PREFIX) for n in LIB.get_material_expressions(m)):
            continue
        assert not m.get_editor_property('use_material_attributes'),path
        mount,relative=path.lstrip('/').split('/',1)
        descriptors=list((ROOT/'Game/Plugins/DivineBeasts/ContentPacks').rglob(mount+'.uplugin'))
        assert len(descriptors)==1,mount
        file=descriptors[0].parent/'Content'/(relative+'.uasset')
        assert file.is_file(),str(file)
        target=backup/(path.strip('/').replace('/','__')+'.uasset');assert not target.exists(),str(target)
        shutil.copy2(file,target)
        before=connections(m);old_blend=str(m.get_editor_property('blend_mode'))
        blend=m.get_editor_property('blend_mode');translucent=blend not in [u.BlendMode.BLEND_OPAQUE,u.BlendMode.BLEND_MASKED]
        mask_property=u.MaterialProperty.MP_OPACITY if translucent else u.MaterialProperty.MP_OPACITY_MASK
        old_mask=LIB.get_material_property_input_node(m,mask_property)
        old_mask_pin=LIB.get_material_property_input_node_output_name(m,mask_property)
        old_emissive=LIB.get_material_property_input_node(m,u.MaterialProperty.MP_EMISSIVE_COLOR)
        old_emissive_pin=LIB.get_material_property_input_node_output_name(m,u.MaterialProperty.MP_EMISSIVE_COLOR)
        def new(cls,label):
            n=LIB.create_material_expression(m,cls,-1800,len(LIB.get_material_expressions(m))*35)
            n.set_editor_property('desc',PREFIX+label);return n
        def connect(a,pin,b,name):assert LIB.connect_material_expressions(a,pin,b,name),(a.get_name(),pin,b.get_name(),name)
        def scalar(name,value):
            n=new(u.MaterialExpressionScalarParameter,name);n.set_editor_property('parameter_name',name);n.set_editor_property('default_value',value);n.set_editor_property('group','CharacterDissolve');return n
        enabled=scalar('GP_DissolveEnabled',0);start=scalar('GP_DissolveStartTime',0)
        duration=scalar('GP_DissolveDuration',1.5);preview=scalar('GP_DissolvePreviewProgress',-1)
        strength=scalar('GP_DissolveEdgeStrength',1.5);width=scalar('GP_DissolveEdgeWidth',.055)
        color=new(u.MaterialExpressionVectorParameter,'GP_DissolveEdgeColor');color.set_editor_property('parameter_name','GP_DissolveEdgeColor');color.set_editor_property('default_value',u.LinearColor(.08,.65,.42,1));color.set_editor_property('group','CharacterDissolve')
        time=new(u.MaterialExpressionTime,'WorldGameTime')
        progress=SHARED.new_shared_call(m,'Progress',shared_functions,(-1800,len(LIB.get_material_expressions(m))*35))
        for n,name in [(enabled,'Enabled'),(time,'Now'),(start,'Start'),(duration,'Duration'),(preview,'Preview')]:connect(n,'',progress,name)
        uv=new(u.MaterialExpressionTextureCoordinate,'UV');uv.set_editor_property('u_tiling',3.0);uv.set_editor_property('v_tiling',3.0)
        sample=new(u.MaterialExpressionTextureSample,'Noise');sample.set_editor_property('texture',noise)
        sample.set_editor_property('sampler_type',u.MaterialSamplerType.SAMPLERTYPE_GRAYSCALE if noise.get_editor_property('srgb') else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        connect(uv,'',sample,'UVs')
        call=new(u.MaterialExpressionMaterialFunctionCall,'SharedCharacterDissolve');assert call.set_material_function(function)
        connect(sample,'R',call,'NoiseMask');connect(progress,'',call,'DissolveProgress');connect(width,'',call,'EdgeWidth');connect(color,'RGB',call,'EdgeColor')
        mask=SHARED.new_shared_call(m,'PreserveMask',shared_functions,(-1800,len(LIB.get_material_expressions(m))*35))
        if not old_mask:old_mask=new(u.MaterialExpressionConstant,'OriginalMaskDefault');old_mask.set_editor_property('r',1)
        connect(old_mask,old_mask_pin,mask,'Old');connect(progress,'',mask,'Progress');connect(call,'Mask',mask,'Mask')
        assert LIB.connect_material_property(mask,'',mask_property)
        if not translucent:m.set_editor_property('blend_mode',u.BlendMode.BLEND_MASKED)
        emission=SHARED.new_shared_call(m,'PreserveEmissive',shared_functions,(-1800,len(LIB.get_material_expressions(m))*35))
        if not old_emissive:old_emissive=new(u.MaterialExpressionConstant3Vector,'OriginalEmissiveDefault')
        connect(old_emissive,old_emissive_pin,emission,'Old');connect(call,'Emissive',emission,'Edge');connect(strength,'',emission,'Strength');connect(progress,'',emission,'Progress')
        assert LIB.connect_material_property(emission,'',u.MaterialProperty.MP_EMISSIVE_COLOR)
        assert connections(m)==before,'禁止改变原PBR输入'
        LIB.recompile_material(m);assert u.EditorAssetLibrary.save_loaded_asset(m,False)
        reports.append({'material':path,'backup':str(target),'oldBlend':old_blend,'newBlend':str(m.get_editor_property('blend_mode')),'pbrInputsPreserved':before,'sha256':hashlib.sha256(file.read_bytes()).hexdigest()})
    (OUT/'NativeMaterialAuthoring.json').write_text(json.dumps(reports,ensure_ascii=False,indent=2),encoding='utf-8')
    print('CHARACTER_DISSOLVE_MATERIALS_SAVED',len(reports))


def verify():
    required=['GP_DissolveEnabled','GP_DissolveStartTime','GP_DissolveDuration','GP_DissolveEdgeStrength','GP_DissolveEdgeColor']
    rows=[]
    for path in MATERIALS:
        m=u.load_asset(path)
        names=[str(n.get_editor_property('parameter_name')) for n in LIB.get_material_expressions(m)
            if isinstance(n,(u.MaterialExpressionScalarParameter,u.MaterialExpressionVectorParameter))]
        assert set(required)<=set(names),(path,names)
        rows.append({'material':path,'parameters':required})
    (OUT/'NativeMaterialContract.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
    print('CHARACTER_DISSOLVE_MATERIAL_CONTRACT_PASS',len(rows))
