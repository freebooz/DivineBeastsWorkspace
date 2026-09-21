"""Foundation：由 UE5.8 编辑器创建或只读验证中立地图与开发探针。

调用形式（使用调用方解析出的绝对路径，不改变项目默认地图配置）：
    UnrealEditor-Cmd.exe <主工程.uproject> -unattended -nop4 -nosplash
        -ScriptErrorsAreFatal -ExecutePythonScript="<本脚本.py> --phase Maps"

必须在独占、无未保存编辑的编辑器进程运行。不要加 -run=PythonScript：
UE5.8 的 EditorPythonExecuter.cpp 明确拒绝 commandlet 使用
-ExecutePythonScript；LevelEditorSubsystem 依赖完整编辑器初始化。
-ScriptErrorsAreFatal 将本脚本抛出的失败转成进程非零退出，调用方必须检查退出码。

Maps 仅生成下列两张非分区地图，使用引擎 Cube、GameModeBase／DefaultPawn。
--phase Probe 独立生成唯一 DA_FoundationProbe；要求真实探针及 Data 基类已编译加载。
Probe 初始身份为 foundation.probe@1，结构版本／内容修订为1，ProbeValue=42，无必需依赖。
--phase Flow 独立生成 Definitions/DA_FoundationFlow，身份 foundation.flow@1；
RequiredDefinitions 为空，探针身份仅写入 LoadProbeDefinition 节点输入，不在生成时预载。
已有 ProbeValue 整数和合法流程图编辑只报告 custom_differences，不重置为生成默认值。
Definitions 不是阶段名；不生成蓝图、不修改项目配置或资产管理器扫描设置。
已有包（含空包、类型冲突、孤立外置数据）不覆盖、不删除、不自动修复。
NewLevel 会立即保存空地图；后续失败留下的文件在报告中可见，下次仅允许验证。
报告以 FOUNDATION_ASSETS_REPORT JSON 行写入标准输出，由调用方保存 UE 日志。
CREATED 表示保存、磁盘包检查及重新加载验证成功；VALIDATED 表示已有包只读通过；
FAILED 表示缺失、冲突、API 失败或内容不匹配。哈希差异不是 UE 运行／烘焙验收。

已核对的引擎源码：LevelEditor/Public/LevelEditorSubsystem.h 及其 Private 实现、
UnrealEd/Public/Subsystems/{EditorActor,EditorAsset,UnrealEditor}Subsystem.h、
UnrealEd/Public/FileHelpers.h、Engine/Private/{GameModeBase,DefaultPawn}.cpp。
导入本模块不加载 unreal、不生成文件，便于离线纯逻辑测试。
"""

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re


MAP_PACKAGES = (
    "/Game/Development/Foundation/Maps/L_FoundationBootstrap",
    "/Game/Development/Foundation/Maps/L_FoundationSandbox",
)
PROBE_PACKAGE = "/Game/Development/Foundation/Definitions/DA_FoundationProbe"
PROBE_CLASS_PATH = "/Script/DivineBeastsArena.DBAFoundationProbeDefinition"
FLOW_PACKAGE = "/Game/Development/Foundation/Definitions/DA_FoundationFlow"
FLOW_CLASS_PATH = "/Script/GamePlatformApplicationFlow.GamePlatformFlowDefinition"
DEFINITION_CLASS_PATH = "/Script/GamePlatformData.GamePlatformDefinitionBase"
NATIVE_SUFFIXES = (".umap", ".uasset", ".uexp", ".ubulk", ".uptnl")
CUBE_PATH = "/Engine/BasicShapes/Cube.Cube"
GAME_MODE_PATH = "/Script/Engine.GameModeBase"
PAWN_PATH = "/Script/Engine.DefaultPawn"
# 位置单位为厘米；地面上表面在 Z=0，出生点与参照物分离，允许默认球形 Pawn 移动。
ACTOR_SPECS = (
    ("FoundationGround", "StaticMeshActor", (0, 0, -10), (0, 0, 0), (40, 40, 0.2)),
    ("FoundationReferenceCube", "StaticMeshActor", (300, 0, 50), (0, 0, 0), (1, 1, 1)),
    ("FoundationSun", "DirectionalLight", (0, 0, 500), (-45, -30, 0), (1, 1, 1)),
    ("FoundationPlayerStart", "PlayerStart", (-400, 0, 150), (0, 0, 0), (1, 1, 1)),
)


def require(condition, message):
    """用显式异常保留失败检查；不能被 Python 的优化模式关闭。"""
    if not condition:
        raise RuntimeError(message)


