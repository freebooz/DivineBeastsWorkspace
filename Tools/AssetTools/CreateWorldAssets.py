"""在真实 UE 编辑器创建 World/Region 定义，不生成地图、不改配置或已有包。

先用原 CreateFoundationAssets.py --phase Maps 生成 Sandbox；本脚本要求地图已存在。
用 -unattended -ScriptErrorsAreFatal -ExecutePythonScript=<本脚本绝对路径>，不加 -run。
输出 WORLD_ASSETS_REPORT JSON；异常留证据且不回滚其他文件。必须独占资产编辑进程。
离线导入不加载 unreal；磁盘审计复用原脚本，编辑器适配只能在 UE 中验证。
"""
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from CreateFoundationAssets import UnrealAssetEditor, native_asset_diff, require

DIRECTORY = "/Game/Development/Foundation/Definitions/"
REGION_PACKAGES = (DIRECTORY + "DA_FoundationRegionA", DIRECTORY + "DA_FoundationRegionB")
WORLD_PACKAGE = DIRECTORY + "DA_FoundationWorld"
PACKAGES = REGION_PACKAGES + (WORLD_PACKAGE,)
MAP_PACKAGE = "/Game/Development/Foundation/Maps/L_FoundationSandbox"
MAP_OBJECT = MAP_PACKAGE + ".L_FoundationSandbox"
MAP_FILE = MAP_PACKAGE[len("/Game/"):] + ".umap"
CLASS_PATHS = {
    **{p: "/Script/GamePlatformWorld.GamePlatformRegionDefinition" for p in REGION_PACKAGES},
    WORLD_PACKAGE: "/Script/GamePlatformWorld.GamePlatformWorldDefinition",
}


def filename(package):
    """限制输出为三个授权包；不能以参数扩大到其他 Content。"""
    require(package in PACKAGES, "定义包不在白名单：" + package)
    return package[len("/Game/"):] + ".uasset"


def related(snapshot, package):
    """同名地图、附属数据或外置对象同样算已占用，禁止覆盖。"""
    stem = filename(package).rsplit(".", 1)[0].casefold()
    return {k: v for k, v in snapshot.items() if k.casefold() in {
        stem + ext for ext in (".uasset", ".umap", ".uexp", ".ubulk", ".uptnl")}
        or k.casefold().startswith(tuple(p + stem + "/" for p in ("__externalactors__/", "__externalobjects__/")))}


def default_values(package):
    """项目夹具默认值；平台身份使用 LogicalId，不引入第二套 WorldId 字段。"""
    filename(package)
    name = {REGION_PACKAGES[0]: "region_a", REGION_PACKAGES[1]: "region_b", WORLD_PACKAGE: "world"}[package]
    values = dict(logical_id="foundation." + name + "@1", schema_version=1, content_revision=1, required_definitions=[])
    if package == WORLD_PACKAGE:
        ids = ["GamePlatformDefinition:foundation.region_a@1", "GamePlatformDefinition:foundation.region_b@1"]
        values.update(map_identity=MAP_OBJECT, regions=ids, required_definitions=list(ids),
                      default_experience_id="", readiness_timeout_seconds=60.0)
    else:
        values.update(parent_region_id="", region_type_tag="Development", bounds_policy="AxisAlignedBox",
                      activation_policy="AlwaysRegistered")
    return values


def validate_values(package, values):
    """纯字段验收与 UE 读取共用：身份/依赖固定，合法超时和修订差异只报告、不覆盖。"""
    expected = default_values(package)
    for field in expected.keys() - {"content_revision", "readiness_timeout_seconds", "region_type_tag"}:
        require(values.get(field) == expected[field], "定义字段不匹配：" + field)
    require(type(values.get("content_revision")) is int and values["content_revision"] > 0, "content_revision 必须为正整数")
    if package == WORLD_PACKAGE:
        seconds = values.get("readiness_timeout_seconds")
        require(type(seconds) in (int, float) and math.isfinite(seconds) and seconds > 0, "readiness_timeout_seconds 必须有限且为正")
    else:
        require(isinstance(values.get("region_type_tag"), str) and values["region_type_tag"].strip()
                and values["region_type_tag"] != "None", "region_type_tag 不能为空")
    return [{"field": field, "default": expected[field], "actual": values[field]}
            for field in expected if values[field] != expected[field]]


