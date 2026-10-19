#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM苍龙V6八枚AI图像映射只读静态门禁；不调用UE、不生成.uasset。"""
import importlib.util
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/"Tools/Unreal/Abilities/ImportDragonV6ChosenIconsUE58.py"
PROPOSAL=ROOT/"Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.json"
RECEIPT=ROOT/"Saved/ArtReview/DragonV6Selected8NativeIcons_20261018/UEImportedTextures.json"


def main():
    spec=importlib.util.spec_from_file_location("dragon_v6_icon_importer",SCRIPT)
    assert spec and spec.loader
    module=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=module
    spec.loader.exec_module(module)
    assert module.VARIANTS==("A","A","C","A","A","A","A","C")
    assert len(module.ABILITIES)==len(module.NAMES)==len(module.SLOTS)==8
    assert len(set(module.ABILITIES))==8
    assert len(set(module.RUNTIME_HASHES))==len(set(module.SOURCE_HASHES))==8
    assert all(len(x)==64 for x in (*module.RUNTIME_HASHES,*module.SOURCE_HASHES))
    original=json.loads(PROPOSAL.read_text(encoding="utf-8-sig"))
    assert original["V5NamesStillProductionAuthority"] is True
    dragon=next(x for x in original["heroes"] if x["heroId"]=="Hero.Zodiac.Dragon")
    assert [v["abilityId"] for v in dragon["skills"]]==list(module.ABILITIES)
    assert [v["slotId"] for v in dragon["skills"]]==list(module.SLOTS)
    assert [v["approvedDisplayNameV5"] for v in dragon["skills"]]==list(module.V5_LABELS)
    assert [v["proposedNameV61"] for v in dragon["skills"]]==list(module.LABELS)
    assert len(module.entries())==8
    missing=sum(not x["master"].exists() or not x["source"].exists()
                for x in module.entries())
    if RECEIPT.exists():
        d=json.loads(RECEIPT.read_text(encoding="utf8"))
        assert d["importedCount"]==8 and len(d["items"])==8 and d["profileBound"] is False
        print("SSLM_DRAGON_V6_UE_ASSET_COUNT_RECORDED coldVerified="+str(d["coldVerified"]))
    else:
        print("SSLM_DRAGON_V6_IMPORT_PENDING stillMissingSelectedImagePairs="+str(missing))
    print("SSLM_DRAGON_V6_SELECTION_EIGHT_ABILITY_IDS_V5_NAMES_STATIC_PASS")


if __name__=="__main__":
    main()