class PhaseArgumentParser(argparse.ArgumentParser):
    """参数错误进入统一失败报告，不由 argparse 提前中断引擎日志。"""

    def error(self, message):
        raise ValueError(message)


def parse_arguments(arguments=None):
    """三个阶段分开执行；未知参数明确失败，不默认执行全部阶段。"""
    parser = PhaseArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=("Maps", "Probe", "Flow"), default="Maps", help="独立执行地图、探针或流程阶段")
    return parser.parse_args(arguments)


def map_filename(package):
    """将允许的包身份映射到项目 Content 相对路径，拒绝任意目标扩张。"""
    require(package in MAP_PACKAGES, "目标不在任务00地图白名单：" + package)
    return package[len("/Game/"):] + ".umap"


def asset_filename(package):
    """只扩展已授权探针身份，其他包仍由地图白名单严格限制。"""
    if package in (PROBE_PACKAGE, FLOW_PACKAGE):
        return package[len("/Game/"):] + ".uasset"
    return map_filename(package)


def related_files(snapshot, package):
    """找出同包文件及外置对象；孤立文件也占用身份，不允许生成时越过。"""
    stem = asset_filename(package).rsplit(".", 1)[0].casefold()
    external_prefixes = tuple(
        directory + "/" + stem + "/"
        for directory in ("__externalactors__", "__externalobjects__")
    )
    return {
        name: evidence for name, evidence in snapshot.items()
        if name.casefold().startswith(external_prefixes)
        or name.casefold() in {stem + suffix for suffix in NATIVE_SUFFIXES}
    }


def snapshot_native_assets(content_directory):
    """只读审计项目 Content 原生资产；记录大小、SHA256 和修改时间，不写伪资产。

    扫描包括外置 Actor／对象；链接或读取失败明确失败，避免不完整快照误判为空目录。
    只扫描主工程 Content，不声称审计了插件、引擎资产或整个工作空间。
    """
    root = Path(content_directory)
    require(root.is_dir() and not root.is_symlink(), "项目 Content 目录不存在或是链接")
    snapshot = {}

    def fail_walk(error):
        raise error

    for directory, subdirectories, filenames in os.walk(root, onerror=fail_walk):
        for name in subdirectories + filenames:
            path = Path(directory) / name
            require(not path.is_symlink(), "不审计链接目标：" + str(path))
        for name in filenames:
            path = Path(directory) / name
            if path.suffix.casefold() not in NATIVE_SUFFIXES:
                continue
            before = path.stat()
            digest = hashlib.sha256()
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            after = path.stat()
            require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns),
                    "审计期间资产被并发写入：" + str(path))
            snapshot[path.relative_to(root).as_posix()] = {
                "size_bytes": after.st_size, "sha256": digest.hexdigest(),
                "mtime_ns": after.st_mtime_ns,
            }
    return snapshot


def native_asset_diff(before, after):
    """计算原生文件新增、修改、删除证据；相同内容但被重写也属于修改。"""
    return [
        {"path": name, "change": "CREATED" if name not in before else
         "DELETED" if name not in after else "MODIFIED",
         "before": before.get(name), "after": after.get(name)}
        for name in sorted(before.keys() | after.keys()) if before.get(name) != after.get(name)
    ]


def failed_report(message, phase="Maps"):
    """初始化失败仍列出本阶段未完成资产；未审计时不伪造磁盘状态。"""
    require(phase in ("Maps", "Probe", "Flow"), "不支持的资产阶段：" + phase)
    packages = {"Maps": MAP_PACKAGES, "Probe": (PROBE_PACKAGE,), "Flow": (FLOW_PACKAGE,)}[phase]
    item_key = "maps" if phase == "Maps" else "assets"
    return {
        "phase": phase, "policy": "nooverwrite", "exit_code": 1,
        item_key: [{"package": package, "state": "FAILED", "message": message,
                    "before": None, "after": None} for package in packages],
        "native_asset_diff": [], "errors": [message], "audit_complete": False,
    }


def run_maps(editor):
    """只处理地图，保持现有 maps 报告字段和默认阶段兼容。"""
    return run_assets(editor, "Maps")


def run_probe(editor):
    """只处理唯一探针；不会调用地图生成或加载 Flow 类型。"""
    return run_assets(editor, "Probe")


def run_flow(editor):
    """只处理流程定义，不创建或预载其节点输入引用的探针。"""
    return run_assets(editor, "Flow")


