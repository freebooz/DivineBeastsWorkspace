# SSLM通用水体静态资产合同：只核对真实的*.uasset文件与三目标隔离配置。
# 此脚本不创建/修改UE二进制、不加载地图，也不将文件存在当作资产Shader或Cook通过。
# 使用：python Tests/Architecture/ValidateGamePlatformWaterAssets.py --require-review-map（严格验收时）
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def verify(root: Path, require_review_map: bool) -> tuple[list[str], list[str]]:
    """验证本工作区现有水资源及专服依赖边界；返回已确认/失败信息，不触碰资产。"""
    manifest_path = root / "Docs/Implementation/Water/WaterAssetRegistry_V1.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
    mounts = {
        name: root / relative for name, relative in manifest["mounts"].items()
    }
    good: list[str] = []
    bad: list[str] = []

    def asset_exists(uri: str, *, is_level: bool = False) -> bool:
        """按资产类别选择.umap或.uasset，不能依赖路径是否包含Maps目录。"""
        parts = uri.strip("/").split("/", 1)
        if len(parts) != 2 or parts[0] not in mounts:
            bad.append(f"不可识别的内容包挂载点：{uri}")
            return False
        suffix = ".umap" if is_level else ".uasset"
        target = mounts[parts[0]] / (parts[1] + suffix)
        return target.is_file() and target.stat().st_size > 1024

    for category, paths in manifest["requiredAssets"].items():
        for uri in paths:
            if asset_exists(uri):
                good.append(f"{category}: {uri}")
            else:
                bad.append(f"缺少原生资产或资产过小：{category} {uri}")
    # 水体PNG源图并非*.uasset，但需要核对通道清单、真实PNG尺寸与哈希；
    # 任何资源被替换后未更新证据应提示，不能将源图存在误报为正式Texture2D。
    source_manifest = manifest.get("sourceTextureManifest")
    if source_manifest:
        texture_dir = (root / source_manifest).parent
        metadata = json.loads((root / source_manifest).read_text(encoding="utf-8"))
        if len(metadata.get("items", [])) != 4:
            bad.append("通用水体源纹理必须为4个已登记类别")
        for item in metadata.get("items", []):
            filename = item.get("file", "")
            if not filename or Path(filename).name != filename or not filename.endswith(".png"):
                bad.append(f"水体源纹理文件名非法：{filename}")
                continue
            path = texture_dir / filename
            if not path.is_file():
                bad.append(f"水体源纹理PNG缺失：{filename}")
                continue
            image = path.read_bytes()
            # PNG IHDR尺寸位于签名后16..23字节，利用标准库避免引入构建期Pillow依赖。
            if len(image) < 33 or image[:8] != bytes.fromhex("89504e470d0a1a0a"):
                bad.append(f"源纹理PNG头部不合法：{filename}")
                continue
            width = int.from_bytes(image[16:20], "big")
            height = int.from_bytes(image[20:24], "big")
            if width != item.get("width") or height != item.get("height"):
                bad.append(f"源纹理尺寸与台账不一致：{filename}")
            digest = hashlib.sha256(image).hexdigest()
            if digest != item.get("sha256") or item.get("sRGB") is not False:
                bad.append(f"水体源纹理哈希或色彩空间配置不符：{filename}")
            else:
                good.append(f"源PNG（待导入）: {filename}")
    if require_review_map:
        for uri in manifest["pendingAssets"]["水体独立审核地图"]:
            if not asset_exists(uri, is_level=True):
                bad.append(f"P4审核地图文件缺失或过小：{uri}；地图存在性不等于视觉与性能验收通过")

    project_file = root / "Game/DivineBeastsArena.uproject"
    project = json.loads(project_file.read_text(encoding="utf-8-sig"))
    enabled = {entry["Name"] for entry in project.get("Plugins", [])
               if entry.get("Enabled", False)}
    if "GamePlatformWater" not in enabled:
        bad.append("主工程未启用通用水体插件GamePlatformWater")
    if "FluidFlux" in enabled or any("FluidFlux" in name for name in enabled):
        bad.append("正式工程不得硬启用FluidFlux，它只允许角色预览独立可选适配")

    plugin_path = root / "Game/Plugins/GamePlatform/World/GamePlatformWater/GamePlatformWater.uplugin"
    plugin = json.loads(plugin_path.read_text(encoding="utf-8-sig"))
    modules = {mod["Name"]: mod for mod in plugin.get("Modules", [])}
    names = {"GamePlatformWaterRuntime", "GamePlatformWaterClient", "GamePlatformWaterEditor"}
    if not names.issubset(modules):
        bad.append("通用水体插件三模块缺失")
    else:
        client = modules["GamePlatformWaterClient"]
        if client.get("Type") != "ClientOnly" or "Server" in client.get("TargetAllowList", []):
            bad.append("客户端水体模块不得进入Dedicated Server")
        if modules["GamePlatformWaterRuntime"].get("Type") != "Runtime":
            bad.append("权威水域模块必须双端共享")

    server_build = root / ("Game/Plugins/GamePlatform/World/GamePlatformWater/"
                           "Source/GamePlatformWaterRuntime/GamePlatformWaterRuntime.Build.cs")
    server_build_text = server_build.read_text(encoding="utf-8")
    if any(f'"{name}"' in server_build_text for name in ("Water", "Niagara", "FluidFlux")):
        bad.append("水域共享Runtime Build.cs直接引用纯视觉模块")
    return good, bad


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM水体资源与服务器隔离静态审核")
    parser.add_argument("--require-review-map", action="store_true",
                        help="P4真实地图已经制作后启用严格模式；未生成地图必须失败")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    good, bad = verify(root, args.require_review_map)
    print(f"水体原生资产及PNG源图确认={len(good)} 问题={len(bad)}")
    for detail in bad:
        print("ERROR:", detail)
    if not bad:
        print("水体原生资产存在性与静态模块目标隔离检查通过，尚不证明UE编译、Shader或Cook")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
