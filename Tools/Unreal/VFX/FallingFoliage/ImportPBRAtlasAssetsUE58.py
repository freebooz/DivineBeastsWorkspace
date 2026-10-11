# -*- coding: utf-8 -*-
"""《神兽联盟》UE5.8 Niagara用展示图集PBR材质导入器（分批次）。

所有权：GamePlatformVFX（平台）提供通用PBR母材质，DBAPresentationPack_Core（项目）持有纹理/MI；
MobaCommon（通用MOBA）不产生额外纯表现实现；Dedicated Server不加载装饰性Niagara。
输入：本次用户生成6图集的可信ZIP（保存至项目SourceArt/AmbientNature/Incoming）。
预检：每张PNG/Manifest核对SHA-256，3种不合格花瓣、其他复杂粒子图只留待人工审查。
安全：默认dry-run，只在AMBIENT_PBR_APPLY=approved_apply后按选中名单分批重导，
      原.uasset逐份备份到Saved/Validation，保持原资源身份。磁盘空间不足提前拒绝。
风险：UE5.8 shader/GPU/区域实际运行/打包验收属于后续门禁，成功保存不等于播放成功。
"""
from __future__ import annotations
import hashlib, json, os, shutil, traceback
from pathlib import Path
import unreal

ROOT=Path(unreal.Paths.project_dir()).resolve().parent
SOURCE=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/AmbientNature"
STAGE=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/Stage"
REPORT=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/UEBatchImportReport.json"
BACKUP=ROOT/"Saved/Validation/FallingFoliage/PBRAtlas20261011/UEBackup"
CONTENT=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/AmbientNature"
PREFIX="/DBAPresentationPack_Core/VFX/Environment/AmbientNature"
MASTER="/GamePlatformVFX/Environment/AmbientNature/Materials/M_GP_AmbientNature_PBR_Foliage"
MODE=os.environ.get("AMBIENT_PBR_APPLY","inspect").strip()
NAME_FILTER=[x.strip() for x in os.environ.get("AMBIENT_PBR_STYLES","").split(",") if x.strip()]
ED=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
OUTPUT={"engine":str(unreal.SystemLibrary.get_engine_version()),"project":str(unreal.Paths.get_project_file_path()),
        "mode":MODE,"styles":[],"failures":[],"warnings":[],
        "editorSavedAssets":[],"runtimeVisualValidated":False,"clientCookValidated":False}

def expected_identity():
    if not OUTPUT["engine"].startswith("5.8.") or not OUTPUT["project"].replace("\\","/").endswith("/DivineBeastsArena.uproject"):
        raise RuntimeError("必须运行锁定的DivineBeastsArena UE5.8项目")

def check_hash(file,spec):
    if not file.is_file() or hashlib.sha256(file.read_bytes()).hexdigest()!=spec["sha256"]:
        raise RuntimeError("源纹理SHA-256校验失败："+str(file))

def item_path(sty,kind):
    name="T_DBA_Ambient_"+sty+"_"+kind
    return PREFIX+"/Textures/"+name,name

def copy_before_edit(path):
    relative=path.removeprefix(PREFIX+"/")
    if relative==path or ".." in Path(relative).parts:
        raise RuntimeError("试图备份未批准的挂载路径："+path)
    file=CONTENT/(relative+".uasset")
    if not file.is_file():
        raise RuntimeError("目标UE资产文件不存在，拒绝覆盖："+str(file))
    target=BACKUP/(relative+".uasset")
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists():
        shutil.copy2(file,target)
    return str(target),hashlib.sha256(file.read_bytes()).hexdigest()

