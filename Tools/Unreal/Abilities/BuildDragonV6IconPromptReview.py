#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 辰龙V6 八技能 24项独立AI绘图提示词/审核冻结清单生成器。

只读取V6.1历史提案以及实有24张程序化构图概念图；
输出实际可投喂图像模型的完整24份独立原稿提示词与人工三选一清单。
未获人审时不能把候选误认成电影级母版、不能改V5现行UIProfile或GAS能力。
不需要安装Pillow；使用Python3标准库与PNG/SHA格式校验。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
SOURCE=ROOT/"Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.json"
COLORS=ROOT/"Docs/Art/SSLMColorRegistry_V3.json"
REVIEW=ROOT/("Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dragon/"
             "SourceArt/UI/SkillIcons/V6/Review")
CONTACT=REVIEW/"DragonV6CandidateReviewManifest.json"
DEST=REVIEW/"DragonV6_24IndependentAIArtPrompts.json"
SELECTION=REVIEW/"DragonV6_HumanReviewSelections_Draft.json"

VARIANTS=(
  (
    "A：三道巨型苍青龙爪由左上斜落至右下，龙鳞指节可见，三道蓝白裂光呈不同粗细，主体占画面约75%，第一眼识别斜爪。",
    "B：正面透视一只五指神龙巨爪从浓云中冲向镜头，爪尖透亮玉银，画面有明显冲刺压迫力，背后不画完整龙身。",
    "C：龙爪自右上撕开交叉裂隙，锋刃前端带青金色微亮龙鳞，强调X形交叉破空而非雷电。"
  ),
  (
    "A：一道笔直青白色巨大雷柱自正上方向下贯入直径很小的玉青预警环，地面只有少量冲击碎石，落雷主体高于全幅75%。",
    "B：斜向下贯穿云层的雷枪，单一粗亮主电束与最多两条细副电丝，落点白青灼光明确，不画龙爪。",
    "C：天光中双段劈落雷裁，双闪电线由云端合并于唯一落点，警戒圈亮青，白蓝主光透亮，明确单点技能。"
  ),
  (
    "A：东方苍龙侧脸张口喷吐扇形雷息，龙首位于左上三分区，前方蓝白雷雾锥形向右爆发，扇区界限清楚。",
    "B：放大的苍龙鼻吻和颚骨居画面左侧，嘴内凝聚明亮玉青雷珠，雷霆龙息如扇叶向右下喷射。",
    "C：巨龙盘云仅露出尖锐龙首，向斜下方喷出三束汇聚成一个锥形的青白云雷，云雾浓而不糊主体。"
  ),
  (
    "A：东方苍龙盘绕成近乎完整的大圆，风云雷三层交叠，中心是唯一的金青色风暴眼与一道雷裁，呈史诗终结仪式。",
    "B：苍龙自上方云层俯冲降临三段式雷霆领域，前景圆形冲击波，中景云幕，背景风压，主龙头最亮。",
    "C：空中三层清晰环状法阵围绕巨大神龙头，外环风、中环云、内核青白雷柱，表达‘定天’，避免堆砌文字图腾。"
  ),
  (
    "A：东方苍龙化成明亮青风残影从左下斜冲右上，三条长云尾拉出方向箭头，末端龙首完整清晰。",
    "B：一股横向青玉色高速风流由左向右吹开云层，龙影藏在风流前端，主体表现机动位移和加速而非攻击。",
    "C：前景亮青小龙首奔向上方出口，身后两道错位残影和卷云轨迹，方向感极强，低密度闪电只作辅光。"
  ),
  (
    "A：六角玉青云盾包裹正面苍龙龙首，盾外缘银白透亮，内部有三枚大面积龙鳞，而不是攻击尖刺。",
    "B：半透明金青圆弧由一条盘龙形成坚固护罩，中心留出一块平滑云盾，青蓝白层次明显、守护感强。",
    "C：苍龙双角从云层上方弯成罩形，云白屏障正面挡住三支极简化远程光箭，突出保护效果，画面柔和。"
  ),
  (
    "A：两道大小不同的同心风压环，外圈青蓝向外震荡，内圈银白向内牵引，中心一点龙气，轮廓一眼能看成控制区域。",
    "B：俯视角漩涡中两条反向旋转的云青能量轨道，外缘强烈冲击线，中心压缩的苍龙目光，明显‘推拉’。",
    "C：苍龙龙角在风压环后方出现剪影，两个半径清晰的蓝白圆环交汇制造失衡震荡，避免与‘龙域’雷云穹顶混淆。"
  ),
  (
    "A：圆形青白雷云穹顶覆盖一片微缩土地，苍龙虚影沿穹顶盘旋，领域边界清楚，中心明暗分明。",
    "B：局部风云雷场域上方显现苍龙首，下面有三个不同高度的风云与雷击能量柱，巨幅深蓝云幕。",
    "C：一条东方苍龙盘成巨大深青雷云法环，法环内部有简洁的有限雷霆格区和厚云，不绘制旧五行和王印。"
  )
)
ART_NAMES=("CangLongZhua","LeiCai","LeiXi","CangLongDingTian",
           "YuFeng","YunHu","LongYa","LongYu")
