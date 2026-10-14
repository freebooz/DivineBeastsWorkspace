# -*- coding: utf-8 -*-
"""经Monolith在正式UE编辑器中设置十二生肖英雄的初始气势为50。

归属：项目资源制作工具；真实定义由DBAGameplay拥有，运行时资源由服务器GAS初始化和复制。
本工具先检查全部英雄身份与上限，再只修改Momentum.InitialMomentum并精确保存各包。
调用方须先备份这些实际uasset；失败立即停止发布，不修改地图、技能成本、角色档案或气势上限。
不会直接写二进制文件；保存仅调用UE资产接口。保存成功仍须新进程读取及真实出生回归验证。
"""
import math
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake",
          "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")


def inspect_hero_momentum():
    """只读核对正式工程、12份英雄定义及气势合法范围，返回人工审核快照；不保存或改资源。"""
    import unreal
    if Path(unreal.Paths.get_project_file_path()).resolve() != WORKSPACE / "Game/DivineBeastsArena.uproject":
        raise RuntimeError("当前编辑器不是SSLM正式工程，拒绝资源操作")
    records = []
    for hero in HEROES:
        path = "/DBAGameplay/Definitions/DA_Hero_Zodiac_" + hero
        asset = unreal.load_asset(path)
        if not isinstance(asset, unreal.DivineBeastsHeroDefinition):
            raise RuntimeError("真实英雄定义缺失或类型不符：" + path)
        identity = str(asset.get_editor_property("DefinitionId"))
        if identity != "Hero.Zodiac." + hero:
            raise RuntimeError("英雄稳定身份与资产不符：" + path)
        momentum = asset.get_editor_property("Momentum")
        initial = float(momentum.get_editor_property("InitialMomentum"))
        maximum = float(momentum.get_editor_property("MaxMomentum"))
        if not math.isfinite(initial) or not math.isfinite(maximum) or maximum < 50.0:
            raise RuntimeError("气势定义无效或上限不足50，拒绝暗改上限：" + path)
        records.append({"AssetPath": path, "HeroId": identity,
                        "InitialMomentum": initial, "MaxMomentum": maximum})
    return records


def apply_hero_initial_momentum():
    """全部前置检查成功后将初始值设为50；逐包保存非事务，失败须用调用前整组备份回退。"""
    import unreal
    before = inspect_hero_momentum()
    for record in before:
        asset = unreal.load_asset(record["AssetPath"])
        asset.modify()
        momentum = asset.get_editor_property("Momentum")
        momentum.set_editor_property("InitialMomentum", 50.0)
        asset.set_editor_property("Momentum", momentum)
        if not unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False):
            raise RuntimeError("英雄定义保存失败，请保留证据并使用本轮备份回退：" + record["AssetPath"])
    after = inspect_hero_momentum()
    if any(record["InitialMomentum"] != 50.0 for record in after):
        raise RuntimeError("保存后编辑器状态不符合初始50，禁止继续Cook")
    return {"Before": before, "After": after, "Saved": True,
            "FreshProcessVerified": False, "RuntimeSource": "HeroDefinition -> Server GAS"}
