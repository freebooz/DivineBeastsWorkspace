#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""服务器表现资源排除策略：Build层单源生成、漂移检查和真实Stage/Pak/IoStore验收。

调用方为CookFoundation及Foundation回归；只处理三个固定服务器INI中的受管段。
配置生成不会创建资源、改变地图、AlwaysCook、开发技能或构建收据。审计只读实际Stage，
逐容器运行锁定UnrealPak并留存命令/退出码/原始清单；超时或取消只影响本次子进程。
JSON Schema描述文件用于编辑器提示，本模块以标准库严格校验同一封闭结构及资源边界。
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[2]
DEFAULT_POLICY = Path(__file__).with_name("ServerPresentationExclusions.json")
PROFILES = ("DedicatedServer", "VillageServer", "VillageDevelopmentServer")
COOK_CONFIGS = (*PROFILES, "FoundationStandalone")
PAK_CONFIG = "Game/Config/Custom/DedicatedServer/DefaultPakFileRules.ini"
BEGIN_COOK = "; BEGIN GENERATED SERVER PRESENTATION EXCLUSIONS"
END_COOK = "; END GENERATED SERVER PRESENTATION EXCLUSIONS"
BEGIN_PAK = "; BEGIN GENERATED SERVER PRESENTATION PAK RULES"
END_PAK = "; END GENERATED SERVER PRESENTATION PAK RULES"
PACKAGING_SECTION = "[/Script/UnrealEd.ProjectPackagingSettings]"
PAK_SECTION = "[ExcludeProjectClientPresentationFromDedicatedServer]"
ASSET_EXTENSIONS = (".uasset", ".umap")
# 权威所有者绝不能被误作为纯表现根剔除；开发UI和下方已核验水材质叶子是窄范围例外。
AUTHORITY_ROOTS = ("/DBAGameplay", "/DBAWorldPack_Village", "/GamePlatformWeather",
                   "/DBAServer", "/DBAArena", "/Game/Development/Foundation")
ERROR_LOG = re.compile(r"(?:Log[A-Za-z0-9_]+:\s*)?(?:Error|Fatal):", re.I)
METADATA_CHUNKS = frozenset(("ScriptObjects", "ContainerHeader", "ShaderCodeLibrary",
                           "ShaderCode", "PackageStoreEntry", "DerivedData", "EditorDerivedData"))


class PolicyError(RuntimeError):
    """配置、来源或证据不成立时的拒绝结果；不得转成Cook成功。"""


@dataclass(frozen=True)
class ArtifactEntry:
    """一次真实文件或容器条目；Size为字节，ChunkType保留工具语义用于判定包数据。"""
    filename: str
    size: int
    chunk_type: str
    platform: str


def _closed_fields(value: dict, fields: set[str], label: str) -> None:
    """要求字段完整且无未知扩展，避免拼写错误静默改变规则范围。"""
    if not isinstance(value, dict) or set(value) != fields:
        raise PolicyError(f"{label}字段必须为 {sorted(fields)}")


def _without_duplicate_keys(pairs):
    """JSON重复键在解析前失败，防止后一个值悄悄覆盖先前的profile或规则。"""
    result = {}
    for key, value in pairs:
        if key in result:
            raise PolicyError(f"JSON重复键: {key}")
        result[key] = value
    return result


def _under(path: str, root: str) -> bool:
    """按目录段匹配；Foo不包含Foobar，大小写由调用方统一。"""
    return path == root or path.startswith(root + "/")


def _evidence_path(value: str) -> str:
    """规范化引擎挂载前缀和Windows分隔符；只允许开头的../，拒绝内部逃逸。"""
    if not isinstance(value, str) or not value.strip() or any(ord(c) < 32 for c in value):
        raise PolicyError("证据中的文件路径为空或含控制字符")
    parts = value.strip().replace("\\", "/").split("/")
    parts = [part for part in parts if part not in ("", ".")]
    while parts and parts[0] == "..":
        parts.pop(0)
    if parts and re.fullmatch(r"[A-Za-z]:", parts[0]):
        parts.pop(0)
    if not parts or ".." in parts or any(any(c in part for c in '*?:"<>|') for part in parts):
        raise PolicyError(f"非法证据路径: {value}")
    return "/".join(parts).casefold()


def _package_variants(package: str) -> list[str]:
    """相同地图目录的外置Actor/对象仍归该目录；不使用泛化外置根匹配。"""
    root, separator, tail = package[1:].partition("/")
    if not separator:
        return [package]
    return [package, f"/{root}/__ExternalActors__/{tail}", f"/{root}/__ExternalObjects__/{tail}"]


