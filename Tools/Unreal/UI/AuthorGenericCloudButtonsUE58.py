"""经正式 Monolith 执行的云纹按钮资源作者；复用现有主题链，禁止并行修改共享编辑器。"""
from pathlib import Path
import json, hashlib, shutil
import unreal as u

ROOT=Path(__file__).resolve().parents[3]
CFG=json.loads(Path(__file__).with_name("GenericCloudButtons.json").read_text(encoding="utf-8"))
OUT=ROOT/"Saved/Validation/GenericCloudButtons20261103"
PACK=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAUIPack_Core"
LIB=u.MaterialEditingLibrary
TEX="/DBAUIPack_Core/UI/Textures/Buttons"
MAT="/DBAUIPack_Core/UI/Materials/Buttons"
FIELDS={"NormalBase":"Normal","NormalHovered":"Hovered","NormalPressed":"Pressed","Disabled":"Disabled",
        "SelectedBase":"Hovered","SelectedHovered":"Hovered","SelectedPressed":"Pressed"}

def report(name,value):
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/name).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding="utf-8")

def context():
    assert Path(u.Paths.get_project_file_path()).resolve()==ROOT/"Game/DivineBeastsArena.uproject"
    lease=json.loads((OUT/"EditorLease.json").read_text(encoding="utf-8-sig"))
    import os
    assert lease["pid"]==os.getpid() and lease["write_authorized"],"编辑器租约与当前进程不匹配"

def save(asset):
    assert u.EditorAssetLibrary.save_loaded_asset(asset,False),asset.get_path_name()

def texture_path(family,mask=False):
    return TEX+"/T_DBA_UI_CloudButton_"+family+("_Alpha" if mask else "")

def instance_path(family,state):
    return MAT+"/MI_DBA_UI_Button_"+family+"_"+state

def ensure_asset(path,kind,factory):
    if u.EditorAssetLibrary.does_asset_exist(path):
        asset=u.load_asset(path)
        assert isinstance(asset,kind),path
        return asset
    asset=u.AssetToolsHelpers.get_asset_tools().create_asset(path.rsplit("/",1)[1],path.rsplit("/",1)[0],kind,factory)
    assert asset,path
    return asset

def import_textures():
    records=[]
    for family in CFG["families"]:
        source=PACK/("SourceArt/UI/Buttons/T_DBA_UI_CloudButton_"+family+".png")
        assert source.is_file()
        for mask in (False,True):
            path=texture_path(family,mask)
            task=u.AssetImportTask()
            for k,v in {"filename":str(source),"destination_path":path.rsplit("/",1)[0],
                        "destination_name":path.rsplit("/",1)[1],"replace_existing":True,"automated":True,"save":False}.items():
                task.set_editor_property(k,v)
            u.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
            tex=u.load_asset(path);assert isinstance(tex,u.Texture2D)
            for k,v in {"srgb":not mask,"compression_settings":u.TextureCompressionSettings.TC_EDITOR_ICON,
                        "mip_gen_settings":u.TextureMipGenSettings.TMGS_NO_MIPMAPS,
                        "lod_group":u.TextureGroup.TEXTUREGROUP_UI,"compression_no_alpha":False,"never_stream":True,
                        "adjust_brightness":1.0,"adjust_brightness_curve":1.0,"adjust_saturation":1.0,
                        "adjust_vibrance":0.0,"adjust_rgb_curve":1.0,"adjust_hue":0.0,"use_legacy_gamma":False}.items():
                tex.set_editor_property(k,v)
            save(tex)
            records.append({"Source":str(source.relative_to(ROOT)).replace("\\","/"),"Destination":path,
                            "Role":"AlphaMask" if mask else "Color","SHA256":hashlib.sha256(source.read_bytes()).hexdigest(),
                            "SRGB":not mask,"Size":[tex.blueprint_get_size_x(),tex.blueprint_get_size_y()]})
    report("NativeTextures.json",records)
    return records