def run_assets(editor, phase):
    """顺序创建／验证阶段白名单，汇总部分失败及只读差异，返回可序列化报告。

    editor 是同步编辑器边界，提供 snapshot、exists、create、validate；真实执行
    必须在 UE 编辑器主线程。任何异常均保留 FAILED，不回滚或清理已产生的原生文件。
    """
    report = failed_report("尚未执行", phase)
    items = report["maps" if phase == "Maps" else "assets"]
    report["errors"] = []
    try:
        before = editor.snapshot()
    except Exception as error:
        return failed_report("初始原生资产审计失败：" + str(error), phase)
    created_packages = set()
    validated_files = {}
    for item in items:
        package = item["package"]
        filename = asset_filename(package)
        item["before"] = before.get(filename)
        try:
            current = editor.snapshot()
            occupied = related_files(current, package)
            registered = editor.exists(package)
            if occupied or registered:
                require(filename in occupied and occupied[filename]["size_bytes"] > 0,
                        "已有身份缺少非空 " + Path(filename).suffix + "；nooverwrite 禁止覆盖或补写")
                conflicting_suffix = ".uasset" if phase == "Maps" else ".umap"
                require(not any(name.casefold().endswith(conflicting_suffix) and
                                "/__external" not in ("/" + name.casefold())
                                for name in occupied), "同包存在其他资产类型冲突；nooverwrite")
                require(registered, "磁盘已有资产但资产注册表不可见；nooverwrite")
                validated_files[package] = occupied
                item["custom_differences"] = editor.validate(package) or []
                item["state"] = "VALIDATED"
                item["message"] = "已有资产只读验证通过，未调用保存或修复"
            else:
                # 创建前再次确认磁盘；NewLevel 本身还会拒绝已有包。
                require(not related_files(editor.snapshot(), package), "创建前目标已被占用；nooverwrite")
                created_packages.add(package)
                editor.create(package)
                saved_snapshot = editor.snapshot()
                persisted = saved_snapshot.get(filename)
                require(persisted is not None and persisted["size_bytes"] > 0,
                        "编辑器返回成功但非空资产包未落盘")
                require(editor.exists(package), "资产保存后未进入资产注册表")
                validated_files[package] = related_files(saved_snapshot, package)
                item["custom_differences"] = editor.validate(package) or []
                item["state"] = "CREATED"
                item["message"] = "新资产保存、非空包检查及重新加载验证通过"
        except Exception as error:
            item["state"] = "FAILED"
            item["message"] = str(error) + "；nooverwrite：保留现场，不覆盖、不删除"
    try:
        after = editor.snapshot()
        report["native_asset_diff"] = native_asset_diff(before, after)
        report["audit_complete"] = True
        allowed_new_files = {
            asset_filename(package).rsplit(".", 1)[0] + suffix
            for package in created_packages
            for suffix in (Path(asset_filename(package)).suffix, ".uexp", ".ubulk", ".uptnl")
        }
        for difference in report["native_asset_diff"]:
            if difference["change"] != "CREATED" or difference["path"] not in allowed_new_files:
                report["errors"].append("原生资产出现不允许的差异：" + difference["path"])
        for item in items:
            filename = asset_filename(item["package"])
            item["after"] = after.get(filename)
            original_files = related_files(before, item["package"])
            if any(after.get(name) != evidence for name, evidence in original_files.items()):
                item["state"] = "FAILED"
                item["message"] = "已有包或外置数据被修改／删除；nooverwrite 审计失败，不自动恢复"
            elif item["state"] != "FAILED" and (
                    related_files(after, item["package"]) != validated_files[item["package"]]):
                item["state"] = "FAILED"
                item["message"] = "资产验证期间或验证后包内容发生变化，最终状态无效；不自动恢复"
    except Exception as error:
        report["errors"].append("最终原生资产审计失败：" + str(error))
        for item in items:
            item["state"] = "FAILED"
            item["message"] = "最终磁盘状态无法确认；保留部分产物，不修复"
    report["exit_code"] = int(bool(report["errors"]) or
                              any(item["state"] == "FAILED" for item in items))
    return report