def _source_variants(source: str) -> list[str]:
    """将外置目录映射到同一Content挂载根；完整插件根本身已覆盖所有外置文件。"""
    parts = source.split("/")
    if "Content" not in parts or parts[-1] == "Content":
        return [source]
    index = parts.index("Content")
    prefix, tail = "/".join(parts[:index + 1]), "/".join(parts[index + 1:])
    return [source, f"{prefix}/__ExternalActors__/{tail}", f"{prefix}/__ExternalObjects__/{tail}"]


def _path_candidates(filename: str) -> set[str]:
    """接受长包名、UAT相对Stage路径和UnrealPak挂载文件名，保留完整目录边界。"""
    normalized = _evidence_path(filename)
    parts = normalized.split("/")
    candidates = {normalized}
    for index, part in enumerate(parts):
        if part == "plugins":
            candidates.add("/".join(parts[index:]))
        if part == "content" and (index == 0 or parts[index - 1] in ("game", "divinebeastsarena")):
            candidates.add("/".join(parts[index:]))
    if parts[0] == "game":
        candidates.add("content/" + "/".join(parts[1:]))
    return candidates


def _virtual_package(filename: str) -> str | None:
    """从实际项目/插件Content文件名还原长包名，仅用于核查本次请求地图确实交付。"""
    normalized = _evidence_path(filename)
    if not normalized.endswith(ASSET_EXTENSIONS):
        return None
    parts = normalized.rsplit(".", 1)[0].split("/")
    if parts[0] == "game":
        return "/" + "/".join(parts)
    for index, part in enumerate(parts):
        if part == "content":
            if index == 0 or parts[index - 1] in ("game", "divinebeastsarena"):
                return "/game/" + "/".join(parts[index + 1:])
            if "plugins" in parts[:index]:
                return "/" + parts[index - 1] + "/" + "/".join(parts[index + 1:])
    # 直接输入的插件长包名，例如/DBAGameplay/Definitions/A.uasset。
    if parts[0].startswith(("dba", "gameplatform")):
        return "/" + "/".join(parts)
    return None


def _replace_managed(text: str, begin: str, end: str, lines: list[str]) -> tuple[str, str]:
    """只替换唯一、完整的受管段；缺少/重复标记时拒绝猜测迁移，不动段外字节。"""
    pattern = re.compile(r"^" + re.escape(begin) + r"\r?\n.*?^" + re.escape(end) + r"(?=\r?$)", re.M | re.S)
    if text.count(begin) != 1 or text.count(end) != 1:
        raise PolicyError(f"受管段标记缺失或重复: {begin}")
    match = pattern.search(text)
    if not match:
        raise PolicyError(f"受管段顺序或换行无效: {begin}")
    newline = "\r\n" if "\r\n" in match.group(0) else "\n"
    block = newline.join([begin, *lines, end])
    return text[:match.start()] + block + text[match.end():], text[:match.start()] + text[match.end():]


def _unmanaged_ini_lines(text: str, begin: str, end: str):
    """保留INI节状态读取段外指令；END注释不结束节，其他节的同名键不归此工具管理。"""
    managed, section = False, ""
    for line in text.splitlines():
        stripped = line.strip()
        if stripped == begin:
            managed = True
            continue
        if stripped == end:
            managed = False
            continue
        match = re.fullmatch(r"\[\s*([^\]]+?)\s*\]", stripped)
        if match:
            section = match.group(1).strip().casefold()
        if not managed and stripped and not stripped.startswith((";", "#")):
            yield section, stripped


def _ini_directive(line: str, *, native: bool) -> tuple[str, str, str] | None:
    """按锁定UE5.8消费者识别指令，不把未知前缀反复剥离成受管键。

    ConfigCacheIni要求等号，先跳过一个~，再消费一个+-!.@*^命令；命令后不去前导空白。
    UBT ConfigFile仅消费+-!，随后跳过空格/Tab；! RemoveKey允许没有等号和值。
    Game配置按两种消费者的识别结果预检，Pak按实际UBT语义；不声称两者完全等价。
    """
    key, separator, value = line.partition("=")
    command = ""
    if native:
        if not separator:
            return None
        key = key.lstrip()
        if key.startswith("~"):
            key = key[1:]
        if key[:1] in ("+", "-", ".", "!", "@", "*", "^"):
            command, key = key[0], key[1:]
        key = key.rstrip()
    else:
        if key[:1] in ("+", "-", "!"):
            command, key = key[0], key[1:].lstrip(" \t")
        if not separator and command != "!":
            return None
        key = key.rstrip(" \t") if separator else key.strip()
    return (key.casefold(), command, value.strip()) if key else None


