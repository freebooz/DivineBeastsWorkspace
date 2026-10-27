"""通过Monolith调整12角色的实际GAS输入：1火球、2冰晶、3定向飞弹；保留5治疗和原槽位图标。
只修改既有AbilitySet.InputTag和UIProfile.Icon引用，稳定身份/数值/GA/图像像素均不变。
先audit记录真实失败，再apply备份并保存，cold audit由独立UE进程执行。
"""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
import shutil
import unreal

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "Saved/Validation/FireIceHotbar20261027"
HEROES = ("Rat", "Ox", "Tiger", "Rabbit", "Dragon", "Snake", "Horse", "Goat", "Monkey", "Rooster", "Dog", "Boar")
BASE = "/DBAGameplay/Abilities/Placeholders"
PREFIX = "Platform.Ability.Input.DivineBeasts."
NEW = {"fireball": "Primary", "frostbolt": "Slot1", "directedmissile": "Slot2"}
OLD = {"fireball": "Slot1", "frostbolt": "Slot2", "directedmissile": "Primary"}

def require(value, message):
    if not value:
        raise RuntimeError(message)

def write(name, data):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")

def identity(hero, mode):
    return "dba.ability." + hero.lower() + "_placeholder_" + mode + "@1"

def grant_id(grant):
    value = grant.get_editor_property("ability_id")
    return str(value.namespace) + "." + str(value.name) + "@" + str(value.logical_version)

def package_file(asset):
    mount, tail = asset.get_path_name().split(".")[0][1:].split("/", 1)
    descriptors = list((ROOT / "Game/Plugins").rglob(mount + ".uplugin"))
    require(len(descriptors) == 1, "挂载所有者不唯一 " + mount)
    return descriptors[0].parent / "Content" / (tail + ".uasset")

def load_all():
    require(Path(unreal.Paths.get_project_file_path()).resolve() == ROOT / "Game/DivineBeastsArena.uproject", "错误工程")
    result = []
    for hero in HEROES:
        aset = unreal.load_asset(BASE + "/DA_DBA_" + hero + "_PlaceholderAbilitySet")
        profile = unreal.load_asset("/DBAHeroPack_" + hero + "/UI/Abilities/DA_DBA_" + hero + "_PlaceholderUIProfile")
        require(aset and profile, "缺少角色集合或Profile " + hero)
        grants = list(aset.get_editor_property("abilities"))
        entries = list(profile.get_editor_property("entries"))
        require(len(grants) == 9, "角色集合数量已变化 " + hero)
        result.append((hero, aset, profile, grants, entries))
    return result

def summarize(rows):
    result = []
    for hero, aset, profile, grants, entries in rows:
        active = [g for g in grants if str(g.get_editor_property("input_tag").get_editor_property("tag_name")).startswith(PREFIX)]
        tags = [str(g.get_editor_property("input_tag").get_editor_property("tag_name")) for g in active]
        require(len(tags) == 8 and len(set(tags)) == 8, "主动输入不唯一 " + hero)
        selected = {}
        for mode in (*NEW, "primary"):
            expected_id = identity(hero, mode)
            matches = [g for g in grants if grant_id(g) == expected_id]
            require(len(matches) == 1, "技能身份不唯一 " + expected_id)
            g = matches[0]
            cls = g.get_editor_property("ability_class")
            cdo = unreal.get_default_object(cls)
            ui = [e for e in entries if str(e.get_editor_property("ability_id")) == expected_id]
            require(len(ui) == 1, "UI身份不唯一 " + expected_id)
            icon = ui[0].get_editor_property("icon")
            require(icon and icon.get_path_name().startswith("/DBAHeroPack_" + hero + "/"), "角色图标所有权错误 " + expected_id)
            selected[mode] = {
                "input": str(g.get_editor_property("input_tag").get_editor_property("tag_name")).split(".")[-1],
                "class": cls.get_path_name(), "mode": str(cdo.get_editor_property("placeholder_mode")),
                "style": str(cdo.get_editor_property("presentation_style_override")), "icon": icon.get_path_name()
            }
        require(selected["primary"]["input"] == "Slot3" and "HEALING" in selected["primary"]["mode"], "治疗5号槽合同异常")
        result.append({"hero": hero, "slots": selected, "grants": [g.export_text() for g in grants],
                       "entries": [e.export_text() for e in entries]})
    return result