class UnrealAssetEditor:
    """共享编辑器环境与只读资产审计；不加载地图、角色或任何项目定义类型。"""

    def __init__(self, unreal):
        self.unreal = unreal
        command_line = unreal.SystemLibrary.get_command_line()
        for flag in ("unattended", "ScriptErrorsAreFatal"):
            require(re.search(r"(?:^|\s)-" + flag + r"(?:\s|$)", command_line, re.IGNORECASE),
                    "必须提供 -" + flag + "，保障无人值守和失败退出")
        require("-executepythonscript=" in command_line.lower() and
                not re.search(r"(?:^|\s)-run=", command_line, re.IGNORECASE),
                "使用完整编辑器 -ExecutePythonScript，不使用 commandlet -run=PythonScript")
        self.assets = self._subsystem(unreal.EditorAssetSubsystem)
        require(not unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages() and
                not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(),
                "编辑器存在未保存修改，拒绝切图或生成")
        expected_content = Path(__file__).resolve().parents[2] / "Game/Content"
        self.content_directory = Path(unreal.Paths.convert_relative_path_to_full(
            unreal.Paths.project_content_dir())).resolve()
        require(self.content_directory == expected_content.resolve(), "当前编辑器不是脚本所属正式主工程")

    def _subsystem(self, subsystem_type):
        instance = self.unreal.get_editor_subsystem(subsystem_type)
        require(instance is not None, "所需编辑器子系统未初始化：" + str(subsystem_type))
        return instance

    def snapshot(self):
        """提供整个主工程 Content 的只读磁盘证据，不把注册表当作持久化证明。"""
        return snapshot_native_assets(self.content_directory)

    def exists(self, package):
        """注册表只作第二项检查；不能用注册表缺失推断磁盘路径可覆盖。"""
        return self.assets.does_asset_exist(package)


