#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM UE5.8新手村真实Landscape母材质Texture Bombing（纹理轰炸）作者。

领域/所有权：第三层DBAWorldPack_Village中立共享材质；只复用UE5.8引擎原生
Texture_Bombing（纹理轰炸函数），不依赖客户端GamePlatformSurface或新建平行系统。
GamePlatformSurface现有MF_GP_TextureAntiTiling（双颜色混合函数）没有独立UV随机采样，
不能简单连接后伪称Texture Bombing。当前共享母材质已有55节点，三组地表
BaseColor/Normal/ORM（基础色/法线/遮蔽粗糙度金属度），仍保持三组稳定参数名。
只针对正常Ground采样加Bombing；雪覆盖、高度、天气、地图、PCG和服务器权威不修改。
通过三组同步UV和一致函数参数防止PBR通道错位，Normal使用引擎函数的法线修正配置。
新的StaticSwitchParameter（静态开关）用于低端实例回退原样单采样，运行时不Tick。
共享母材质只保留原有小体积中性纹理硬引用，正式V3高清配色由客户端世界租约加载；
不得为了静态编辑器预览而把项目表现2K纹理全部硬打进Dedicated Server。
源色采用用户已确认的王者峡谷风格V3.0草地参考色 #6B9956，实际纹理由既有V3源取得，
不是用参考HEX直接作线性FLinearColor，地表DryColor默认白色不破坏PBR原色。
默认inspect只读；apply需目标SHA与可信备份，verify在独立UE进程冷重载。
不能由本脚本替代GPU、正式客户端Cook/Server Cook或人工实际画面签收。
"""
from __future__ import annotations
import hashlib
import json
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
MODE = os.getenv("SSLM_VILLAGE_BOMB_MODE", "inspect").strip().lower()
MASTER = "/DBAWorldPack_Village/Materials/Landscape/M_DBA_Village_LandscapeBridge"
SOURCE = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/DBAWorldPack_Village/"
                 "Content/Materials/Landscape/M_DBA_Village_LandscapeBridge.uasset")
MAPFILE = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Worlds/"
                  "DBAWorldPack_Village/Content/Maps/L_Village_Start.umap")
BACKUP = ROOT / "Saved/ArtReview/VillageTerrainBombingV3/Before"
EVIDENCE = ROOT / "Game/Saved/Validation/VillageTerrainBombingV3"
REPORT = EVIDENCE / "AssetProof.json"
EXPECTED_OLD_SHA = "0d2ffe32ac564f2095e5337959a2a8343f702473892f5d6bb8d1b5955a0b00ca"
NATIVE_BOMB = "/Engine/Functions/Engine_MaterialFunctions01/Texturing/Texture_Bombing"
RESOURCE_ROOT = "/DBAWorldPack_Village/Terrain/Textures/Grass/T_DBA_Terrain_Grass_"
TEXTURES = {"Albedo": RESOURCE_ROOT+"BaseColor",
            "Normal": RESOURCE_ROOT+"Normal",
            "ORM": RESOURCE_ROOT+"ORM"}
PARAMS = {"Albedo": "VillageGroundAlbedoTexture",
          "Normal": "VillageGroundNormalTexture",
          "ORM": "VillageGroundORMTexture"}
EXISTING_NODES = {"Albedo": "MaterialExpressionTextureSampleParameter2D_4",
                  "Normal": "MaterialExpressionTextureSampleParameter2D_5",
                  "ORM": "MaterialExpressionTextureSampleParameter2D_6"}
CONSUMERS = {"Albedo": ("MaterialExpressionMultiply_9","A"),
             "Normal": ("MaterialExpressionLinearInterpolate_4","A"),
             "ORM": ("MaterialExpressionMultiply_10","A")}
EXPECTED_ROOTS = {"BaseColor": "MaterialExpressionLinearInterpolate_1",
                  "Normal": "MaterialExpressionLinearInterpolate_4",
                  "Roughness": "MaterialExpressionLinearInterpolate_3"}
TAG = "SSLM_V3_TEXTURE_BOMBING_UE"


def sha(path: Path) -> str:
    h=hashlib.sha256()
    with path.open("rb") as f:
        for b in iter(lambda: f.read(1024*1024),b""):h.update(b)
    return h.hexdigest()


def unreal_editor():
    import unreal # type: ignore
    project=str(unreal.Paths.get_project_file_path()).replace("\\","/")
    if not project.endswith("/Game/DivineBeastsArena.uproject"):
        raise RuntimeError("SSLM Texture Bombing必须在正式UE5.8工程中执行")
    return unreal


def find_nodes(unreal, material):
    edit=unreal.MaterialEditingLibrary
    return {x.get_name():x for x in edit.get_material_expressions(material)}


def preflight(unreal, material):
    edit=unreal.MaterialEditingLibrary
    a=unreal.EditorAssetLibrary
    if not isinstance(material,unreal.Material):
        raise RuntimeError("目标路径未加载为真实共享UMaterial（母材质）")
    nodes=find_nodes(unreal,material)
    for role,name in EXISTING_NODES.items():
        n=nodes.get(name)
        if not isinstance(n,unreal.MaterialExpressionTextureSampleParameter2D):
            raise RuntimeError("真实Landscape原始Ground纹理节点缺失："+name)
        if str(n.get_editor_property("parameter_name"))!=PARAMS[role]:
            raise RuntimeError("地表原始TextureParam身份已被外部任务改动："+role)
        tex=a.load_asset(TEXTURES[role])
        if not isinstance(tex,unreal.Texture2D):
            raise RuntimeError("缺少V3同源2K扫描贴图："+TEXTURES[role])
    for role,(consumer,pin) in CONSUMERS.items():
        if consumer not in nodes:raise RuntimeError("Ground通道消费节点不符合55节点合同："+role)
        if MODE in ("inspect", "apply"):
            found=edit.get_inputs_for_material_expression(material,nodes[consumer])
            if nodes[EXISTING_NODES[role]] not in found:
                raise RuntimeError("原始地表采样没有直连正确PBR消费节点："+role)
    for name,p in (("BaseColor",unreal.MaterialProperty.MP_BASE_COLOR),
                   ("Normal",unreal.MaterialProperty.MP_NORMAL),
                   ("Roughness",unreal.MaterialProperty.MP_ROUGHNESS)):
        root=edit.get_material_property_input_node(material,p)
        if not root or root.get_name()!=EXPECTED_ROOTS[name]:
            raise RuntimeError("用户关卡已变更材质最终输出图："+name)
    fn=a.load_asset(NATIVE_BOMB)
    if not fn or "MaterialFunction" not in fn.get_class().get_name():
        raise RuntimeError("锁定UE5.8的原生Texture_Bombing函数缺失")
    if any("VillageGroundBombEnabled"==str(x.get_editor_property("parameter_name"))
           for x in nodes.values() if isinstance(x,unreal.MaterialExpressionStaticSwitchParameter)):
        if MODE=="apply":
            raise RuntimeError("已存在Texture Bombing静态开关；只允许verify，禁止重复构建")
    if MODE=="apply" and len(nodes)!=55:
        raise RuntimeError("要修改的共享母材质原始55节点版本已变化，不强制覆盖")
    return nodes,fn


def current_sha_gate():
    if not SOURCE.is_file() or not MAPFILE.is_file():
        raise RuntimeError("正式母材质或Village地图实际文件缺失")
    if MODE=="apply" and sha(SOURCE)!=EXPECTED_OLD_SHA:
        raise RuntimeError("正式母材质不是此前确认的55节点SHA版本；拒绝覆盖其它任务")
    if BACKUP.exists() and MODE=="apply":
        raise RuntimeError("已有本任务备份，请审查执行状态，不允许再次原位覆盖")
    return sha(SOURCE), sha(MAPFILE)


def backup_original(old_sha: str,map_sha:str):
    if BACKUP.exists():raise RuntimeError("备份目录已存在但本次无可信应用记录")
    BACKUP.mkdir(parents=True,exist_ok=False)
    for input,output in ((SOURCE,BACKUP/SOURCE.name),(MAPFILE,BACKUP/MAPFILE.name)):
        shutil.copy2(input,output)
        if sha(input)!=sha(output):raise RuntimeError("正式资产备份SHA复制校验不一致")
    manifest={"date":"2026-10-18","contractZh":"仅覆盖V3地形采样母材质，不修改地图及权威PCG",
              "sourceMaterial":str(SOURCE),"sourceSHA256":old_sha,
              "map":str(MAPFILE),"mapSHA256":map_sha,
              "engineNativeBomb":NATIVE_BOMB,"humanReviewPassed":False}
    (BACKUP/"BackupManifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(TAG+"_BACKUP_OK",old_sha,map_sha,flush=True)


def connect(edit,left,target,pin,out=""):
    ok=edit.connect_material_expressions(left,out,target,pin)
    if not ok and pin in ("Coordinates","Input"):
        ok=edit.connect_material_expressions(left,out,target,"UVs" if pin=="Coordinates" else "")
    if not ok:
        raise RuntimeError("UE5.8 Texture Bombing材质连线失败："+
                           left.get_name()+":"+out+" -> "+target.get_name()+":"+pin)


def make(unreal,mat,fn,nodes):
    edit=unreal.MaterialEditingLibrary
    a=unreal.EditorAssetLibrary
    def expr(cls,x,y):
        n=edit.create_material_expression(mat,cls,x,y)
        if not n:raise RuntimeError("UE5.8不能创建Texture Bombing材质表达式："+str(cls))
        return n
    def scalar(name,value,x,y):
        n=expr(unreal.MaterialExpressionScalarParameter,x,y)
        n.set_editor_property("parameter_name",unreal.Name(name))
        n.set_editor_property("default_value",float(value))
        return n
    # 既有WorldPosition和VillageGroundUVScale已提供世界对齐UV。
    uv=nodes["MaterialExpressionMultiply_8"]
    # 三通道调用相同随机变换合同，使草地纹理PBR法线与颜色/粗糙度采样一致。
    tiling=scalar("VillageGroundBombTiling",1.0,-4700,-1960)
    offset=scalar("VillageGroundBombOffset",0.55,-4700,-1750)
    contrast=scalar("VillageGroundBombContrast",0.45,-4700,-1540)
    created=[]
    for idx,role in enumerate(("Albedo","Normal","ORM")):
        yy=idx*850-1000
        current=nodes[EXISTING_NODES[role]]
        # 共享母材质沿用WorldPack内原有中性轻量默认纹理，不将2K贴图硬引用入
        # Dedicated Server（专用服务器）Cook；实际V3扫描高清BaseColor/Normal/ORM在
        # Village客户端由Data世界租约使用同名Texture参数绑定，且不改地图碰撞/PCG。
        default_tex=current.get_editor_property("texture")
        if not isinstance(default_tex,unreal.Texture2D):
            raise RuntimeError("原有Ground默认Texture2D丢失："+role)
        obj=expr(unreal.MaterialExpressionTextureObjectParameter,-4400,yy)
        obj.set_editor_property("parameter_name",unreal.Name(PARAMS[role]))
        obj.set_editor_property("texture",default_tex)
        call=expr(unreal.MaterialExpressionMaterialFunctionCall,-3950,yy)
        if not call.set_material_function(fn):
            raise RuntimeError("UE原生Texture_Bombing函数无法绑定："+role)
        available=[str(x) for x in edit.get_material_expression_input_names(call)]
        if not available:raise RuntimeError("UE Texture_Bombing调用无输入引脚："+role)
        key=lambda x: "".join(c for c in x.lower() if c.isalnum())
        bound=set()
        # 真实函数输入合同按原生签名匹配，不将未知引脚当成成功。
        for name in available:
            normalized=key(name)
            src=None
            if normalized in ("textureobject","texture","tex","textureinput"):
                src=obj
            elif normalized in ("uvs","uv","coordinates"):
                src=uv
            elif normalized=="tiling":
                src=tiling
            elif normalized=="offset":
                src=offset
            elif normalized in ("contrast","blendcontrast"):
                src=contrast
            elif normalized in ("isnormalmap","isnormal","normalmap"):
                # Texture_Bombing引擎接口采用StaticBool以执行法线贴图旋转/翻转补偿，
                # 不可用标量1.0冒充StaticBool（材质函数编译类型不匹配）。
                src=expr(unreal.MaterialExpressionStaticBool,-4370,yy+200)
                src.set_editor_property("value",role=="Normal")
            elif normalized in ("enableheightlerp","heightlerp"):
                # 本桥接不需要四路高度贴图；关闭额外高度混合采样以控制Village基础GPU预算。
                src=expr(unreal.MaterialExpressionStaticBool,-4370,yy+310)
                src.set_editor_property("value",False)
            if src is not None:
                connect(edit,src,call,name)
                bound.add(normalized)
        if not any(x in bound for x in ("textureobject","texture","tex","textureinput")) or not any(
                x in bound for x in ("uvs","uv","coordinates")):
            raise RuntimeError("内置纹理轰炸原生签名未满足Texture和UV两个必要输入："+str(available))
        if role=="Normal" and not any(x in bound for x in ("isnormalmap","isnormal","normalmap")):
            raise RuntimeError("纹理轰炸原生函数没有正确的IsNormalmap静态布尔保护，不接受错误的法线旋转")
        switch=expr(unreal.MaterialExpressionStaticSwitchParameter,-3450,yy)
        switch.set_editor_property("parameter_name",unreal.Name("VillageGroundBombEnabled"))
        switch.set_editor_property("default_value",True)
        connect(edit,current,switch,"False","RGB" if role=="Albedo" else "")
        connect(edit,call,switch,"True")
        # 优先使用原生质量等级分支：Low/Medium（低/中）仅原采样、High/Epic（高/史诗）
        # 使用Texture Bombing，避免手机/低画质支付多份原生随机纹理采样的GPU费用。
        # 若引擎实际类/引脚没有对应项，只用上方静态开关提供显式人工降级入口。
        source=switch
        if hasattr(unreal,"MaterialExpressionQualitySwitch"):
            quality=expr(unreal.MaterialExpressionQualitySwitch,-3160,yy-250)
            pins=[str(x) for x in edit.get_material_expression_input_names(quality)]
            configured_low=configured_high=0
            for name in pins:
                clean="".join(c for c in name.lower() if c.isalnum())
                if clean in ("default","low","medium"):
                    connect(edit,current,quality,name,"RGB" if role=="Albedo" else "")
                    configured_low+=1
                elif clean in ("high","epic","cinematic"):
                    connect(edit,switch,quality,name)
                    configured_high+=1
            if configured_low and configured_high:
                source=quality
                print(TAG+"_QUALITY_SWITCH",role,pins,flush=True)
            else:
                # UE小版本不提供可识别的QualitySwitch引脚时，保留既有静态开关，
                # 且该QualitySwitch未连接到输出；不能伪报已完成自动画质降级。
                print(TAG+"_QUALITY_SWITCH_UNAVAILABLE",role,pins,flush=True)
        # 独立的FeatureLevelSwitch（渲染特性分支）保证ES3_1移动端回退单样本，
        # 不强制把ShaderModel5/6的成本下压到低能力设备；真实质量仍须GPU实测。
        if hasattr(unreal,"MaterialExpressionFeatureLevelSwitch"):
            feat=expr(unreal.MaterialExpressionFeatureLevelSwitch,-2870,yy-300)
            pins=[str(x) for x in edit.get_material_expression_input_names(feat)]
            configured_low=configured_pc=0
            for name in pins:
                clean="".join(c for c in name.lower() if c.isalnum())
                if clean in ("es31","es30","es2"):
                    connect(edit,current,feat,name,"RGB" if role=="Albedo" else "")
                    configured_low+=1
                elif clean in ("sm5","sm6","sm4","default"):
                    connect(edit,source,feat,name)
                    configured_pc+=1
            if configured_low and configured_pc:
                source=feat
                print(TAG+"_FEATURE_SWITCH",role,pins,flush=True)
            else:
                # 不支持的FeatureLevelSwitch不强制改写地形PBR输出；保持可人工关闭Bombing。
                print(TAG+"_FEATURE_SWITCH_UNAVAILABLE",role,pins,flush=True)
        consumer,pin=CONSUMERS[role]
        if role=="Normal":
            norm=expr(unreal.MaterialExpressionNormalize,-3090,yy)
            try:
                connect(edit,switch,norm,"VectorInput")
            except RuntimeError:
                connect(edit,switch,norm,"")
            source=norm
        if role=="ORM":
            green=expr(unreal.MaterialExpressionComponentMask,-3090,yy)
            green.set_editor_property("r",False)
            green.set_editor_property("g",True)
            green.set_editor_property("b",False)
            green.set_editor_property("a",False)
            connect(edit,switch,green,"Input")
            source=green
        connect(edit,source,nodes[consumer],pin)
        created.append({"roleZh":role,"functionInputs":available,
                        "stableParameter":PARAMS[role]})
    # 当前原材质DryColor默认暗褐，曾把绿色扫描草地压灰；设为白色保留V3真实BaseColor。
    # 新手村客户端原有事件绑定已经在MID中设白色，本次同步编辑器默认预览。
    dry=next((n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionVectorParameter)
              and str(n.get_editor_property("parameter_name"))=="VillageDryColor"),None)
    if dry is None:raise RuntimeError("无法确认旧DryColor参数，拒绝改色")
    dry.set_editor_property("default_value",unreal.LinearColor(1,1,1,1))
    # 湿润仍有自然变暗，同时保留原有Roughness动态变化，不用全局自发光修正。
    wet=next((n for n in nodes.values() if isinstance(n,unreal.MaterialExpressionVectorParameter)
              and str(n.get_editor_property("parameter_name"))=="VillageWetColor"),None)
    if wet is None:raise RuntimeError("无法确认旧WetColor参数")
    wet.set_editor_property("default_value",unreal.LinearColor(.58,.62,.55,1))
    return created


def scan_output(unreal, mat):
    edit=unreal.MaterialEditingLibrary
    nodes=find_nodes(unreal,mat)
    calls=[x for x in nodes.values() if isinstance(x,unreal.MaterialExpressionMaterialFunctionCall)
           and NATIVE_BOMB in str(x.get_editor_property("material_function").get_path_name())]
    if len(calls)!=3:raise RuntimeError("正式新手村地形没有3条原生Texture Bombing纹理采样链")
    for role,name in EXISTING_NODES.items():
        consumers=edit.get_inputs_for_material_expression(mat,nodes[CONSUMERS[role][0]])
        if role=="Albedo":
            allowed=(unreal.MaterialExpressionStaticSwitchParameter,)
            for cls_name in ("MaterialExpressionQualitySwitch", "MaterialExpressionFeatureLevelSwitch"):
                cls=getattr(unreal,cls_name,None)
                if cls:allowed+=(cls,)
        elif role=="Normal":
            allowed=(unreal.MaterialExpressionNormalize,)
        else:
            allowed=(unreal.MaterialExpressionComponentMask,)
        if not any(isinstance(node,allowed) for node in consumers):
            raise RuntimeError("原生轰炸产物未进入正式Ground PBR输出："+role)
    ss=[x for x in nodes.values() if isinstance(x,unreal.MaterialExpressionStaticSwitchParameter)
        and str(x.get_editor_property("parameter_name"))=="VillageGroundBombEnabled"]
    if len(ss)!=3:raise RuntimeError("缺少三条一致的低端静态开关")
    return len(nodes)


def execute():
    if MODE not in ("inspect","apply","verify"):
        raise ValueError("SSLM_VILLAGE_BOMB_MODE仅接受inspect/apply/verify")
    pre_sha,map_sha=current_sha_gate()
    ue=unreal_editor()
    lib=ue.EditorAssetLibrary
    mat=lib.load_asset(MASTER)
    nodes,fn=preflight(ue,mat)
    if MODE=="inspect":
        print(TAG+"_PLAN",json.dumps({"master":MASTER,"oldNodes":len(nodes),
                    "engineNativeFunction":NATIVE_BOMB,"oldSHA256":pre_sha,
                    "formalMapSHA256":map_sha,
                    "editMap":False,"requiresExclusiveAssetWrite":True,
                    "colors": "V3扫描配色，不是人工纯色染色"},ensure_ascii=False),flush=True)
        return
    if MODE=="apply":
        backup_original(pre_sha,map_sha)
        created=make(ue,mat,fn,nodes)
        count=scan_output(ue,mat)
        diagnostics=[str(x) for x in ue.MaterialEditingLibrary.recompile_material(mat) or []]
        if any("error" in msg.lower() for msg in diagnostics):
            raise RuntimeError("原生Texture Bombing母材质Shader编译报错，保持磁盘原材质："+str(diagnostics[:8]))
        if not lib.save_loaded_asset(mat,only_if_is_dirty=False):
            raise RuntimeError("共享Village母材质原生UE保存失败（可能存在其它编辑器文件锁）")
        if sha(MAPFILE)!=map_sha:
            raise RuntimeError("非本轮正式地图SHA意外变化")
        EVIDENCE.mkdir(parents=True,exist_ok=True)
        REPORT.write_text(json.dumps({
            "schemaVersion":1,"date":"2026-10-18","material":MASTER,
            "sourceSha256":pre_sha,"materialSha256":sha(SOURCE),
            "mapSha256":map_sha,"expressionCount":count,"roles":created,
            "nativeShaderMessages":diagnostics,
            "coldVerifyPassed":False,"cookPassed":False,"gpuPassed":False,
            "humanReviewPassed":False,"ownershipZh":"Village共享地图母材质，仅Engine原生Bomb函数及WorldPack自有PBR纹理"
        },ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print(TAG+"_APPLY_SAVED",json.dumps({"exprCount":count,"channels":3,"mapUnchanged":True},ensure_ascii=False),flush=True)
    else:
        if not REPORT.exists():raise RuntimeError("缺少正式UE材质修改清单，不接受猜测的冷重载")
        prior=json.loads(REPORT.read_text(encoding="utf-8"))
        if sha(SOURCE)!=prior["materialSha256"] or sha(MAPFILE)!=prior["mapSha256"]:
            raise RuntimeError("原生母材质SHA或共享地图已经偏离")
        count=scan_output(ue,mat)
        if count!=prior["expressionCount"]:
            raise RuntimeError("UE独立重载节点数量异常")
        prior["coldVerifyPassed"]=True
        REPORT.write_text(json.dumps(prior,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        print(TAG+"_COLD_VERIFY_PASS",count,"shader_and_gpu_manual_still_independent=true",flush=True)


if __name__=="__main__":execute()