def run_assets(editor):
    """先区域再世界；失败即停止后续写入。每次验证之后再次对照保存快照。"""
    report = dict(exit_code=1, audit_complete=False, assets=[], errors=[], native_asset_diff=[])
    before = None
    created = set()
    validated = {}
    try:
        before = editor.snapshot()
        editor.preflight()
        for package in PACKAGES:
            item = dict(package=package, state="FAILED", custom_differences=[])
            report["assets"].append(item)
            try:
                current = editor.snapshot()
                occupied = related(current, package)
                if occupied or editor.exists(package):
                    require(filename(package) in occupied and occupied[filename(package)]["size_bytes"] > 0,
                            "已有身份缺少非空包；nooverwrite")
                    require(not any(k.endswith(".umap") for k in occupied), "同名地图冲突；nooverwrite")
                    require(editor.exists(package), "磁盘资产未注册；nooverwrite")
                    validated[package] = occupied
                    item["custom_differences"] = editor.validate(package)
                    item["state"] = "VALIDATED"
                else:
                    require(not related(editor.snapshot(), package), "创建前目标被占用；nooverwrite")
                    created.add(package)
                    editor.create(package)
                    saved = editor.snapshot()
                    require(saved.get(filename(package), {}).get("size_bytes", 0) > 0 and editor.exists(package), "保存未落盘或未注册")
                    validated[package] = related(saved, package)
                    item["custom_differences"] = editor.validate(package)
                    item["state"] = "CREATED"
            except Exception as error:
                item["message"] = str(error)
                raise
    except Exception as error:
        report["errors"].append(str(error))
    try:
        require(before is not None, "初始审计失败，无法提供完整差异")
        after = editor.snapshot()
        report["native_asset_diff"] = native_asset_diff(before, after)
        report["audit_complete"] = True
        allowed = {filename(p).rsplit(".", 1)[0] + ext for p in created for ext in (".uasset", ".uexp", ".ubulk", ".uptnl")}
        for diff in report["native_asset_diff"]:
            if diff["change"] != "CREATED" or diff["path"] not in allowed:
                report["errors"].append("发现越界原生资产差异：" + diff["path"])
        for item in report["assets"]:
            package = item["package"]
            if package in validated and related(after, package) != validated[package]:
                item["state"] = "FAILED"
                report["errors"].append("只读验证修改了资产：" + package)
    except Exception as error:
        report["errors"].append("最终审计失败：" + str(error))
    report["exit_code"] = int(bool(report["errors"]) or len(report["assets"]) != 3
                              or any(item["state"] == "FAILED" for item in report["assets"]))
    return report