def materials():
    context()
    assert not u.EditorLoadingAndSavingUtils.get_dirty_content_packages(),"存在其他任务未保存的内容包"
    records=import_textures()
    material=ensure_asset(CFG["material"],u.Material,u.MaterialFactoryNew())
    material.set_editor_property("material_domain",u.MaterialDomain.MD_UI)
    material.set_editor_property("blend_mode",u.BlendMode.BLEND_TRANSLUCENT)
    LIB.delete_all_material_expressions(material)
    custom=LIB.create_material_expression(material,u.MaterialExpressionCustom,150,0)
    custom.set_editor_property("code",Path(__file__).with_name("GenericCloudButton.hlsl").read_text(encoding="utf-8"))
    custom.set_editor_property("output_type",u.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property("description","云纹按钮：原RGB直通、内芯可选换色、状态亮度和独立线性Alpha")
    inputs=[]
    nodes={}
    for index,(name,is_mask) in enumerate([("ColorTexture",False),("AlphaTexture",True)]):
        node=LIB.create_material_expression(material,u.MaterialExpressionTextureSampleParameter2D,-600,index*200)
        node.set_editor_property("parameter_name",name)
        node.set_editor_property("texture",u.load_asset(texture_path("Primary",is_mask)))
        node.set_editor_property("sampler_type",u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR if is_mask else u.MaterialSamplerType.SAMPLERTYPE_COLOR)
        input_name="Alpha" if is_mask else "ColorRGB"
        ci=u.CustomInput();ci.set_editor_property("input_name",input_name);inputs.append(ci)
        nodes[input_name]=(node,"A" if is_mask else "RGB")
    vector=LIB.create_material_expression(material,u.MaterialExpressionVectorParameter,-600,450)
    vector.set_editor_property("parameter_name","FillColor")
    vector.set_editor_property("default_value",u.LinearColor(0.03,1.0,0.02,1.0))
    ci=u.CustomInput();ci.set_editor_property("input_name","FillColor");inputs.append(ci);nodes["FillColor"]=(vector,"")
    for idx,(name,value) in enumerate([("RecolorAmount",0.0),("Brightness",1.0),("Saturation",1.0),("Opacity",1.0)]):
        node=LIB.create_material_expression(material,u.MaterialExpressionScalarParameter,-600,600+idx*150)
        node.set_editor_property("parameter_name",name);node.set_editor_property("default_value",value)
        ci=u.CustomInput();ci.set_editor_property("input_name",name);inputs.append(ci);nodes[name]=(node,"")
    custom.set_editor_property("inputs",inputs)
    for name,(node,pin) in nodes.items():assert LIB.connect_material_expressions(node,pin,custom,name),name
    alpha=LIB.create_material_expression(material,u.MaterialExpressionComponentMask,450,160)
    for c in ("r","g","b"):alpha.set_editor_property(c,False)
    alpha.set_editor_property("a",True)
    assert LIB.connect_material_expressions(custom,"",alpha,"")
    assert LIB.connect_material_property(custom,"",u.MaterialProperty.MP_EMISSIVE_COLOR)
    assert LIB.connect_material_property(alpha,"",u.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(material);save(material)
    instances=[]
    for family in CFG["families"]:
        for state,values in CFG["states"].items():
            path=instance_path(family,state)
            inst=ensure_asset(path,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
            LIB.set_material_instance_parent(inst,material)
            for param,mask in [("ColorTexture",False),("AlphaTexture",True)]:
                assert LIB.set_material_instance_texture_parameter_value(inst,param,u.load_asset(texture_path(family,mask)))
            for key,value in {**values,"RecolorAmount":0.0}.items():
                assert LIB.set_material_instance_scalar_parameter_value(inst,key,value)
            assert LIB.set_material_instance_vector_parameter_value(inst,"FillColor",u.LinearColor(0.03,1.0,0.02,1.0) if family=="Primary" else u.LinearColor(0.02,0.65,1.0,1.0))
            LIB.update_material_instance(inst);save(inst);instances.append(path)
    # 一份用于展示参数的项目实例，仍共用同一母材质和执行链。
    custom_path=instance_path("Custom","Normal")
    inst=ensure_asset(custom_path,u.MaterialInstanceConstant,u.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(inst,material)
    for name,mask in [("ColorTexture",False),("AlphaTexture",True)]:
        LIB.set_material_instance_texture_parameter_value(inst,name,u.load_asset(texture_path("Primary",mask)))
    LIB.set_material_instance_scalar_parameter_value(inst,"RecolorAmount",1.0)
    LIB.set_material_instance_vector_parameter_value(inst,"FillColor",u.LinearColor(0.52,0.08,0.9,1.0))
    LIB.update_material_instance(inst);save(inst);instances.append(custom_path)
    report("NativeMaterials.json",{"Material":CFG["material"],"Instances":instances,"TextureSamples":2,
                                 "CodeSHA256":hashlib.sha256(Path(__file__).with_name("GenericCloudButton.hlsl").read_bytes()).hexdigest()})
    print("CLOUD_BUTTON_NATIVE_MATERIALS_SAVED",len(records),len(instances))

def make_brush(brush,resource):
    brush.set_editor_property("resource_object",resource)
    brush.set_editor_property("draw_as",u.SlateBrushDrawType.BOX)
    brush.set_editor_property("image_type",u.SlateBrushImageType.FULL_COLOR)
    m=CFG["margin"];brush.set_editor_property("margin",u.Margin(left=m[0],top=m[1],right=m[2],bottom=m[3]))
    size=u.DeprecateSlateVector2D();size.set_editor_property("x",CFG["default_size"][0]);size.set_editor_property("y",CFG["default_size"][1])
    brush.set_editor_property("image_size",size)
    tint=brush.get_editor_property("tint_color");tint.set_editor_property("specified_color",u.LinearColor(1,1,1,1));brush.set_editor_property("tint_color",tint)
    return brush

def bind_styles():
    context();records=[]
    backup=OUT/"Backups";backup.mkdir(exist_ok=True)
    for family in CFG["families"]:
        path="/DBAUIPack_Core/UI/Styles/BP_DBA_ButtonStyle_"+family
        disk=PACK/("Content/UI/Styles/BP_DBA_ButtonStyle_"+family+".uasset")
        if not (backup/disk.name).exists():shutil.copy2(disk,backup/disk.name)
        cdo=u.get_default_object(u.EditorAssetLibrary.load_blueprint_class(path))
        assert isinstance(cdo,u.CommonButtonStyle)
        fields={}
        cdo.set_editor_property("bSingleMaterial",False)
        for field,state in FIELDS.items():
            resource=u.load_asset(instance_path(family,state));assert resource
            cdo.set_editor_property(field,make_brush(cdo.get_editor_property(field),resource))
            fields[field]=cdo.get_editor_property(field).get_editor_property("resource_object").get_path_name()
        asset=u.load_asset(path)
        u.BlueprintEditorLibrary.compile_blueprint(asset)
        save(asset);records.append({"Style":path,"Fields":fields})
    report("NativeStyleBindings.json",records)
    print("CLOUD_BUTTON_EXISTING_STYLES_BOUND",len(records))

def verify():
    context();checks=[]
    for family in CFG["families"]:
        for mask in (False,True):
            tex=u.load_asset(texture_path(family,mask))
            assert tex.get_editor_property("srgb")== (not mask)
            assert [tex.blueprint_get_size_x(),tex.blueprint_get_size_y()]==CFG["source_size"]
            assert tex.get_editor_property("compression_settings")==u.TextureCompressionSettings.TC_EDITOR_ICON
        cdo=u.get_default_object(u.EditorAssetLibrary.load_blueprint_class("/DBAUIPack_Core/UI/Styles/BP_DBA_ButtonStyle_"+family))
        for field,state in FIELDS.items():
            brush=cdo.get_editor_property(field);resource=brush.get_editor_property("resource_object")
            assert resource.get_path_name().split(".")[0]==instance_path(family,state),(field,resource)
            assert brush.get_editor_property("draw_as")==u.SlateBrushDrawType.BOX
            assert brush.get_editor_property("tint_color").get_editor_property("specified_color")==u.LinearColor(1,1,1,1)
            size=brush.get_editor_property("image_size")
            assert [size.get_editor_property("x"),size.get_editor_property("y")]==CFG["default_size"]
        assert not cdo.get_editor_property("bSingleMaterial")
        for state,params in CFG["states"].items():
            inst=u.load_asset(instance_path(family,state))
            assert inst.get_editor_property("parent").get_path_name().split(".")[0]==CFG["material"]
            assert abs(LIB.get_material_instance_scalar_parameter_value(inst,"RecolorAmount"))<0.0001
            for parameter,mask in [("ColorTexture",False),("AlphaTexture",True)]:
                assert LIB.get_material_instance_texture_parameter_value(inst,parameter).get_path_name().split(".")[0]==texture_path(family,mask)
            for key,value in params.items():
                assert abs(LIB.get_material_instance_scalar_parameter_value(inst,key)-value)<0.0001
        checks.append({"Family":family,"StyleFields":len(FIELDS),"States":len(CFG["states"]),"Passed":True})
    report("NativeVerification.json",{"Checks":checks,"DirtyContent":[p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
                                     "NativeReadback":True,"RuntimeInteraction":False,"HumanAccepted":False})
    print("CLOUD_BUTTON_NATIVE_READBACK_PASSED",len(checks))

def style_showcase():
    """仅配置独立审核控件；正式页面仍由已有主题事件设置样式。"""
    context()
    path="/Game/Development/UI/WBP_DBA_CloudButtonShowcase"
    bp=u.load_asset(path);assert bp
    tree=u.find_object(bp,"WidgetTree");assert tree
    records=[]
    def apply(name,family,state=None,custom=False):
        widget=u.find_object(tree,name);assert isinstance(widget,u.Button),name
        style=widget.get_editor_property("widget_style")
        for field,phase in [("normal","Normal"),("hovered","Hovered"),("pressed","Pressed"),("disabled","Disabled")]:
            resource=u.load_asset(instance_path("Custom" if custom else family,"Normal" if custom else state or phase));assert resource
            style.set_editor_property(field,make_brush(style.get_editor_property(field),resource))
        # 保留原生按钮输入语义；静态状态行只用于四态视觉比较。
        widget.set_style(style)
        widget.set_background_color(u.LinearColor(1,1,1,1))
        widget.set_color_and_opacity(u.LinearColor(1,1,1,1))
        widget.set_is_enabled(state!="Disabled")
        records.append({"Widget":name,"Family":family,"FixedVisualState":state,"NativeDisabled":state=="Disabled"})
    for family in CFG["families"]:
        for state in CFG["states"]:apply("State_"+family+"_"+state,family,state)
        apply("Interactive_"+family,family)
    apply("CustomColor","Primary",custom=True)
    for size in ("Minimum","Default","Wide"):apply("Size_"+size,"Primary")
    u.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    report("NativeShowcase.json",{"Asset":path,"Buttons":records,"RuntimeInputVerified":False})
    print("CLOUD_BUTTON_NATIVE_SHOWCASE_SAVED",len(records))

def register_color_sources():
    """在原生纹理保存后合并当前导入合同；保留其它任务新写的全部登记。"""
    context()
    path=PACK/"Docs/UIColorImportManifest.json"
    data=json.loads(path.read_text(encoding="utf-8-sig"))
    records=json.loads((OUT/"NativeTextures.json").read_text(encoding="utf-8"))
    entries={item["Destination"]:item for item in data["Files"]}
    assert len(entries)==len(data["Files"]),"当前颜色合同存在重复资源身份"
    for item in records:
        entry={key:item[key] for key in ("Source","Destination","Role","SHA256")}
        entry["DescriptionZh"]="透明云纹通用按钮；Color采sRGB RGB，AlphaMask只采线性A；默认色彩直通，换色由材质显式参数控制。"
        entries[item["Destination"]]=entry
    data["Files"]=list(entries.values())
    data["DescriptionZh"]="第三层原稿颜色政策；彩色原稿与Alpha遮罩分别登记sRGB/线性合同、源哈希及稳定资源身份。"
    path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    report("NativeColorManifest.json",{"AddedOrUpdated":[r["Destination"] for r in records],"Total":len(entries)})