class ServerPresentationPolicy:
    """持有经校验的只读规则快照；不存在全局动态扫描或默认未知profile回退。"""

    def __init__(self, raw: dict):
        """构造本次真源快照及固定Stage匹配表；不在逐文件审计时重复生成目录规则。"""
        self.raw = raw
        rules = raw["CookDirectories"] + raw["StageOnlyDirectories"]
        self._stage_roots = tuple(sorted(
            {path[1:].casefold() for rule in rules for path in _package_variants(rule["PackagePath"])}
            | {path[5:].casefold() for rule in rules for path in _source_variants(rule["SourceDirectory"])}))

    def resolve_profile(self, custom_config: str) -> str:
        """只接受已登记实际Cook配置，FoundationStandalone仅映射服务器兼容调用。"""
        try:
            return self.raw["CookConfigProfiles"][custom_config]
        except KeyError as exc:
            raise PolicyError(f"未知服务器Cook profile: {custom_config}") from exc

    def cook_paths(self, profile: str) -> list[str]:
        """返回该服务器profile的排序、去重NeverCook目录，含同目录外置对象。"""
        if profile not in PROFILES:
            raise PolicyError(f"未知服务器profile: {profile}")
        paths = {path for rule in self.raw["CookDirectories"]
                 if rule["Profiles"] in ("AllServers", profile)
                 for path in _package_variants(rule["PackagePath"])}
        return sorted(paths)

    def cooker_arguments(self, custom_config: str) -> list[str]:
        """Foundation兼容入口每目录追加独立-ini数组项；其他profile使用已校验INI。

        锁定UE5.8 ConfigCacheIni.cpp的+Key为ArrayAdd，不用Set或ClearArray。
        单token只含一个安全ASCII路径，避免引擎已声明不支持的逗号/嵌套引号组合。
        """
        profile = self.resolve_profile(custom_config)
        if custom_config != "FoundationStandalone":
            return []
        prefix = "-ini:Game:[/Script/UnrealEd.ProjectPackagingSettings]:+DirectoriesToNeverCook="
        return [prefix + "(Path=" + path + ")" for path in self.cook_paths(profile)]

    def render_config(self, profile: str, text: str) -> str:
        """确定性更新NeverCook受管段；所有地图、AlwaysCook、开发开关及其他节原样保留。"""
        paths = self.cook_paths(profile)
        lines = ["; 真源: Build/Game/ServerPresentationExclusions.json；请用策略工具生成。",
                 "; 仅纯表现目录及其外置Actor/对象；不能据此声称真实Cook通过。",
                 PACKAGING_SECTION, *[f'+DirectoriesToNeverCook=(Path="{path}")' for path in paths]]
        rendered, _ = _replace_managed(text, BEGIN_COOK, END_COOK, lines)
        # 允许开发权威技能的精确撤销；受管节的Clear/Set/撤销不因空白或大小写绕过检查。
        governed = {path.casefold() for path in paths}
        for section, line in _unmanaged_ini_lines(text, BEGIN_COOK, END_COOK):
            if section != PACKAGING_SECTION[1:-1].casefold():
                continue
            for native in (True, False):
                directive = _ini_directive(line, native=native)
                if directive is None or directive[0] != "directoriestonevercook":
                    continue
                _, command, raw_value = directive
                # Set/!/^(清空)和@/*(改变结构数组合并)均不能在真源段外影响受管数组。
                if command not in ("+", "-", "."):
                    raise PolicyError(f"{profile}段外重置或改变NeverCook数组语义")
                value = re.fullmatch(r'\(\s*Path\s*=\s*"?([A-Za-z0-9_/]+)"?\s*\)\s*(?:;.*)?', raw_value, re.I)
                if not value:
                    raise PolicyError(f"{profile}段外NeverCook目录值无效")
                if value.group(1).casefold() in governed:
                    raise PolicyError(f"{profile}段外重复/撤销受管目录: {value.group(1)}")
        return rendered

    def render_pak(self, text: str) -> str:
        """从同一真源生成专服收据的Pak规则；只保留已确认的精确插件/内容目录。"""
        directories = {source[5:] for group in ("CookDirectories", "StageOnlyDirectories")
                       for rule in self.raw[group] for source in _source_variants(rule["SourceDirectory"])}
        lines = ["; 真源: Build/Game/ServerPresentationExclusions.json；精确目录边界，无英雄通配符。",
                 "; Common/十二英雄当前均为表现；权威定义及必要权威资源归DBAGameplay。",
                 PAK_SECTION, "bExcludeFromPaks=true", "bOverrideChunkManifest=true",
                 *[f'+Files=".../{path}/..."' for path in sorted(directories)]]
        rendered, _ = _replace_managed(text, BEGIN_PAK, END_PAK, lines)
        for section, line in _unmanaged_ini_lines(text, BEGIN_PAK, END_PAK):
            directive = _ini_directive(line, native=False)
            if section == PAK_SECTION[1:-1].casefold() and directive is not None and directive[0] in (
                    "files", "bexcludefrompaks", "boverridechunkmanifest"):
                raise PolicyError("段外覆盖DedicatedServer受管Pak规则")
        return rendered

    def config_paths(self) -> list[str]:
        """可修改的文件集合为三个固定服务器Game配置与一个既有Pak配置。"""
        return [self.raw["Profiles"][profile] for profile in PROFILES] + [PAK_CONFIG]

    def planned_configs(self, workspace: Path) -> dict[str, tuple[bytes, bytes]]:
        """在写入前读取并验证全部文件；保留原始BOM/换行，返回前后字节而不写文件。"""
        result = {}
        for profile, relative in self.raw["Profiles"].items():
            path = workspace / relative
            if not path.is_file():
                raise PolicyError(f"缺少受管配置: {relative}")
            before = path.read_bytes()
            result[relative] = (before, self.render_config(profile, before.decode("utf-8")).encode("utf-8"))
        path = workspace / PAK_CONFIG
        if not path.is_file():
            raise PolicyError(f"缺少受管配置: {PAK_CONFIG}")
        before = path.read_bytes()
        result[PAK_CONFIG] = (before, self.render_pak(before.decode("utf-8")).encode("utf-8"))
        return result

    def check_configs(self, workspace: Path) -> dict:
        """漂移即失败，Cook前调用；返回哈希只证明本次配置快照一致。"""
        plans = self.planned_configs(workspace)
        drift = [relative for relative, (before, after) in plans.items() if before != after]
        if drift:
            raise PolicyError("服务器排除配置漂移: " + ", ".join(drift))
        return {"ConfigFiles": list(plans), "ConfigSha256": {
            relative: hashlib.sha256(before).hexdigest() for relative, (before, _) in plans.items()}}

    def is_forbidden(self, filename: str, profile: str) -> bool:
        """检查实际产物的所有纯表现禁入路径；Stage集合含既有更宽的纯表现插件根。"""
        if profile not in PROFILES:
            raise PolicyError(f"未知服务器profile: {profile}")
        candidates = _path_candidates(filename)
        # 三profile共用DedicatedServer收据，因此最终Stage边界采用全部纯表现规则的并集。
        return any(_under(candidate, root) for candidate in candidates for root in self._stage_roots)

    def validate_entries(self, entries: list[ArtifactEntry], profile: str,
                         required_maps: tuple[str, ...] = ()) -> dict:
        """逐文件拒绝纯表现泄漏；要求非空实际包数据，并验证本次请求地图，非运行验收。"""
        if not entries:
            raise PolicyError("空产物清单不能证明服务器资源排除")
        forbidden = [entry.filename for entry in entries if entry.filename and self.is_forbidden(entry.filename, profile)]
        if forbidden:
            raise PolicyError("服务器产物含纯表现资源: " + "; ".join(sorted(set(forbidden))[:20]))
        assets = [entry for entry in entries if entry.filename.casefold().endswith(ASSET_EXTENSIONS)
                  and entry.chunk_type in ("ExportBundleData", "PakFile", "LooseFile") and entry.size > 0]
        if not assets:
            raise PolicyError("清单没有非空服务器包数据；元数据/可执行文件不能替代Cook内容")
        packages = {_virtual_package(entry.filename) for entry in assets}
        missing = [path for path in required_maps if path.casefold() not in packages]
        if missing:
            raise PolicyError("实际服务器产物缺少本次请求地图: " + ", ".join(missing))
        return {"Entries": len(entries), "AssetEntries": len(assets), "ForbiddenEntries": 0,
                "RequiredMaps": list(required_maps)}