def audit(filename="Audit.json"):
    rows = load_all()
    snapshot = summarize(rows)
    errors = []
    for row in snapshot:
        for mode, slot in NEW.items():
            actual = row["slots"][mode]
            if actual["input"] != slot:
                errors.append(row["hero"] + ":" + mode + " is " + actual["input"] + ", expected " + slot)
        require("FIREBALL" in row["slots"]["fireball"]["mode"] and row["slots"]["fireball"]["style"] == "None", "火球执行类型异常")
        require("FROSTBOLT" in row["slots"]["frostbolt"]["mode"] and row["slots"]["frostbolt"]["style"] == "None", "冰晶执行类型异常")
        require(row["slots"]["directedmissile"]["style"] == "DBA.Development.DirectedMissile", "定向飞弹表现异常")
    report = {"Passed": not errors, "HeroCount": len(rows), "Errors": errors, "Heroes": snapshot,
              "Project": unreal.Paths.get_project_file_path(), "Utc": datetime.now(timezone.utc).isoformat()}
    write(filename, report)
    print("FIRE_ICE_HOTBAR_AUDIT " + json.dumps({"passed": report["Passed"], "heroes": len(rows), "errors": len(errors), "report": str(OUT / filename)}))
    return report

def apply():
    require(not unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages(), "存在未保存内容，保留现场")
    rows = load_all()
    before = summarize(rows)
    already = all(all(r["slots"][m]["input"] == s for m, s in NEW.items()) for r in before)
    if already:
        return audit("Green.json")
    require(all(all(r["slots"][m]["input"] == s for m, s in OLD.items()) for r in before), "存在非预期自定义映射，拒绝覆盖")
    write("Before.json", before)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    backup = OUT / "Backup" / stamp
    for _, aset, profile, _, _ in rows:
        for obj in (aset, profile):
            src = package_file(obj)
            target = backup / src.relative_to(ROOT)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target)
    for hero, aset, profile, grants, entries in rows:
        # 原角色图标按按钮位置保留，仅调整其UI条目引用，不编辑纹理/作者预览顺序。
        icons_by_slot = {}
        for g in grants:
            ability_id = grant_id(g)
            if ability_id not in {identity(hero, m) for m in NEW}:
                continue
            slot = str(g.get_editor_property("input_tag").get_editor_property("tag_name")).split(".")[-1]
            entry = next(e for e in entries if str(e.get_editor_property("ability_id")) == ability_id)
            icons_by_slot[slot] = entry.get_editor_property("icon")
        for i, grant in enumerate(grants):
            mode = next((m for m in NEW if grant_id(grant) == identity(hero, m)), None)
            if not mode:
                continue
            old_slot = str(grant.get_editor_property("input_tag").get_editor_property("tag_name"))
            replacement = unreal.GamePlatformAbilityGrant()
            require(replacement.import_text(grant.export_text().replace(old_slot, PREFIX + NEW[mode])), "输入授权结构导入失败")
            grants[i] = replacement
            for j, entry in enumerate(entries):
                if str(entry.get_editor_property("ability_id")) == identity(hero, mode):
                    entry.set_editor_property("icon", icons_by_slot[NEW[mode]])
                    entries[j] = entry
        aset.set_editor_property("abilities", grants)
        profile.set_editor_property("entries", entries)
        require(unreal.EditorAssetLibrary.save_loaded_asset(profile, only_if_is_dirty=False), "Profile保存失败")
        require(unreal.EditorAssetLibrary.save_loaded_asset(aset, only_if_is_dirty=False), "AbilitySet保存失败")
    after = audit("Green.json")
    require(after["Passed"], "新映射检查失败，备份已保留")
    for b, a in zip(before, after["Heroes"]):
        # 每个槽的图标与修改前完全相同；输入之外的授权信息仅做最小修改。
        old_icons = {v["input"]: v["icon"] for v in b["slots"].values()}
        new_icons = {v["input"]: v["icon"] for v in a["slots"].values()}
        require(old_icons == new_icons, "原角色图标槽位改变")
        for old_text, new_text in zip(b["grants"], a["grants"]):
            normalized = old_text
            for m in NEW:
                if 'Name="' + b["hero"].lower() + '_placeholder_' + m + '"' in old_text:
                    normalized = old_text.replace(PREFIX + OLD[m], PREFIX + NEW[m])
            require(normalized == new_text, "授权字段出现额外变化")
    files = [package_file(obj) for _, aset, profile, _, _ in rows for obj in (aset, profile)]
    write("SavedAssets.json", {"Passed": True, "Backup": str(backup), "Files": [
        {"Path": str(p.relative_to(ROOT)), "SHA256": hashlib.sha256(p.read_bytes()).hexdigest()} for p in files]})
    print("FIRE_ICE_HOTBAR_SAVED=24")
    return after