class UnrealWorldEditor(UnrealAssetEditor):
    """游戏编辑器线程适配；只对新建实例赋值，不修改 CDO、不保存已有实例。"""
    def preflight(self):
        snapshot = self.snapshot()
        require(snapshot.get(MAP_FILE, {}).get("size_bytes", 0) > 0, "Sandbox 原生地图缺失；先执行 Maps 阶段")
        self.map = self.assets.load_asset(MAP_PACKAGE)
        require(self.map is not None and isinstance(self.map, self.unreal.World)
                and self.map.get_path_name() == MAP_OBJECT, "Sandbox 地图类型或路径不匹配")
        base = self.unreal.load_class(None, "/Script/GamePlatformData.GamePlatformDefinitionBase")
        require(base is not None, "Data 基类未编译加载")
        self.classes = {p: self.unreal.load_class(None, path, type=base) for p, path in CLASS_PATHS.items()}
        require(all(self.classes.values()), "World/Region 真实反射类型未编译加载")
        # 提前核对字段和枚举，避免写出第一个包后才发现反射版本不一致。
        self.bounds = self.unreal.GamePlatformRegionBoundsPolicy.AXIS_ALIGNED_BOX
        self.activation = self.unreal.GamePlatformRegionActivationPolicy.ALWAYS_REGISTERED
        for package, cls in self.classes.items():
            obj = self.unreal.get_default_object(cls)
            for field in ("logical_id", "data_version", "required_definitions"):
                obj.get_editor_property(field)
            for field in (("map_identity", "regions", "default_experience_id", "readiness_timeout_seconds")
                          if package == WORLD_PACKAGE else ("parent_region_id", "region_type_tag", "bounds_policy", "activation_policy")):
                obj.get_editor_property(field)
        self.asset_tools = self.unreal.AssetToolsHelpers.get_asset_tools()

    @staticmethod
    def logical_text(value):
        namespace, name, version = (value.get_editor_property(f) for f in ("namespace", "name", "logical_version"))
        if namespace == "" and name == "" and version == 1:
            return ""
        return f"{namespace}.{name}@{version}"

    @staticmethod
    def primary_text(value):
        return str(value.get_editor_property("primary_asset_type").get_editor_property("name")) + ":" + str(value.get_editor_property("primary_asset_name"))

    def primary_id(self, text):
        kind, name = text.split(":", 1)
        asset_type = self.unreal.PrimaryAssetType()
        asset_type.set_editor_property("name", kind)
        value = self.unreal.PrimaryAssetId()
        value.set_editor_property("primary_asset_type", asset_type)
        value.set_editor_property("primary_asset_name", name)
        return value

    def read_values(self, package, asset):
        """反射字段转成可离线验证的值；未知枚举不得被映射为默认正确值。"""
        require(asset is not None and asset.get_class() == self.classes[package], "定义类型不匹配")
        require(asset.get_path_name() == package + "." + package.rsplit("/", 1)[1], "资产重定向或对象路径不匹配")
        version = asset.get_editor_property("data_version")
        values = dict(logical_id=self.logical_text(asset.get_editor_property("logical_id")),
                      schema_version=version.get_editor_property("schema_version"),
                      content_revision=version.get_editor_property("content_revision"),
                      required_definitions=[self.primary_text(x) for x in asset.get_editor_property("required_definitions")])
        if package == WORLD_PACKAGE:
            world = asset.get_editor_property("map_identity")
            values.update(map_identity=world.get_path_name() if world else "", regions=[self.primary_text(x) for x in asset.get_editor_property("regions")],
                          default_experience_id=self.logical_text(asset.get_editor_property("default_experience_id")),
                          readiness_timeout_seconds=asset.get_editor_property("readiness_timeout_seconds"))
        else:
            values.update(parent_region_id=self.logical_text(asset.get_editor_property("parent_region_id")),
                          region_type_tag=str(asset.get_editor_property("region_type_tag")),
                          bounds_policy="AxisAlignedBox" if asset.get_editor_property("bounds_policy") == self.bounds else "Unsupported",
                          activation_policy="AlwaysRegistered" if asset.get_editor_property("activation_policy") == self.activation else "Unsupported")
        return values

    def create(self, package):
        require(not related(self.snapshot(), package) and not self.exists(package), "定义身份已占用；nooverwrite")
        factory = self.unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", self.classes[package])
        folder, name = package.rsplit("/", 1)
        asset = self.asset_tools.create_asset(name, folder, self.classes[package], factory, overwrite_existing=False)
        require(asset is not None, "创建定义失败")
        values = default_values(package)
        logical = asset.get_editor_property("logical_id")
        for field, value in (("namespace", "foundation"), ("name", values["logical_id"].split(".", 1)[1].split("@")[0]), ("logical_version", 1)):
            logical.set_editor_property(field, value)
        asset.set_editor_property("logical_id", logical)
        version = asset.get_editor_property("data_version")
        for field in ("schema_version", "content_revision"):
            version.set_editor_property(field, 1)
        asset.set_editor_property("data_version", version)
        asset.set_editor_property("required_definitions", [self.primary_id(x) for x in values["required_definitions"]])
        if package == WORLD_PACKAGE:
            asset.set_editor_property("map_identity", self.map)
            asset.set_editor_property("regions", [self.primary_id(x) for x in values["regions"]])
            asset.set_editor_property("readiness_timeout_seconds", values["readiness_timeout_seconds"])
        else:
            asset.set_editor_property("region_type_tag", values["region_type_tag"])
            asset.set_editor_property("bounds_policy", self.bounds)
            asset.set_editor_property("activation_policy", self.activation)
        require(not validate_values(package, self.read_values(package, asset)), "新定义默认值不一致，拒绝保存")
        require(not related(self.snapshot(), package), "保存前发现已有同包文件；nooverwrite")
        require(self.assets.save_loaded_asset(asset, only_if_is_dirty=False), "保存失败；保留现场")

    def validate(self, package):
        asset = self.assets.load_asset(package)
        require(asset is not None, "定义包加载失败")
        package_object = asset.get_outermost()
        del asset
        reloaded, error = self.unreal.EditorLoadingAndSavingUtils.reload_packages(
            [package_object], interaction_mode=self.unreal.ReloadPackagesInteractionMode.ASSUME_NEGATIVE)
        require(reloaded and not str(error), "定义重新加载失败：" + str(error))
        differences = validate_values(package, self.read_values(package, self.assets.load_asset(package)))
        tags = {str(k): str(v) for k, v in self.assets.get_tag_values(package).items()}
        identity = default_values(package)["logical_id"]
        for key, expected in (("PrimaryAssetType", "GamePlatformDefinition"), ("PrimaryAssetName", identity), ("GamePlatformLogicalId", identity)):
            require(tags.get(key) == expected, "真实注册表标签不匹配：" + key)
        return differences


if __name__ == "__main__":
    try:
        import unreal
        report = run_assets(UnrealWorldEditor(unreal))
    except Exception as error:
        report = dict(exit_code=1, audit_complete=False, assets=[], errors=[str(error)], native_asset_diff=[])
    print("WORLD_ASSETS_REPORT " + json.dumps(report, ensure_ascii=False, allow_nan=False))
    if report["exit_code"]:
        raise RuntimeError("World 资产生成或只读验证失败；见 WORLD_ASSETS_REPORT，禁止覆盖恢复")