def load_policy(path: Path, workspace: Path = WORKSPACE) -> ServerPresentationPolicy:
    """读取封闭Schema v1，核查精确挂载/源码对应、实际目录或明示历史缺失，失败无副作用。"""
    try:
        raw = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=_without_duplicate_keys)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise PolicyError(f"无法读取规则真源: {path}: {exc}") from exc
    _closed_fields(raw, {"$schema", "SchemaVersion", "Description", "Profiles", "CookConfigProfiles",
                         "CookDirectories", "StageOnlyDirectories"}, "规则")
    if type(raw["SchemaVersion"]) is not int or raw["SchemaVersion"] != 1:
        raise PolicyError("只支持SchemaVersion=1")
    if raw["$schema"] != "./ServerPresentationExclusions.schema.json" or not isinstance(raw["Description"], str) or not raw["Description"].strip():
        raise PolicyError("Schema标识或中文规则说明缺失")
    _closed_fields(raw["Profiles"], set(PROFILES), "Profiles")
    for profile, relative in raw["Profiles"].items():
        if relative != f"Game/Config/Custom/{profile}/DefaultGame.ini":
            raise PolicyError(f"不允许改写profile路径: {profile}")
    _closed_fields(raw["CookConfigProfiles"], set(COOK_CONFIGS), "CookConfigProfiles")
    for custom, profile in raw["CookConfigProfiles"].items():
        if profile != (custom if custom in PROFILES else "DedicatedServer"):
            raise PolicyError(f"不允许重定向已有Cook profile: {custom}")
    for group in ("CookDirectories", "StageOnlyDirectories"):
        rules = raw[group]
        if not isinstance(rules, list) or not rules:
            raise PolicyError(f"{group}必须是非空规则数组")
        seen = set()
        for rule in rules:
            fields = {"PackagePath", "SourceDirectory", "Availability", "Reason"}
            if group == "CookDirectories":
                fields.add("Profiles")
            _closed_fields(rule, fields, group)
            package, source = rule["PackagePath"], rule["SourceDirectory"]
            if not isinstance(package, str) or not re.fullmatch(r"/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)*", package):
                raise PolicyError(f"危险或泛化包路径: {package}")
            if package.casefold() in seen:
                raise PolicyError(f"重复排除目录: {package}")
            seen.add(package.casefold())
            # 世界包同时拥有地图/权威水域与纯视觉预设。仅准许明确登记的视觉叶子；
            # Ferns由独立装饰类剥离，真实Cook结果仍须另验，地图与权威AI定义禁止剥离。
            is_reviewed_world_visual_leaf = package.casefold() in (
                "/dbaworldpack_village/water/materials",
                "/dbaworldpack_village/water/textures",  # 只供水下材质采样，CPU水域不依赖纹理
                "/dbaworldpack_village/water/blueprints",
                "/dbaworldpack_village/creatures/wolf",
                "/dbaworldpack_village/creatures/vampirealusth",
                "/dbaworldpack_village/pcg/ferns",
                "/dbaworldpack_village/development/wind",  # 独立植物风审核夹具，不是正式地图/权威定义
            )
            if package in ("/Game", "/Engine", "/Game/Development") or (not is_reviewed_world_visual_leaf and any(
                    _under(package.casefold(), root.casefold()) or _under(root.casefold(), package.casefold())
                    for root in AUTHORITY_ROOTS)):
                raise PolicyError(f"不允许排除权威或泛化根目录: {package}")
            # Windows包路径按大小写无关的目录段保护祖先/后代，仅UI子树可作为表现排除。
            development_abilities = "/game/development/divinebeasts/abilities"
            normalized_package = package.casefold()
            if (_under(normalized_package, development_abilities) or _under(development_abilities, normalized_package)) and not _under(
                    normalized_package, development_abilities + "/ui"):
                raise PolicyError("只能排除开发技能UI子目录，不能剥离其权威技能定义")
            if not isinstance(source, str) or not re.fullmatch(r"Game/(?:Content|Plugins)/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+", source):
                raise PolicyError(f"危险或泛化源目录: {source}")
            parts = source.split("/")
            if source.startswith("Game/Content/"):
                expected = "/Game/" + source[len("Game/Content/"):]
                if group == "StageOnlyDirectories":
                    raise PolicyError("项目内容使用CookDirectories，不增加额外Stage-only剥离")
            elif group == "CookDirectories" and "Content" in parts:
                index = parts.index("Content")
                if index < 4:
                    raise PolicyError(f"无效插件Content位置: {source}")
                expected = "/" + parts[index - 1] + ("/" + "/".join(parts[index + 1:]) if parts[index + 1:] else "")
            elif group == "StageOnlyDirectories" and len(parts) >= 4 and "Content" not in parts:
                expected = "/" + parts[-1]
            else:
                raise PolicyError(f"源目录不是明确内容或插件根: {source}")
            if expected != package:
                raise PolicyError(f"挂载目录与源目录不一致: {package} / {source}")
            if rule["Availability"] not in ("Present", "LegacyAbsent") or not isinstance(rule["Reason"], str) or not rule["Reason"].strip():
                raise PolicyError(f"规则必须说明资源存在状态与中文理由: {package}")
            if group == "CookDirectories" and rule["Profiles"] not in ("AllServers", "VillageDevelopmentServer"):
                raise PolicyError(f"未知目录profile选择: {package}")
            exists = (workspace / source).is_dir()
            if (rule["Availability"] == "Present") != exists:
                raise PolicyError(f"目录存在性与规则不符，需重新核对: {source} ({rule['Availability']})")
            if source.startswith("Game/Plugins/") and exists:
                plugin_root = workspace.joinpath(*parts[:parts.index("Content")]) if "Content" in parts else workspace / source
                if not (plugin_root / (plugin_root.name + ".uplugin")).is_file():
                    raise PolicyError(f"不是独立真实插件根: {plugin_root}")
    return ServerPresentationPolicy(raw)