def reimport_texture(style,kind,source,overwrite=True):
    path,name=item_path(style,kind)
    if overwrite:
        if not ED.does_asset_exist(path):
            raise RuntimeError("现有UE纹理身份失效："+path)
    else:
        if ED.does_asset_exist(path):
            tex=ED.load_asset(path)
            if not isinstance(tex,unreal.Texture2D):
                raise TypeError("同名纹理存在其他资产类型："+path)
            return tex
    task=unreal.AssetImportTask()
    settings=(
        ("filename",str(source)),("destination_path",PREFIX+"/Textures"),
        ("destination_name",name),("automated",True),
        ("replace_existing",overwrite),("save",False)
    )
    for key,value in settings:
        task.set_editor_property(key,value)
    TOOLS.import_asset_tasks([task])
    tex=ED.load_asset(path)
    if not isinstance(tex,unreal.Texture2D):
        raise RuntimeError("引擎实际纹理导入失败："+path)
    if kind=="Albedo":
        tex.set_editor_property("srgb",True)
        tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_DEFAULT)
    elif kind=="Normal":
        tex.set_editor_property("srgb",False)
        tex.set_editor_property("compression_settings",unreal.TextureCompressionSettings.TC_NORMALMAP)
    else:
        tex.set_editor_property("srgb",False)
        enum=unreal.TextureCompressionSettings
        # roughness/opacity为线性数据，不允许sRGB伽马重复变化。
        tex.set_editor_property("compression_settings",
            enum.TC_MASKS if hasattr(enum,"TC_MASKS") else enum.TC_DEFAULT)
    if not ED.save_loaded_asset(tex,only_if_is_dirty=False):
        raise RuntimeError("真实UE纹理保存失败："+path)
    OUTPUT["editorSavedAssets"].append(path)
    print("AMBIENT_PBR_TEXTURE_IMPORTED",style,kind,flush=True)
    return tex

def assign_material(style,textures,master,sizes):
    path=PREFIX+"/Materials/MI_DBA_Ambient_"+style
    mi=ED.load_asset(path)
    if not isinstance(mi,unreal.MaterialInstanceConstant):
        raise RuntimeError("现有项目材质实例无效："+path)
    LIB.set_material_instance_parent(mi,master)
    for param,key in (("LeafAlbedo","Albedo"),("LeafNormal","Normal"),
                      ("LeafOpacity","Opacity"),("LeafRoughnessMap","Roughness")):
        LIB.set_material_instance_texture_parameter_value(mi,unreal.Name(param),textures[key])
    LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("RoughnessScale"),1.0)
    LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("LeafSpecular"),.18)
    LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("OpacityMultiplier"),1.0)
    LIB.set_material_instance_scalar_parameter_value(mi,unreal.Name("LeafTransmission"),.50)
    if not ED.save_loaded_asset(mi,only_if_is_dirty=False):
        raise RuntimeError("项目PBR材质实例保存失败："+path)
    if mi.get_editor_property("parent")!=master:
        raise RuntimeError("PBR母材质引用回读不一致："+path)
    OUTPUT["editorSavedAssets"].append(path)
    print("AMBIENT_PBR_MATERIAL_SAVED",style,path,flush=True)
    return mi

def verify_existing_niagara(style,mi):
    name="NS_DBA_Ambient_"+style
    path=PREFIX+"/Niagara/"+name
    system=ED.load_asset(path)
    if not isinstance(system,unreal.NiagaraSystem):
        raise RuntimeError("既有Niagara特效系统不存在："+path)
    count=0
    prefix=system.get_path_name()+":"
    for renderer in unreal.ObjectIterator(unreal.NiagaraSpriteRendererProperties):
        if renderer.get_path_name().startswith(prefix):
            count+=1
            if renderer.get_editor_property("material")!=mi:
                raise RuntimeError("Niagara使用其他材质实例，必须中止重导："+path)
    if count<1:
        raise RuntimeError("Niagara不具有Sprite渲染器："+path)
    return count