BAD=(
 "无文字、字母、数字、标记、UI画框、热键提示、水印、人物照片、世界地图截图、"
 "插画海报、图表、九宫格拼图、同一图多技能、AI胶片颗粒、材质噪点、"
 "过度锐化、过曝白边、霓虹光污染、像素画、仿制王者荣耀真实图标、"
 "血腥、恐怖、五行克制、旧王印、跨英雄旧关系玩法。"
)

def load()->tuple[dict,dict,dict]:
    a=json.loads(SOURCE.read_text(encoding="utf-8-sig"))
    b=json.loads(COLORS.read_text(encoding="utf-8-sig"))
    c=json.loads(CONTACT.read_text(encoding="utf-8-sig"))
    if a.get("V5NamesStillProductionAuthority") is not True:
        raise RuntimeError("项目当前V5正式显示名权威已发生变化")
    hero=next((x for x in a["heroes"] if x.get("heroId")=="Hero.Zodiac.Dragon"),None)
    if not hero or len(hero["skills"])!=8 or len(c["candidates"])!=24:
        raise RuntimeError("真实V6龙8槽/24候选概念仍不完整")
    return hero,b,c

def build()->tuple[dict,dict]:
    hero,palette,review=load()
    colors={x["semanticId"]:x["referenceSRGBHex"] for x in palette["items"]}
    essentials=("UI.Brand.InkCyan","UI.Brand.DeepJade","UI.Brand.Jade",
                "UI.Brand.Gold","UI.Brand.JadeWhite","VFX.ElementalVisual.Water",
                "VFX.ElementalVisual.Metal")
    if any(k not in colors for k in essentials):
        raise RuntimeError("全局V3真实配色语义缺失")
    scheme={k:colors[k] for k in essentials}
    rows=[]
    skills=hero["skills"]
    for i,skill in enumerate(skills):
        if len(VARIANTS[i])!=3:
            raise RuntimeError("龙技能候选并非每技能A/B/C")
        for j,variant in enumerate("ABC"):
            match=next((x for x in review["candidates"] if x["skillOrder"]==i+1 and
                        x["variant"]==variant and x["abilityId"]==skill["abilityId"]),None)
            if not match:
                raise RuntimeError(f"缺少候选{i+1}{variant}原生审查来源")
            prompt=(
                "为原创东方神话多人竞技游戏《神兽联盟》创作**单一独立的方形技能图标**，"
                "英雄辰龙·御霄苍龙，技能设计V6，技能名称“"+skill["proposedNameV61"]+"”。"
                "图标正方形1024×1024像素，独立输出一图，**不是整栏、九宫格、信息图或海报**。"
                "3A游戏专业技能图标质感，电影级3D手绘结合，东方苍龙鳞片材质细腻、"
                "云雾体积自然、青白雷光集中、金玉细节精致，表面干净无颗粒；"
                "明确在48×48像素仍能一眼辨认的中心主体，主体约占画幅75%，"
                "严格前中后最多三层，轮廓锐利但不过度锐化。"
                f"统一V3色系：玄墨青{scheme['UI.Brand.InkCyan']}，"
                f"深碧玉{scheme['UI.Brand.DeepJade']}，"
                f"灵玉青{scheme['UI.Brand.Jade']}，"
                f"鎏金{scheme['UI.Brand.Gold']}，"
                f"玉脂白{scheme['UI.Brand.JadeWhite']}，"
                f"寒渊蓝{scheme['VFX.ElementalVisual.Water']}，"
                f"银青{scheme['VFX.ElementalVisual.Metal']}。"
                "请采用以下具体构图，不能与同英雄其他七个图标雷同："
                +VARIANTS[i][j]+"。背景为深青色高质感低干扰魔法气氛，"
                "图标本身不得带技能栏金属框、文字、冷却扇形遮罩或快捷键。"
                "负面约束："+BAD
            )
            rows.append({
                "index":len(rows),
                "heroId":"Hero.Zodiac.Dragon",
                "skillOrder":i+1,
                "slotId":skill["slotId"],
                "abilityId":skill["abilityId"],
                "v6ProposedSkillNameZh":skill["proposedNameV61"],
                "approvedV5ProductionDisplayNameZh":skill["approvedDisplayNameV5"],
                "renameNeedsApproval":bool(skill["renameNeedsApproval"]),
                "variant":variant,
                "sourceConceptPath":match["master"]["path"],
                "outputFilename":f"T_DBA_Dragon_S{i+1:02}_{ART_NAMES[i]}_V6_{variant}_Master_1024.png",
                "promptZh":prompt,
                "negativePromptZh":BAD,
                "reviewState":"PENDING_HUMAN_SELECTION",
                "formalArtGenerated":False,
                "ueImported":False,
            })
    return {
        "schemaVersion":1,
        "heroId":"Hero.Zodiac.Dragon",
        "source":"Docs/Design/ZodiacSkillsV6_1_DetailedProposal_20261018.json",
        "palette":"Docs/Art/SSLMColorRegistry_V3.json",
        "status":"PROMPTS_READY_3D_ART_NOT_GENERATED",
        "productionV5Authority":True,
        "candidateCount":len(rows),
        "promptEnginePolicyZh":"每个prompt分别生成独立1024/2048母版，不能拼贴后切图；每技能A/B/C各一张。",
        "colorPalette":scheme,
        "art":rows
    }, {
        "schemaVersion":1,
        "heroId":"Hero.Zodiac.Dragon",
        "candidateSource":"DragonV6CandidateReviewManifest.json",
        "artPromptSource":"DragonV6_24IndependentAIArtPrompts.json",
        "explicitUserHumanSelection":False,
        "productionUIProfileTouched":False,
        "productionAbilitySetTouched":False,
        "formalImportedTextures":0,
        "selection":[
            {
                "skillOrder":i+1,
                "v6ProposedSkillNameZh":s["proposedNameV61"],
                "abilityId":s["abilityId"],
                "choiceABC":None,
                "approval":"PENDING",
                "artMasterPNG":None,
                "quality48Verified":False,
                "quality64Verified":False
            } for i,s in enumerate(hero["skills"])
        ]
    }

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--verify-only",action="store_true")
    args=parser.parse_args()
    prompts,selections=build()
    if args.verify_only:
        for path,expected in ((DEST,prompts),(SELECTION,selections)):
            if not path.is_file() or json.loads(path.read_text(encoding="utf-8-sig"))!=expected:
                raise RuntimeError("已冻结24份技能提示词或8份人工审核草案身份漂移："+str(path))
        print("SSLM_DRAGON_V6_24_PROMPTS_AND_8_EMPTY_HUMAN_APPROVALS_VERIFIED")
        return
    if DEST.exists() or SELECTION.exists():
        raise FileExistsError("已有人工审核流程，禁止覆盖历史选型记录")
    DEST.write_text(json.dumps(prompts,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    SELECTION.write_text(json.dumps(selections,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print("SSLM_DRAGON_V6_24_INDEPENDENT_AI_ART_PROMPTS_FROZEN",len(prompts["art"]),
          "human_selected=0 formal_ai_art=0 UE_import=0")

if __name__=="__main__":
    main()