def read_iostore_csv(path: Path) -> list[ArtifactEntry]:
    """读取UE5.8 ListContainer CSV；拒绝空/截断/错误平台/无名资源，不能只相信工具退出0。"""
    try:
        with path.open("r", encoding="utf-8-sig", newline="") as handle:
            rows = list(csv.reader(handle, skipinitialspace=True, strict=True))
    except (OSError, UnicodeError, csv.Error) as exc:
        raise PolicyError(f"无效IoStore CSV: {path}: {exc}") from exc
    if len(rows) < 2:
        raise PolicyError(f"IoStore CSV为空或只有表头: {path}")
    header = [name.strip() for name in rows[0]]
    required = {"Filename", "ChunkType", "Size", "Platform"}
    if not required.issubset(header) or len(set(header)) != len(header):
        raise PolicyError(f"IoStore CSV字段损坏: {path}")
    entries = []
    for index, values in enumerate(rows[1:], 2):
        if len(values) != len(header):
            raise PolicyError(f"IoStore CSV第{index}行列数错误: {path}")
        row = dict(zip(header, (value.strip() for value in values)))
        filename, chunk, platform = row["Filename"], row["ChunkType"], row["Platform"]
        if not re.fullmatch(r"[0-9]+", row["Size"]) or int(row["Size"]) <= 0:
            raise PolicyError(f"IoStore CSV第{index}行尺寸无效: {path}")
        if not chunk:
            raise PolicyError(f"IoStore CSV第{index}行缺少ChunkType")
        if (filename,chunk) in (("<ContainerHeader>","ContainerHeader"),("<ScriptObjects>","ScriptObjects")):
            # UE5.8真实ListContainer将容器头写成精确占位标记，并非文件路径。
            # global.utoc的ScriptObjects没有目标平台，真实CSV为Unknown；仅精确global容器例外。
            # 其余元数据仍须专服目标；不允许实际资产借Unknown或改块类型绕过路径审计。
            global_script_objects = chunk == "ScriptObjects" and platform == "Unknown" and row.get("ContainerName") == "global"
            if platform != "WindowsServer" and not global_script_objects:
                raise PolicyError(f"IoStore容器头目标不是WindowsServer: {platform}")
            filename = ""
        elif not filename or filename == "-":
            if chunk not in METADATA_CHUNKS:
                raise PolicyError(f"IoStore CSV第{index}行资源名称丢失")
            filename = ""  # 明确元数据不能计为资产存在，也不解释成任意文件名。
        else:
            _evidence_path(filename)
            if filename.casefold().endswith(ASSET_EXTENSIONS) and platform != "WindowsServer":
                raise PolicyError(f"IoStore资源目标不是WindowsServer: {platform}")
        entries.append(ArtifactEntry(filename, int(row["Size"]), chunk, platform))
    return entries