def load_source():
    path=STAGE/"Manifest.json"
    if not path.is_file():
        raise FileNotFoundError("图集ZIP尚未按PreflightPBRAtlases.py解包验收："+str(path))
    obj=json.loads(path.read_text(encoding="utf-8"))
    if len(obj.get("styles",[]))!=33:
        raise RuntimeError("拆分纹理清单不是预期的33套")
    candidates=[s for s in obj["styles"] if s.get("importPolicy")=="auto_candidate" and s.get("kind") in ("petal","leaf")]
    if NAME_FILTER:
        extra=set(NAME_FILTER)-{s["id"] for s in candidates}
        if extra:
            raise RuntimeError("请求导入的风格未通过源图质量审查："+",".join(sorted(extra)))
        candidates=[s for s in candidates if s["id"] in NAME_FILTER]
    if not candidates:
        raise RuntimeError("没有任何合格的导入候选风格")
    # 所有候选先验证完整四通道源文件与已有Niagara材质绑定；防止半途才发现缺失。
    for s in candidates:
        for m in s["maps"]:
            check_hash(STAGE/m["file"],m)
        for kind in ("Albedo","Normal"):
            path,_=item_path(s["id"],kind)
            if not isinstance(ED.load_asset(path),unreal.Texture2D):
                raise RuntimeError("旧纹理不具备真实UE资产："+path)
        material=ED.load_asset(PREFIX+"/Materials/MI_DBA_Ambient_"+s["id"])
        if not isinstance(material,unreal.MaterialInstanceConstant):
            raise RuntimeError("旧材质实例无效："+s["id"])
        verify_existing_niagara(s["id"],material)
    return candidates

def run():
    expected_identity()
    if MODE not in ("inspect","approved_apply"):
        raise ValueError("运行模式只支持inspect/approved_apply")
    master=ED.load_asset(MASTER)
    if not isinstance(master,unreal.Material):
        raise RuntimeError("平台通用PBR母材质尚未由UE5.8成功创建："+MASTER)
    chosen=load_source()
    OUTPUT["candidates"]=[x["id"] for x in chosen]
    OUTPUT["batchCount"]=len(chosen)
    if MODE=="inspect":
        print("AMBIENT_PBR_BATCH_INSPECT_OK",len(chosen),",".join(OUTPUT["candidates"]),flush=True)
        OUTPUT["success"]=True
        return
    free=shutil.disk_usage(ROOT).free
    if free<1_400_000_000:
        raise RuntimeError("E盘剩余空间不足1.4GB，不安全导入并保存资产，避免其他任务构建失败")
    # 建立整批原始资产备份，保留原材质身份及恢复证据。
    for s in chosen:
        name=s["id"]
        for kind in ("Albedo","Normal"):
            path,_=item_path(name,kind)
            copy_before_edit(path)
        copy_before_edit(PREFIX+"/Materials/MI_DBA_Ambient_"+name)
    for s in chosen:
        name=s["id"]
        assets={}
        for kind in ("Albedo","Normal","Roughness","Opacity"):
            spec=next((m for m in s["maps"] if m["channel"]==kind),None)
            if not spec:raise RuntimeError("缺PBR通道："+name+"/"+kind)
            assets[kind]=reimport_texture(name,kind,STAGE/spec["file"],
                     overwrite=kind in ("Albedo","Normal"))
        mat=assign_material(name,assets,master,
             (s["physicalWidthCm"],s["physicalHeightCm"]))
        renderer_count=verify_existing_niagara(name,mat)
        OUTPUT["styles"].append({"id":name,"maps":4,"miPath":mat.get_path_name(),
              "niagaraRenderersChecked":renderer_count,"visualSizeCm":[s["physicalWidthCm"],s["physicalHeightCm"]],
              "status":"ue_assets_saved_visual_pending"})
    OUTPUT["success"]=True

try:
    run()
except Exception as exc:
    OUTPUT["success"]=False
    OUTPUT["failures"].append(str(exc))
    print("AMBIENT_PBR_IMPORT_ERROR",str(exc),flush=True)
    raise
finally:
    REPORT.parent.mkdir(parents=True,exist_ok=True)
    REPORT.write_text(json.dumps(OUTPUT,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print("AMBIENT_PBR_IMPORT_REPORT",REPORT,"success",OUTPUT.get("success"),flush=True)