class UnrealMapEditor(UnrealAssetEditor):
    """UE5.8 地图适配；所有原生写入仅通过引擎地图 API 完成。"""

    def __init__(self, unreal):
        super().__init__(unreal)
        self.levels = self._subsystem(unreal.LevelEditorSubsystem)
        self.actors = self._subsystem(unreal.EditorActorSubsystem)
        self.worlds = self._subsystem(unreal.UnrealEditorSubsystem)
        require(not self.levels.is_in_play_in_editor(), "必须退出 PIE 后生成地图")
        self.game_mode = unreal.load_class(None, GAME_MODE_PATH)
        require(self.game_mode is not None, "无法加载引擎 GameModeBase")
        self.cube = unreal.load_asset(CUBE_PATH)
        require(isinstance(self.cube, unreal.StaticMesh), "引擎基础 Cube 资产缺失或类型错误")
        self._validate_observer()

    def _world(self, package):
        world = self.worlds.get_editor_world()
        require(world is not None and isinstance(world, self.unreal.World), "编辑器世界无效")
        object_path = package + "." + package.rsplit("/", 1)[1]
        require(world.get_path_name() == object_path, "当前世界身份不匹配：" + world.get_path_name())
        level = self.levels.get_current_level()
        require(level is not None and level.get_path_name() == object_path + ":PersistentLevel",
                "当前层级不是目标持久关卡，拒绝编辑／保存")
        return world

    def _validate_observer(self):
        defaults = self.unreal.get_default_object(self.game_mode)
        pawn_class = defaults.get_editor_property("default_pawn_class")
        require(pawn_class is not None and pawn_class.get_path_name() == PAWN_PATH,
                "GameModeBase 的 DefaultPawnClass 不再是引擎默认观察 Pawn")
        pawn_defaults = self.unreal.get_default_object(pawn_class)
        require(pawn_defaults.get_editor_property("add_default_movement_bindings"),
                "DefaultPawn 默认移动输入未启用")
        require(isinstance(pawn_defaults.get_editor_property("movement_component"),
                           self.unreal.FloatingPawnMovement), "DefaultPawn 缺少浮动移动组件")

    def create(self, package):
        """仅创建此前不存在的地图；空地图首次保存后只修改本次新建世界。

        初始包内容指纹在最终保存前必须保持一致；拒绝覆盖运行期间的外部变更。
        不修改默认类对象、项目配置、其他关卡或共享引擎 Cube。
        """
        require(not related_files(self.snapshot(), package) and not self.exists(package),
                "目标已存在；nooverwrite")
        require(self.levels.new_level(package, is_partitioned_world=False), "new_level 返回失败")
        world = self._world(package)
        initial_files = related_files(self.snapshot(), package)
        require(map_filename(package) in initial_files, "new_level 未持久化地图包")
        world.get_world_settings().set_editor_property("default_game_mode", self.game_mode)
        for label, class_name, position, rotation, scale in ACTOR_SPECS:
            actor = self.actors.spawn_actor_from_class(
                getattr(self.unreal, class_name), self.unreal.Vector(*position),
                self.unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]),
                transient=False,
            )
            require(actor is not None, "创建 Actor 失败：" + label)
            actor.set_actor_label(label)
            actor.set_actor_scale3d(self.unreal.Vector(*scale))
            if class_name == "StaticMeshActor":
                component = actor.get_editor_property("static_mesh_component")
                require(component.set_static_mesh(self.cube), "设置立方体网格失败：" + label)
                component.set_collision_profile_name("BlockAll")
                component.set_collision_enabled(self.unreal.CollisionEnabled.QUERY_AND_PHYSICS)
            elif class_name == "DirectionalLight":
                component = actor.get_component_by_class(self.unreal.DirectionalLightComponent)
                require(component is not None, "方向光组件缺失")
                component.set_mobility(self.unreal.ComponentMobility.MOVABLE)
                component.set_intensity(3.0)
        self._validate_current(package)
        require(related_files(self.snapshot(), package) == initial_files,
                "地图生成期间目标被外部修改；nooverwrite 拒绝保存")
        self._world(package)
        require(self.levels.save_current_level(), "save_current_level 返回失败，可能留下空地图")

    def validate(self, package):
        """从磁盘重新加载目标并检查身份、场景与观察 Pawn；不会修复或调用保存。"""
        require(self.levels.load_level(package), "load_level 返回失败")
        self._validate_current(package)

    def _validate_current(self, package):
        world = self._world(package)
        mode = world.get_world_settings().get_editor_property("default_game_mode")
        require(mode is not None and mode.get_path_name() == GAME_MODE_PATH, "地图 GameModeBase 不匹配")
        self._validate_observer()
        actors = list(self.actors.get_all_level_actors())
        require(sum(isinstance(actor, self.unreal.PlayerStart) for actor in actors) == 1,
                "地图必须有且只有一个 PlayerStart")
        for label, class_name, position, rotation, scale in ACTOR_SPECS:
            matches = [actor for actor in actors if actor.get_actor_label() == label]
            require(len(matches) == 1, "必要 Actor 缺失或标签重复：" + label)
            actor = matches[0]
            require(actor.get_class() == getattr(self.unreal, class_name).static_class(),
                    "Actor 类型不匹配：" + label)
            require(actor.get_level() == self.levels.get_current_level(), "Actor 不属于持久关卡：" + label)
            self._check_components(actor.get_actor_location(), position, ("x", "y", "z"), label)
            self._check_components(actor.get_actor_scale3d(), scale, ("x", "y", "z"), label)
            self._check_components(actor.get_actor_rotation(), rotation, ("pitch", "yaw", "roll"), label)
            if class_name == "StaticMeshActor":
                component = actor.get_editor_property("static_mesh_component")
                mesh = component.get_editor_property("static_mesh")
                require(mesh is not None and mesh.get_path_name() == CUBE_PATH, "网格资源不匹配：" + label)
                require(str(component.get_collision_profile_name()) == "BlockAll" and
                        component.get_collision_enabled() == self.unreal.CollisionEnabled.QUERY_AND_PHYSICS,
                        "碰撞配置不匹配：" + label)
                require(component.is_visible() and not actor.get_editor_property("hidden"),
                        "地面或参照物不可见：" + label)
            elif class_name == "DirectionalLight":
                component = actor.get_component_by_class(self.unreal.DirectionalLightComponent)
                require(component is not None and component.is_visible() and
                        not actor.get_editor_property("hidden"), "方向光缺失或不可见")
                require(component.get_editor_property("mobility") == self.unreal.ComponentMobility.MOVABLE,
                        "方向光必须动态运行，不产生额外烘焙资产")
                require(math.isclose(component.get_editor_property("intensity"), 3.0, abs_tol=0.001),
                        "方向光强度不匹配")

    @staticmethod
    def _check_components(actual, expected, names, label):
        # 允许引擎浮点序列化的微小误差，NaN／无穷与实际布局差异均失败。
        require(all(math.isclose(getattr(actual, name), value, rel_tol=0, abs_tol=0.001)
                    for name, value in zip(names, expected)), "Actor 变换不匹配：" + label)