def read_pak_listing(path: Path) -> list[ArtifactEntry]:
    """读取UnrealPak -List真实日志；要求完整文件总数与字节总数，截断/错误日志拒绝。"""
    try:
        text = path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeError) as exc:
        raise PolicyError(f"无法读取Pak清单: {path}") from exc
    if ERROR_LOG.search(text):
        raise PolicyError(f"Pak工具报告错误: {path}")
    matches = re.findall(r'LogPakFile:\s*(?:Display:\s*)?"([^"]+)" offset:\s*\d+,\s*size:\s*(\d+) bytes,', text)
    totals = re.findall(r"LogPakFile:\s*(?:Display:\s*)?(\d+) files \((\d+) bytes\),", text)
    if len(totals) != 1 or not matches or int(totals[0][0]) != len(matches) or int(totals[0][1]) != sum(int(size) for _, size in matches):
        raise PolicyError(f"Pak清单为空、截断或计数不符: {path}")
    entries = []
    for filename, size in matches:
        _evidence_path(filename)
        if filename.casefold().endswith(ASSET_EXTENSIONS) and int(size) <= 0:
            raise PolicyError(f"Pak资源为空: {filename}")
        entries.append(ArtifactEntry(filename, int(size), "PakFile", "WindowsServer"))
    return entries


