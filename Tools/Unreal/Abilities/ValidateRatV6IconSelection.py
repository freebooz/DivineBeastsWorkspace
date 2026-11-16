#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 子鼠 V6 美术候选/人工优选的失败关闭检查器。

阶段A（--check-plan）只核对8技能的稳定AbilityId/V5名称与槽位，不读UE资产。
阶段B（--selection JSON）必须有人逐项选择且24候选包已正确导入 SourceArt，
逐一核对该候选48px/64px/1024px PNG头和SHA-256，不生成UAsset、不改UIProfile。
本脚本不是原生UE Texture2D导入器；UI创建与纹理导入由Monolith MCP执行。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FROZEN = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/RatV6_VisualArtScopeFreeze_20261018.json"
DRAFT = ROOT / "Docs/Implementation/GamePlatformAbilitySystem/RatV6SkillIconImportManifest_Draft.json"
DEFAULT_ASSETS = ROOT / ("Game/Plugins/DivineBeasts/ContentPacks/Heroes/"
    "DBAHeroPack_Rat/SourceArt/UI/SkillIcons/V6/Candidates")


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8-sig"))


def png_info(path: Path) -> tuple[int, int, int]:
    with path.open("rb") as source:
        head = source.read(26)
    if len(head) < 26 or head[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"PNG文件头损坏或格式不正确：{path}")
    if head[12:16] != b"IHDR":
        raise ValueError(f"缺少PNG原生IHDR尺寸头：{path}")
    width, height = struct.unpack(">II", head[16:24])
    colortype = head[25]
    if colortype not in (2, 6):
        raise ValueError(f"图标必须RGB或RGBA（PNG颜色类型2/6）：{path}, type={colortype}")
    return width, height, colortype


def check_plan() -> dict:
    freeze, draft = load(FROZEN), load(DRAFT)
    assert freeze["state"] == "ART_SCOPE_FROZEN_GAMEPLAY_NOT_APPROVED"
    assert freeze["hero"] == draft["heroId"] == "Hero.Zodiac.Rat"
    assert not freeze["defaultAbilitySetModified"]
    assert not freeze["uiProfileModified"]
    assert not freeze["humanSelected"]
    assert not draft["productionApproval"]
    fields = freeze["abilities"]
    drows = draft["entries"]
    assert len(fields) == len(drows) == 8
    assert len({a["stableAbilityId"] for a in fields}) == 8
    assert len({a["imageStem"] for a in fields}) == 8
    assert [a["inputSlot"] for a in fields] == (
        ["Primary", "Slot1", "Slot2", "Slot4", "Slot3", "Slot5", "Slot6", "Slot7"])
    for a, d in zip(fields, drows):
        assert a["stableAbilityId"] == d["abilityId"]
        assert a["approvedV5NameZh"] == d["approvedNameV5Zh"]
        assert a["proposedV6NameZh"] == d["proposalNameV6Zh"]
        assert a["imageStem"] == d["assetName"]
        assert a["selectedCandidate"] is None
    return freeze


def under(root: Path, child: str) -> Path:
    base = root.resolve()
    target = (base / child).resolve()
    if target == base or base not in target.parents:
        raise ValueError(f"拒绝访问越过候选资产目录的路径：{child}")
    return target


def validate_choice(selection_file: Path, review_root: Path) -> dict:
    freeze = check_plan()
    if not selection_file.exists():
        raise ValueError("人工优选尚未提交，不能导入正式图标")
    choices = load(selection_file)
    assert choices.get("hero") == "Hero.Zodiac.Rat"
    assert choices.get("type") == "visual_art_selection_only"
    rows = choices.get("choices", [])
    if len(rows) != 8 or any(x.get("candidate") not in ("A", "B", "C") for x in rows):
        raise ValueError("必须有8个技能各自选择A/B/C之一，不能批量默认批准")
    if [x.get("slot") for x in rows] != list(range(1, 9)):
        raise ValueError("人工作业与八技能稳定显示顺序不一致")
    candidate_manifest_file = review_root / "RatV6_Candidate24_Manifest.json"
    if not candidate_manifest_file.is_file():
        raise ValueError("尚未把24图候选包放入鼠英雄SourceArt目录；拒绝创建无源图的UE资产")
    candidate_manifest = load(candidate_manifest_file)
    entries = candidate_manifest.get("entries", [])
    if len(entries) != 8:
        raise ValueError("完整候选清单必须包含8项技能")
    approved_choices = []
    hashes = set()
    for plan, human, bundle in zip(freeze["abilities"], rows, entries):
        if (plan["slotOrder"] != human["slot"] or plan["slotOrder"] != bundle["order"]
            or plan["stableAbilityId"] != bundle["abilityId"]
            or plan["proposedV6NameZh"] != human["skill"]):
            raise ValueError("候选图与技能身份或人工选择不一致，禁止按中文名字或数组下标盲绑")
        selected = next((x for x in bundle["variants"]
                         if x["variant"] == human["candidate"]), None)
        if selected is None:
            raise ValueError("找不到被选择的候选资产")
        icons = {
            "48": (selected["png48"], (48, 48)),
            "64": (selected["png64"], (64, 64)),
            "master": (selected["master"], (1024, 1024))
        }
        for label, (rel, dimensions) in icons.items():
            physical = under(review_root, rel)
            if not physical.is_file():
                raise ValueError(f"所选{label}图缺失：{rel}")
            if png_info(physical)[:2] != dimensions:
                raise ValueError(f"真实尺寸不符：{physical} 应为{dimensions}")
        icon = under(review_root, selected["png48"])
        actual_hash = hashlib.sha256(icon.read_bytes()).hexdigest()
        if actual_hash != selected["sha256_48"] or actual_hash in hashes:
            raise ValueError("候选48x48源图哈希不符或重复，拒绝导入")
        hashes.add(actual_hash)
        approved_choices.append({
            "slot": plan["slotOrder"], "inputSlot": plan["inputSlot"],
            "abilityId": plan["stableAbilityId"], "v5Name": plan["approvedV5NameZh"],
            "v6ProposalName": plan["proposedV6NameZh"],
            "choice": human["candidate"], "icon48Source": selected["png48"],
            "icon64Preview": selected["png64"], "master1024": selected["master"],
            "sourceSha256": actual_hash, "plannedAssetName": plan["imageStem"],
            "monolithTextureImported": False, "humanFinalApproved": False
        })
    return {"status": "CANDIDATES_SELECTED_ONLY_NOT_UE_IMPORTED",
            "hero": freeze["hero"], "allApprovedNamesV5Unchanged": True,
            "oldDefaultAbilitySetUnchanged": True,
            "records": approved_choices}


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM 鼠英雄V6图标冻结与人工选择检查")
    parser.add_argument("--check-plan", action="store_true", help="只读验证8项冻结身份")
    parser.add_argument("--selection", type=Path, help="人工从候选HTML页面导出的选择JSON")
    parser.add_argument("--review-root", type=Path, default=DEFAULT_ASSETS,
                        help="复制至鼠英雄SourceArt中的完整候选图目录")
    args = parser.parse_args()
    if args.selection:
        result = validate_choice(args.selection, args.review_root)
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        freeze = check_plan()
        print(f"RAT_V6_8_SKILL_ART_FREEZE_VALID: skillCount={len(freeze['abilities'])}; "
              "candidateChoices=3; humanSelected=0; productionAbilitiesUnchanged=1")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