class UnrealDefinitionEditor(UnrealAssetEditor):
    """保存具体开发定义；派生类指定唯一身份，不能以基类或蓝图替代。

    字段来源：DBAFoundationProbeDefinition、GamePlatformDefinitionBase、
    GamePlatformPrimaryDataAsset、GamePlatformId、GamePlatformDataVersion 的实际公开头。
    C++ GetPrimaryAssetId 未标记 UFUNCTION，不虚构 Python 同名调用；保存后的原生
    PrimaryAssetType／PrimaryAssetName 标签由引擎调用该虚函数产生，读取标签验证身份。
    """

    def __init__(self, unreal):
        super().__init__(unreal)
        # load_class 的 type 参数要求继承关系；类缺失或错误均在创建工厂之前失败。
        definition_class = unreal.load_class(None, DEFINITION_CLASS_PATH)
        require(definition_class is not None, "Data 基类反射缺失，必须先编译加载 GamePlatformData")
        self.asset_class = unreal.load_class(None, self.class_path, type=definition_class)
        require(self.asset_class is not None, "定义反射缺失或基类不匹配，禁止生成替代资产：" + self.class_path)
        defaults = unreal.get_default_object(self.asset_class)
        # 只读确认字段可见；绝不改写 CDO 或借用平台默认资产。
        logical_id = defaults.get_editor_property("logical_id")
        for field in ("namespace", "name", "logical_version"):
            logical_id.get_editor_property(field)
        data_version = defaults.get_editor_property("data_version")
        for field in ("schema_version", "content_revision"):
            data_version.get_editor_property(field)
        defaults.get_editor_property("required_definitions")
        self._preflight_payload(defaults)
        self.asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        require(self.asset_tools is not None, "AssetTools 未初始化")

    def create(self, package):
        """只给本次新建实例赋值并保存一次；工厂失败、并发占用或保存失败均保留现场。"""
        require(package == self.package, "阶段仅允许指定开发定义包")
        require(not related_files(self.snapshot(), package) and not self.exists(package),
                "定义身份已被占用；nooverwrite")
        factory = self.unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", self.asset_class)
        folder, name = package.rsplit("/", 1)
        asset = self.asset_tools.create_asset(name, folder, self.asset_class, factory,
                                             overwrite_existing=False)
        require(asset is not None, "create_asset 返回空，定义创建失败")
        self._validate_identity(asset)
        logical_id = asset.get_editor_property("logical_id")
        for field, value in (("namespace", "foundation"), ("name", self.logical_name), ("logical_version", 1)):
            logical_id.set_editor_property(field, value)
        asset.set_editor_property("logical_id", logical_id)
        data_version = asset.get_editor_property("data_version")
        for field in ("schema_version", "content_revision"):
            data_version.set_editor_property(field, 1)
        asset.set_editor_property("data_version", data_version)
        asset.set_editor_property("required_definitions", [])
        self._write_payload(asset)
        require(not self._validate_object(asset), "新建定义未得到指定生成默认值，拒绝保存")
        # create_asset 仅创建内存对象；磁盘已有任何同包文件都不得由保存覆盖。
        require(not related_files(self.snapshot(), package), "定义保存前磁盘已被占用；nooverwrite")
        require(self.assets.save_loaded_asset(asset, only_if_is_dirty=False),
                "save_loaded_asset 返回失败，保留内存／磁盘现场")

    def _validate_identity(self, asset):
        require(asset is not None and asset.get_class() == self.asset_class,
                "定义反射类型不匹配，不接受基类、其他派生类或蓝图替身")
        require(asset.get_path_name() == self.package + "." + self.package.rsplit("/", 1)[1],
                "定义对象路径不匹配，不接受重定向后的其他资产")

    def _validate_object(self, asset):
        self._validate_identity(asset)
        logical_id = asset.get_editor_property("logical_id")
        for field, expected in (("namespace", "foundation"), ("name", self.logical_name), ("logical_version", 1)):
            require(logical_id.get_editor_property(field) == expected, "定义 LogicalId 字段不匹配：" + field)
        data_version = asset.get_editor_property("data_version")
        require(data_version.get_editor_property("schema_version") == 1, "定义 schema_version 当前只支持1")
        revision = data_version.get_editor_property("content_revision")
        require(type(revision) is int and revision > 0, "定义 content_revision 必须为正整数")
        require(len(asset.get_editor_property("required_definitions")) == 0, "开发定义 RequiredDefinitions 必须为空")
        differences = self._validate_payload(asset)
        if revision != 1:
            differences.append({"field": "data_version.content_revision", "default": 1, "actual": revision})
        return differences

    def validate(self, package):
        """非交互重载磁盘包并核验字段／注册表身份；已有资产从不赋值或保存。"""
        require(package == self.package, "阶段仅允许指定开发定义包")
        asset = self.assets.load_asset(package)
        self._validate_identity(asset)
        package_object = asset.get_outermost()
        del asset  # 重载后重新取对象，不继续使用失效的旧实例包装。
        reloaded, error = self.unreal.EditorLoadingAndSavingUtils.reload_packages(
            [package_object], interaction_mode=self.unreal.ReloadPackagesInteractionMode.ASSUME_NEGATIVE)
        require(reloaded and not str(error), "定义包重载失败：" + str(error))
        differences = self._validate_object(self.assets.load_asset(package))
        tags = {str(key): str(value) for key, value in self.assets.get_tag_values(package).items()}
        for key, expected in (("PrimaryAssetType", "GamePlatformDefinition"),
                              ("PrimaryAssetName", "foundation." + self.logical_name + "@1"),
                              ("GamePlatformLogicalId", "foundation." + self.logical_name + "@1")):
            require(tags.get(key) == expected, "定义原生资产注册表标签不匹配：" + key)
        return differences