def _inspect_container(arguments: list[str], directory: Path, timeout_seconds: int) -> dict:
    """只启动本次只读UnrealPak进程；逐参数传递，超时/取消清理自身进程并保留原始日志。"""
    directory.mkdir(parents=True, exist_ok=False)
    facts = {"Arguments": arguments, "ExitCode": None, "Completed": False}
    try:
        with (directory / "stdout.log").open("wb") as stdout, (directory / "stderr.log").open("wb") as stderr:
            result = subprocess.run(arguments, stdout=stdout, stderr=stderr, timeout=timeout_seconds, check=False)
        facts.update(ExitCode=result.returncode, Completed=True)
        output = (directory / "stdout.log").read_text(encoding="utf-8-sig", errors="replace")
        output += (directory / "stderr.log").read_text(encoding="utf-8-sig", errors="replace")
        if result.returncode != 0 or ERROR_LOG.search(output):
            raise PolicyError(f"容器工具失败或输出错误: {directory} (exit={result.returncode})")
        return facts
    except subprocess.TimeoutExpired as exc:
        facts["TimedOut"] = True
        raise PolicyError(f"容器检查超时，未验收: {directory}") from exc
    finally:
        (directory / "process.json").write_text(json.dumps(facts, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def audit_stage(policy: ServerPresentationPolicy, profile: str, stage: Path, unreal_pak: Path,
                evidence: Path, required_maps: tuple[str, ...] = (), timeout_seconds: int = 120) -> dict:
    """只读枚举全部Stage文件，每个发现的pak/utoc均重新取证；缺项、空证据和未知错误失败。

    全局IoStore元数据可以没有命名资产，但CSV仍须有真实非空Chunk；整体仍须有实际包及
    选定地图。审计目录必须新建且位于Stage之外，不能复用历史CSV冒充本次产物。
    本门禁验证资源禁入清单及必需容器存在，不验证全部UCAS/Pak载荷块完整性。
    """
    policy.cook_paths(profile)
    if not stage.is_dir():
        raise PolicyError(f"Stage目录不存在: {stage}")
    stage = stage.resolve()
    if evidence.resolve().is_relative_to(stage):
        raise PolicyError("审计输出必须位于Stage之外")
    files = sorted(path for path in stage.rglob("*") if path.is_file())
    if not files:
        raise PolicyError("Stage目录为空")
    executables = [path for path in files if path.name == "DivineBeastsArenaServer.exe" and path.parent.name == "Win64"]
    if len(executables) != 1 or executables[0].stat().st_size == 0:
        raise PolicyError("Stage必须有唯一非空Win64服务器程序")
    if not unreal_pak.is_file() or unreal_pak.stat().st_size == 0:
        raise PolicyError(f"缺少锁定UnrealPak: {unreal_pak}")
    evidence.mkdir(parents=True, exist_ok=False)
    report = {"Status": "Failed", "Profile": profile, "Stage": str(stage),
              "EvidenceKind": "ActualStageAndFreshUnrealPakListings", "Containers": [], "RuntimeVerified": False,
              "ValidationScope": "ServerPresentationExclusionsAndRequiredContainerInventory",
              "PayloadIntegrityVerified": False}
    try:
        entries = []
        snapshot = []
        for path in files:
            is_reparse = os.name == "nt" and bool(path.stat().st_file_attributes & 0x400)
            if path.is_symlink() or is_reparse:
                raise PolicyError(f"拒绝Stage中的链接/重解析文件: {path}")
            size = path.stat().st_size
            relative = path.relative_to(stage).as_posix()
            snapshot.append({"Path": relative, "Size": size, "MtimeNs": path.stat().st_mtime_ns})
            entries.append(ArtifactEntry(relative, size, "LooseFile", "WindowsServer"))
        (evidence / "StageFiles.json").write_text(json.dumps(snapshot, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        containers = [path for path in files if path.suffix.casefold() in (".utoc", ".pak")]
        report["DiscoveredContainers"] = len(containers)
        for index, container in enumerate(containers):
            if container.stat().st_size == 0:
                raise PolicyError(f"空容器: {container}")
            output = evidence / f"{index:03d}-{container.name}"
            if container.suffix.casefold() == ".utoc":
                data_file = container.with_suffix(".ucas")
                if not data_file.is_file() or data_file.stat().st_size == 0:
                    raise PolicyError(f"IoStore缺少非空UCAS: {container}")
                csv_path = output / "Container.csv"
                facts = _inspect_container([str(unreal_pak), f"-ListContainer={container}", f"-Csv={csv_path}"], output, timeout_seconds)
                container_entries = read_iostore_csv(csv_path)
            else:
                facts = _inspect_container([str(unreal_pak), str(container), "-List"], output, timeout_seconds)
                container_entries = read_pak_listing(output / "stdout.log")
            entries.extend(container_entries)
            report["Containers"].append({"Path": str(container), "Entries": len(container_entries), **facts})
        # 目录快照和全部容器取证必须属于同一静止Stage；发现之后增加/删除/改写均拒绝发布。
        current = sorted(path for path in stage.rglob("*") if path.is_file())
        current_snapshot = [{"Path": path.relative_to(stage).as_posix(), "Size": path.stat().st_size,
                             "MtimeNs": path.stat().st_mtime_ns} for path in current]
        if current_snapshot != snapshot:
            raise PolicyError("审计过程中Stage发生变化，证据不可用于本次验收")
        report.update(policy.validate_entries(entries, profile, required_maps))
        report.update(Status="Passed", DiscoveredContainers=len(containers), InspectedContainers=len(report["Containers"]))
        return report
    except BaseException as exc:
        report["Failure"] = str(exc) or type(exc).__name__
        report["Status"] = "Cancelled" if isinstance(exc, KeyboardInterrupt) else "Failed"
        raise
    finally:
        report["InspectedContainers"] = len(report["Containers"])
        (evidence / "ServerPresentationAudit.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def generate_configs(policy: ServerPresentationPolicy, workspace: Path) -> list[str]:
    """显式生成仅改受管段；全量预检并复核原字节，每文件原子替换，失败可重新check定位。

    不声称跨文件事务；若系统在替换之间终止，后续Cook的漂移检查仍会阻断。
    """
    plans = policy.planned_configs(workspace)
    for relative, (before, _) in plans.items():
        if (workspace / relative).read_bytes() != before:
            raise PolicyError(f"生成前配置已被其他任务修改: {relative}")
    changed = []
    for relative, (before, after) in plans.items():
        if before == after:
            continue
        target = workspace / relative
        if target.read_bytes() != before:
            raise PolicyError(f"生成时配置发生并发修改: {relative}")
        descriptor, temporary = tempfile.mkstemp(prefix=target.name + ".", suffix=".tmp", dir=target.parent)
        try:
            with os.fdopen(descriptor, "wb") as handle:
                handle.write(after)
            os.replace(temporary, target)
        finally:
            if os.path.exists(temporary):
                os.unlink(temporary)
        changed.append(relative)
    return changed


def main() -> int:
    """命令行边界：check默认只读；generate显式改段；audit-stage仅验证真实产物，取消返回130。"""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--policy", type=Path, default=DEFAULT_POLICY, help="单源JSON，不接受目录通配规则")
    parser.add_argument("--workspace", type=Path, default=WORKSPACE, help="用于校验资源归属与受管INI的工作空间")
    commands = parser.add_subparsers(dest="command", required=True)
    check = commands.add_parser("check", help="只读检查全部受管配置漂移")
    check.add_argument("--custom-config", choices=COOK_CONFIGS, default="DedicatedServer")
    commands.add_parser("generate", help="确定性生成已标记段，不迁移缺失标记")
    cooker = commands.add_parser("cooker-args", help="输出有限兼容profile的逐目录数组追加参数")
    cooker.add_argument("--custom-config", required=True, choices=COOK_CONFIGS)
    audit = commands.add_parser("audit-stage", help="枚举实际Stage并逐容器运行UnrealPak")
    audit.add_argument("--profile", choices=PROFILES, required=True)
    audit.add_argument("--stage", type=Path, required=True)
    audit.add_argument("--unreal-pak", type=Path, required=True)
    audit.add_argument("--evidence", type=Path, required=True)
    audit.add_argument("--required-map", action="append", default=[])
    audit.add_argument("--timeout-seconds", type=int, default=120)
    args = parser.parse_args()
    try:
        policy = load_policy(args.policy, args.workspace)
        if args.command == "check":
            report = policy.check_configs(args.workspace)
            profile = policy.resolve_profile(args.custom_config)
            report.update(Profile=profile, CustomConfig=args.custom_config, CookerArguments=policy.cooker_arguments(args.custom_config))
        elif args.command == "generate":
            report = {"ChangedFiles": generate_configs(policy, args.workspace)}
        elif args.command == "cooker-args":
            report = policy.cooker_arguments(args.custom_config)
        else:
            if not 1 <= args.timeout_seconds <= 3600:
                raise PolicyError("容器检查超时必须在1..3600秒")
            report = audit_stage(policy, args.profile, args.stage, args.unreal_pak, args.evidence,
                                 tuple(args.required_map), args.timeout_seconds)
        print(json.dumps(report, ensure_ascii=False))
        return 0
    except (PolicyError, OSError, UnicodeError) as exc:
        print(f"SERVER_PRESENTATION_POLICY_FAILED: {exc}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("SERVER_PRESENTATION_POLICY_CANCELLED", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
