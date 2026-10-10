# -*- coding: utf-8 -*-
"""UE5.8 PCG金标准编辑器反射预检：只访问已编译类型，不创建或覆盖资产。"""
try:
    import unreal
except ImportError:
    unreal = None

def describe(obj, name, candidates):
    print("CHECK_PCG_REFLECT", name, type(obj))
    for key in candidates:
        try:
            value=obj.get_editor_property(key)
            print("  PRESENT", key, repr(value)[:100])
        except Exception as e:
            print("  ABSENT", key, type(e).__name__, str(e)[:135])

def main():
    if unreal is None:
        print("PCG_PYTHON_REFLECTION_INSPECT_ONLY：普通Python没有UE5.8反射模块，不创作资源")
        return
    entry=unreal.GamePlatformPCGMeshSetEntry()
    describe(entry,"MeshSetEntry",["mesh","weight","b_static_collision","static_collision","b_cast_shadow","cast_shadow"])
    pid=unreal.PrimaryAssetId()
    describe(pid,"PrimaryAssetId",["primary_asset_name","primary_asset_type"])
    pt=unreal.PrimaryAssetType()
    describe(pt,"PrimaryAssetType",["name"])
    print("PCG_PRIMARY_FROM_STRING",getattr(unreal.PrimaryAssetId,"from_string",None))
    id_=unreal.GamePlatformId()
    describe(id_,"GamePlatformId",["namespace","name","logical_version"])
    version=unreal.GamePlatformDataVersion()
    describe(version,"DataVersion",["schema_version","content_revision"])
    cls=getattr(unreal,"GamePlatformPCGMeshSetDefinition",None)
    print("PCG_DEF_CLASS",cls)
    print("PCG_DATAASSETFACTORY",getattr(unreal,"DataAssetFactory",None))
    try:
        obj=unreal.new_object(cls) if cls else None
        if obj:describe(obj,"GamePlatformPCGMeshSetDefinition",["logical_id","data_version","entries","required_definitions"])
    except Exception as e:print("TRANSIENT_CLASS_ERROR",type(e).__name__,str(e)[:200])
    policy=getattr(unreal,"GamePlatformPCGExecutionPolicy",None)
    print("PCG_POLICY",policy,"EDITOR_STATIC",getattr(policy,"EDITOR_GENERATED_STATIC",None))
    print("GOLD_PRECHECK_COMPLETE_READ_ONLY")
main()