class UnrealProbeEditor(UnrealDefinitionEditor):
    """Probe 的生成默认值为42；已有合法整数视为用户编辑，只报告差异。"""

    package = PROBE_PACKAGE
    class_path = PROBE_CLASS_PATH
    logical_name = "probe"

    def _preflight_payload(self, defaults):
        defaults.get_editor_property("probe_value")

    def _write_payload(self, asset):
        asset.set_editor_property("probe_value", 42)

    def _validate_payload(self, asset):
        value = asset.get_editor_property("probe_value")
        require(type(value) is int and -(2 ** 31) <= value < 2 ** 31, "ProbeValue 必须为 int32 整数")
        return [] if value == 42 else [{"field": "probe_value", "default": 42, "actual": value}]


def default_flow_values():
    """返回独立的生成默认图；探针只作为节点输入，不进入 RequiredDefinitions。"""
    names = ("Boot", "ValidateConfiguration", "LoadProbeDefinition", "EnterSandbox", "Ready")
    return {
        "entry_node_id": "Boot", "allow_cycles": False, "max_immediate_cycle_transitions": 64,
        "nodes": [{
            "node_id": name, "executor_id": name,
            "input_definition_id": "GamePlatformDefinition:foundation.probe@1" if name == "LoadProbeDefinition" else "",
            "timeout_seconds": 120.0 if name == "EnterSandbox" else 30.0,
            "next_node_id": names[index + 1] if index + 1 < len(names) else "None", "routes": {},
        } for index, name in enumerate(names)],
    }


def validate_flow_values(values):
    """只读校验公开头声明的图约束，返回与生成默认值的差异；不替代运行时工厂校验。

    FName 按大小写等价处理；迭代遍历避免长链递归溢出。循环必须显式允许并有正预算。
    不创建节点、不加载输入定义，也不冒充调用未暴露为 UFUNCTION 的 ValidateDefinition。
    """
    def key(name):
        return str(name).casefold()

    def is_none(name):
        return key(name) in ("", "none")

    nodes = values["nodes"]
    require(bool(nodes), "Flow Nodes 不能为空")
    budget = values["max_immediate_cycle_transitions"]
    require(type(budget) is int and 1 <= budget <= 2147483647, "Flow 即时循环预算必须为正 int32")
    require(type(values["allow_cycles"]) is bool, "Flow allow_cycles 必须为布尔值")
    graph = {}
    for node in nodes:
        node_key = key(node["node_id"])
        require(not is_none(node_key) and node_key not in graph, "Flow 节点名称为空或大小写等价重复")
        require(not is_none(node["executor_id"]), "Flow ExecutorId 不能为空")
        timeout = node["timeout_seconds"]
        require(isinstance(timeout, (int, float)) and not isinstance(timeout, bool) and
                math.isfinite(timeout) and timeout > 0, "Flow TimeoutSeconds 必须有限且大于零")
        input_id = node["input_definition_id"]
        if input_id:
            asset_type, separator, logical_id = input_id.partition(":")
            require(separator and asset_type.casefold() == "gameplatformdefinition", "Flow 输入主资产类型非法")
            identity, separator, version = logical_id.partition("@")
            segments = identity.split(".")
            require(separator and len(logical_id) <= 192 and len(segments) >= 2 and
                    all(re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{0,63}", segment) for segment in segments) and
                    re.fullmatch(r"[1-9][0-9]{0,9}", version) and int(version) <= 2147483647,
                    "Flow 输入逻辑身份非法")
        route_keys = [key(route) for route in node["routes"]]
        require(all(not is_none(route) for route in route_keys) and len(set(route_keys)) == len(route_keys),
                "Flow 事件路由键为空或大小写等价重复")
        graph[node_key] = {key(target) for target in [node["next_node_id"], *node["routes"].values()]
                           if not is_none(target)}
    entry = key(values["entry_node_id"])
    require(entry in graph, "Flow EntryNodeId 不在节点集合内")
    require(all(target in graph for targets in graph.values() for target in targets), "Flow 存在悬空后继")
    reached, pending = set(), [entry]
    while pending:
        current = pending.pop()
        if current not in reached:
            reached.add(current)
            pending.extend(graph[current] - reached)
    require(len(reached) == len(graph), "Flow 存在入口不可达节点")
    if not values["allow_cycles"]:
        indegrees = dict.fromkeys(graph, 0)
        for targets in graph.values():
            for target in targets:
                indegrees[target] += 1
        pending = [node for node, count in indegrees.items() if count == 0]
        removed = 0
        while pending:
            current = pending.pop()
            removed += 1
            for target in graph[current]:
                indegrees[target] -= 1
                if indegrees[target] == 0:
                    pending.append(target)
        require(removed == len(graph), "Flow 未允许循环但存在环")
    defaults = default_flow_values()
    return [{"field": field, "default": expected, "actual": values[field]}
            for field, expected in defaults.items() if values[field] != expected]


class UnrealFlowEditor(UnrealDefinitionEditor):
    """只写真实 GamePlatformFlowDefinition 及节点结构；不实例化执行器或加载探针。"""

    package = FLOW_PACKAGE
    class_path = FLOW_CLASS_PATH
    logical_name = "flow"

    def _preflight_payload(self, defaults):
        for field in ("entry_node_id", "nodes", "allow_cycles", "max_immediate_cycle_transitions"):
            defaults.get_editor_property(field)
        node_type = getattr(self.unreal, "GamePlatformFlowNodeDefinition", None)
        require(node_type is not None, "Flow 节点结构反射缺失")
        sample = node_type()
        for field in default_flow_values()["nodes"][0]:
            sample.get_editor_property(field)

    def _write_payload(self, asset):
        values = default_flow_values()
        nodes = []
        for node_values in values["nodes"]:
            node = self.unreal.GamePlatformFlowNodeDefinition()
            for field, value in node_values.items():
                if field == "input_definition_id":
                    identity = self.unreal.PrimaryAssetId()
                    if value:
                        asset_type, asset_name = value.split(":", 1)
                        primary_type = self.unreal.PrimaryAssetType()
                        primary_type.set_editor_property("name", asset_type)
                        identity.set_editor_property("primary_asset_type", primary_type)
                        identity.set_editor_property("primary_asset_name", asset_name)
                    value = identity
                node.set_editor_property(field, value)
            nodes.append(node)
        for field, value in values.items():
            asset.set_editor_property(field, nodes if field == "nodes" else value)

    def _validate_payload(self, asset):
        values = {field: asset.get_editor_property(field) for field in
                  ("allow_cycles", "max_immediate_cycle_transitions")}
        values["entry_node_id"] = str(asset.get_editor_property("entry_node_id"))
        values["nodes"] = []
        for node in asset.get_editor_property("nodes"):
            identity = node.get_editor_property("input_definition_id")
            asset_type = str(identity.get_editor_property("primary_asset_type").get_editor_property("name"))
            asset_name = str(identity.get_editor_property("primary_asset_name"))
            # 无效默认身份的两个 Name 必须都为 None；半个有效身份不能伪装成空输入。
            input_id = "" if asset_type.casefold() == asset_name.casefold() == "none" else asset_type + ":" + asset_name
            values["nodes"].append({
                "node_id": str(node.get_editor_property("node_id")),
                "executor_id": str(node.get_editor_property("executor_id")),
                "input_definition_id": input_id,
                "timeout_seconds": node.get_editor_property("timeout_seconds"),
                "next_node_id": str(node.get_editor_property("next_node_id")),
                "routes": {str(event): str(target) for event, target in node.get_editor_property("routes").items()},
            })
        return validate_flow_values(values)


def main(arguments=None):
    """输出真实执行报告；失败抛异常供 -ScriptErrorsAreFatal 转成非零进程退出。"""
    phase = "Maps"
    try:
        phase = parse_arguments(arguments).phase
        import unreal
        adapter_type = {"Maps": UnrealMapEditor, "Probe": UnrealProbeEditor, "Flow": UnrealFlowEditor}[phase]
        report = run_assets(adapter_type(unreal), phase)
    except Exception as error:
        report = failed_report(str(error), phase)
    print("FOUNDATION_ASSETS_REPORT " + json.dumps(report, ensure_ascii=False, sort_keys=True), flush=True)
    if report["exit_code"]:
        raise RuntimeError("Foundation " + phase + " FAILED；请检查 FOUNDATION_ASSETS_REPORT，禁止自动覆盖重试")
    return report


if __name__ == "__main__":
    main()
