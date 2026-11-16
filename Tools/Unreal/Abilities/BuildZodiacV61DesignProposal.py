#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM 十二生肖 V6.1 技能设计提案生成器（不修改生产数据或UE资产）。

新提案逐项绑定96个稳定AbilityId；V5已批准显示名保持原样；47处V6拟议改名
需要另行人工审批。以已有V6历史技能为基础补足每项的服务器权威边界、反制、
真实VFX要求及回归条件。V2一级数值和初始前摇均为待平衡的候选，不是批准值。
"""
import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
DESIGN=ROOT/"Docs/Design"
IMPL=ROOT/"Docs/Implementation/GamePlatformAbilitySystem"
OLD_V6=DESIGN/"ZodiacHistoryFirstEightSkillsV6_Review.json"
OLD_V2=DESIGN/"ZodiacEightSlotSkills_V2.json"
V5=DESIGN/"ApprovedSkillDisplayNames_V5.json"
REG=IMPL/"V6SkillImplementationRegistry_20261018.json"
OUT_JSON=DESIGN/"ZodiacSkillsV6_1_DetailedProposal_20261018.json"
OUT_MD=DESIGN/"ZodiacSkillsV6_1_DetailedProposal_20261018.md"

# 各项补足：真实权威判定¦边界及退场¦重点自动测试。十二生肖逐项设计。
REVIEW={
"Rat":[
"首个合法近战命中才获取一次气势¦只命中一次，不能穿墙越过前方友军¦普攻连按/重复Combat事件/气势去重",
"背侧咬影追加有限毒血印记¦服务器按朝向确认侧背，印记不无限叠加¦目标转身、格挡、印记到期",
"交错牙刃左右两次攻击¦每段独立命中，上段结束不能攻击已换化身目标¦二段被闪避/提前死亡/取消",
"最多三段合法影袭，末段低血增益¦每段独立导航/碰撞，失败停止后段，不假退款¦闪现落点被封/第三段断线/伤害事实",
"只穿经过关卡审核的窄缝锚点¦闭合实体墙不能被短移技能跨越¦狭道、门关闭、导航缺失",
"短时扰乱新目标锁定并留下幻尾¦不能抹去服务器已在途弹道或已命中的控制¦反隐、在途投射、持续时间",
"咬筋脚踝减速并条件抑制下次位移¦受韧性约束，不是无条件持续定身¦净化、重复控制和位移冲突",
"只广播可见低血或落单目标模糊方向¦有限距离、两条提示上限、不透视潜行坐标¦掩体、隐身、进入群体、跨世界"
],
"Ox":[
"玄角近距离一次实体撞击¦首个有效敌方目标、普攻气势去重¦阻挡与背向",
"裂角附有界破甲，贴墙才增加震荡¦破甲GAS层数明确，墙边由服务器物理验证¦贴墙/非贴墙与层数上限",
"裂地踏一次圆形地脉波¦警示形状等于真实服务器范围与遮挡¦离圈、跨高度、遮挡",
"实体玄门阻挡部分冲锋与投射¦服务器权威门Actor，血量寿命/每人一扇/不能堵死出生点¦建门非法落点、破门、多人交叠",
"沉步以厚盾守势低速稳步推进¦每帧位移由合法地形与胶囊限制，硬控中止¦拐角、速度封顶、受控中断",
"前向护壁吸收有限攻击并一次反震¦正面扇区、容量与反震唯一事件ID¦背面攻击/盾破/多次反震",
"角顶针对正面冲锋敌人有限击退¦安全落点、质量及免控结算在服务器¦墙角/反向冲锋/强控制",
"立阵在场内增益一组友军¦持续区域实体和离阵即时移除，限制并发数量¦友军出阵/跨服切换/叠阵"
],
"Tiger":[
"白虎利爪向前挥砍攻击单体目标¦服务器一次近战确认才获气势¦目标逃出范围和重复命中",
"裂爪两段攻击并有限叠破甲¦只对当前同一目标叠层，有全局最大层数¦破甲刷新/第二段失败/换目标",
"双爪袭先后两段爪击¦分别结算，第二段必须可躲避¦第一段成功后目标死亡",
"虎啸预警→锁定扑杀→重爪收束¦目标及化身冻结，分段导航，不能靠视觉瞬移穿墙¦门盾阻挡/眩晕打断/末段取消",
"扑向合法猎物落点并一次伤害¦必须验证地面导航、实际胶囊与可达路径¦猎物穿门/断崖/落点拥堵",
"虎踞短暂正面抗扰和减伤¦主动出招或被硬控结束姿态，不可无敌¦绕背、持续消耗、姿态转向",
"虎啸扇区减速，孤立目标才短失衡¦孤立判定依服务器近距友军信息，尊重韧性¦有人接应/没人在旁/范围遮挡",
"开猎指定唯一猎物并强化限次攻击¦仅可见活目标，消失立即解除，不能全图追踪¦隐形/掩体/死亡/目标改换"
],
"Rabbit":[
"月辉单弹远程基础攻击¦服务器真实弹道命中才奖励气势¦侧闪、挡弹、重复碰撞",
"双玉刃与月闪后一次限时反击¦反击消耗一份被动凭证，不能自动弹射邻人¦未触发月闪/两段错开",
"落英月雨合法地面一次范围落击¦落点、半径、阻挡与警示一致¦跳离落区/低画质识别",
"最多三段月影闪击后尝试合法回到锚点¦全部走导航/胶囊，返回被堵保留最近合法位置¦锚点被破/中途目标失效",
"踏月合法短闪并触发极短闪避窗口¦服务端可追溯判定窗口，不依赖客户端帧率¦高延时/重复闪避/墙边",
"回月返回有效旧锚点或短后撤并有限自疗¦只能自疗、不赋予旧V5队友治疗；锚点数量1¦锚点超期/不合法/自疗重复",
"月痕减速，限次叠层后短缠足¦目标抗性/净化生效，禁止持续定身¦净化与叠层上限",
"月影生成短时可打破残像¦服务器有真实身份/寿命/血量，不制造无敌真身¦残像被攻击/反隐/旧世界清理"
],
"Dragon":[
"苍龙爪中短距一次打击¦服务器近程伤害与普攻气势一次性结算¦前向距离及遮挡",
"雷裁显著地面预告后下落雷柱¦目标可移出，不从客户端特效半径决定命中¦预告离开/高地遮挡",
"雷息锥形向前雷云吐息¦合法扇面内有限次结算，不能每帧重复扣血¦绕背/障碍/叠加",
"苍龙定天风压→云幕→雷裁三段¦各段独立定时与效果事实，不恢复天印/五行¦中断和退出时取消后继阶段",
"御风短位移顺带有限友方助速¦合法地面/胶囊，友军必须同世界且实际在范围¦跨世界/落点堵塞/状态退出",
"云护为有限合法友军附单次云盾¦护盾容量/层数上限、来源可撤销¦离开范围/多盾叠加",
"龙压内牵外推，满足条件才短麻痹¦服务器合法推拉落点、控效递减，不穿墙¦内外边界/壁障/多人",
"龙域持续局部风云雷压制¦同角色同时最多一域，每目标有时间窗及叠伤上限¦域外即时取消/多个领域叠加"
],
"Snake":[
"幽鳞刺单枚中程鳞针普攻¦服务器弹道且普攻收益只按合法命中¦被挡、目标失效",
"毒牙一击加短毒持续伤与轻抑疗¦同一来源叠毒次数/周期/总伤害上限明确¦净化、抑疗解除",
"双鳞弹两枚相互分离的弹道¦各自合法目标/碰撞ID，一目标至多两击¦第二枚躲开/互穿友军",
"万毒归鳞引爆自有毒纹三波毒潮¦只能操作同源未过期毒路径，每波独立结算¦先撤出毒区/清除自身毒路",
"蛇行短距弧线路径移动¦合法导航与碰撞，己方毒带只给限时移速¦毒带边界/断崖/门盾",
"蜕鳞有限减伤/清除指定软减速并留淡毒雾¦只净化白名单且毒雾持续/触发次数有限¦硬控阻断/毒雾到期",
"缠纹踩中先减速，停留达到门槛才短缠足¦离区重置计时，不能直接变为V2沉默¦停留时间/抗性/重入",
"封途地面毒路成长后可一次性爆发¦每角色同时最多两条，长度/持续/重叠有上限¦可绕行/穿墙禁止/跨世界清理"
],
"Horse":[
"雷蹄踢近距离一次普攻¦服务端合法命中后仅可发放一次气势奖励¦格挡/背向",
"雷枪正面刺击与有限冲势叠加¦只读服务器已核实移动距离和冲势层，倍率封顶¦诱导急停/换方向",
"雷蹄波一次圆圈震地¦有限半径和地形层高判定¦跳出环区/障碍",
"雷蹄破阵线形预告后冲锋与终点三段震击¦服务器逐段扫掠，撞墙提前结束，多目标不重复¦门盾/减速/脱离冲锋线",
"奔雷加速跑且直线行程积层¦最大速度/转角/减速事件由服务器控制¦引诱急转/硬控",
"踏阵冲锋中短减伤抗击退¦硬控能终止奔跑，不无条件霸体¦连控/拆队/破盾",
"蹄震针对近处突进者有限反推¦消耗本次冲势，免控/抗推生效¦侧绕、空技能、墙边",
"先至冲向合法待援友军并在抵达时生有限盾¦严格验证友军身份及其最新位置，离场不可任意传送¦切断支援/目标死亡/落点被占"
],
"Goat":[
"玉角刺一次短距魔法普攻¦服务器真实命中后只结算一次气势获得¦格挡/躲避",
"净光敌方轻伤或友军微量治疗二选一¦目标关系冻结，单次施放不能双效果¦错开光路/救援目标死亡",
"玉符环一次指定圆形打击¦权威区域目标去重，友方不能误伤¦离圈/遮挡",
"万愿归铃分波群疗、群盾、轻控净化¦各波对实时友军目标独立核验，总收益封顶¦分散、打断前摇/重伤",
"铃步向低血友军合法位移并少量治疗¦合法安全落点，只有抵达有效后才治疗¦友军位移/墙阻挡",
"守愿对单友军一次性有限致命保护¦服务端先于伤害结算检查，有全局触发冷却¦消耗守愿/等待结束",
"安魂扇面软减速并削弱下一次爆发¦不无条件沉默/眩晕，控制抗性必须生效¦脱离范围/净化",
"铃印对单友军一次净化和有限下次受疗增益¦只允许净化白名单且单次消耗¦逼出净化/再次控制"
],
"Monkey":[
"灵棍近身点打已判定的单体目标¦服务端首个合法命中加一次气势¦拉距",
"戏棍打击，误击假身的敌人有有限反击窗口¦假身被击中必须真实服务器事实，反击只触发一次¦看破假身/脱离",
"横扫棍正面扇区单次击中¦目标范围及可视边界精确一致¦绕背/离开扇区",
"万象戏台生成最多两假身并有限交换位置¦服务器假身实体/寿命/伤害上限和合法交换落点¦破假身、揭隐、阻路",
"筋斗越过标注低矮障碍并合法落地¦胶囊与导航，不得越过实体封闭墙¦高墙/狭道",
"替身与存活假身换位，否则给极短弱盾¦失败不强制传送，交换双方代次合法¦提前清假身/打断",
"扰心烟让新目标获取受扰且短减速¦不篡改已经飞行的权威弹道¦拉出烟区/面积伤害",
"百戏至多两个短期模仿假身，二段合法换位¦假身不输出高额真实伤害，二段有期限与代次¦揭露/消灭假身/位置阻断"
],
"Rooster":[
"金羽单枚远程基础攻击¦服务器弹道命中后才增加一次气势奖励¦闪躲/弹道阻断",
"破晓金羽攻击已揭露潜行或假身目标可得有限收益¦只识别已获权威揭露许可的目标¦离开揭露圈/净化",
"晨鸣波前方扇面伤害¦服务器范围有限目标，警示不被装饰掩盖¦跑出扇面",
"破晓金鸣领域揭露潜行、破相假身并轻护盾¦有限面积/视线权限/持续期限，敌方可撤离¦离场/拆阵/侧翼攻击",
"鸣步朝已获模糊侦察线索位移¦只能合法导航到本地落点，不可通过潜行猜精确坐标¦远离范围或掩体",
"金羽盾保护单友军并给受袭预警¦不泄露未知偷袭者全局坐标，权威GE护盾有容量¦击破护盾/绕后",
"鸣震软控，刚揭露者短沉默¦硬控仅条件触发且受抗性限制¦先手施法、抗性、脱圈",
"晨鸣揭隐并为受威胁友军提供极低治疗回响¦副治疗是V6.1待批准新候选，不能复用旧V5大额治疗¦切断照明/远离治愈范围"
],
"Dog":[
"天犬咬一次近距普攻¦服务器近身命中后仅加一次气势奖励¦离距",
"反咬最近攻击受护对象的可见敌人¦攻击者身份与时间窗口有上限，不可跨图追杀¦转移目标/离开护卫",
"双灵爪对同一个合法目标两次攻击¦每段独立命中和伤害去重¦闪开第二段",
"天门守阙放置可破坏定向门盾保护绑定队友¦服务器有实体门/碰撞血量/寿命，不能堵出生安全线¦破盾/绕后",
"援步向绑定友军合法短位移并给轻守护¦绑定唯一、世界/落点严格一致¦分散、撞墙",
"护主为绑定对象分担有上限的致命威胁¦分担不可循环递归、不能无限生存¦拆开护卫/破盾",
"拦门短门盾阻断正面冲锋并轻控¦根据真实冲锋状态与抗性，不能全方位拦截¦侧绕/强力拆盾",
"誓约只绑定一名合法友军提供护卫收益¦重新绑定必须释放旧来源，不是旧V5直接治疗¦迫使护卫离开/契约失效"
],
"Boar":[
"獠牙向近身首个有效目标发动一次冲撞¦服务器真实命中后只增加一次气势¦拉开距离",
"怒锤按最近受伤战意小幅增伤¦战意层数和总增幅封顶，无无限反扑¦控制打断/停止集火",
"重压波一次近身地波范围打击¦合法地面范围/遮挡目标去重¦离开圈",
"玄鬃裂地三段推进重踏，末段低血加成¦三段时序独立结算，目标失效停止后段；V2两段数值不直接沿用¦脱离路径/打断",
"獠撞合法路径直线顶首个碰撞敌人¦胶囊扫掠/地形高差/强制移动落点安全¦侧闪、门盾",
"岩甲有限护盾且破盾一次反震¦反震唯一事件ID，破盾同帧只触发一次¦消耗盾/绕背",
"震鬃附近减速，已有岩甲才附一次轻击退¦护盾资格被破后不能迟到发击退¦先破盾、远离范围",
"鬃吼加战意并保护身后附近少数友军¦正面/背后扇区由服务器验证，不强制嘲讽¦分割阵型/眩晕打断"
]
}

PASSIVES={
"Rat":"嗅弱：仅近处已可见低血或落单敌人展示模糊方向，不能透视隐身；有范围/时长上限",
"Ox":"镇山：站稳增加正面减伤、最多3层，背面无收益，移动或被击退渐退",
"Tiger":"裂甲：专注同一猎物叠有上限护甲削减，失去目标消散",
"Rabbit":"月闪：真实服务器闪避成功才保存一次短期反击充能，超时消失",
"Dragon":"天象轮转：已批准技能序列的风云雷三态增益，独立于被删除的五行和王印",
"Snake":"毒记：有界记录自身毒纹和单目标中毒接触，解毒及换世界即清空",
"Horse":"奔雷之势：按照权威直线移动距离获得最多3层冲势，急转/硬控掉层",
"Goat":"灵愿：治疗和护盾附有限危险预警，不共享敌人未获准视野",
"Monkey":"百戏本能：短期假身模仿起手，但没有高额权威伤害，最大2假身",
"Rooster":"暗迹：近距模糊揭隐情报，不提供精确全图透视",
"Dog":"守阙：绑定唯一友军的受限护卫增益，脱离护卫范围即衰减",
"Boar":"战意：近期真实承伤和低血获得最多3层反扑状态，脱战衰减"
}
# 听感名称仅为音频设计，不能冒充SoundWave(声音波)原生资产。
AUDIO_DNA={
"Rat":"短促牙刃、影步掠风、弱灰银低频",
"Ox":"沉重角撞、门石断裂、低频地震",
"Tiger":"低虎啸、白金双爪切风、末段重击",
"Rabbit":"玉弦轻鸣、月刃清脆双声、回返余响",
"Dragon":"云风长气流、青白雷柱、电弧收束",
"Snake":"细蛇嘶、鳞刃掠空、毒径液态脉冲",
"Horse":"连续重蹄、金雷脉、冲锋速度变化",
"Goat":"清玉铃、祈愿柔光、净化与治疗分离",
"Monkey":"金棍扫风、假身空响、真假换位敲击",
"Rooster":"晨鸣声浪、金羽锋鸣、揭隐与疗护回响",
"Dog":"天门金石撞击、守卫短吼、誓约护卫提示",
"Boar":"岩甲摩擦、獠牙重撞、逐段裂地低频"
}
VFX_THREE_VIEWS=[
"45度斜俯视：准确确认敌我、真实预警范围及团队遮挡",
"正面中景：确认角色血统、主体技能造型、材质与施法动作",
"侧面轨迹：确认弹道/位移高度、碰撞落点、打击反馈与取消"
]
SFX_CUE_STAGES=["Cast(施法)","Release(释放)","Impact(命中或生效)","Cancel(取消)","End(结束)"]
ANIM_STAGES=["普攻起手/命中/收势","攻击准备/释放/收势","多段连击/中断/收势",
"终极预警/多阶段/终结","移动启动/行进/落地","防御启动/维持/被破",
"控制预读/执行/解除","特色启动/持续/撤销"]
SLOTS=["Primary","Slot1","Slot2","Slot4","Slot3","Slot5","Slot6","Slot7"]
ROLES=["普攻","攻击一","攻击二","终极","移动","防御","控制","特色/治疗"]
STARTUP_CANDIDATE=[0.12,0.26,0.30,0.65,0.14,0.14,0.30,0.22]

def load(path): return json.loads(path.read_text(encoding="utf-8-sig"))
def author():
 v6,v2,v5,reg=map(load,(OLD_V6,OLD_V2,V5,REG))
 v2_skill={x["abilityId"]:x for h in v2["heroes"] for x in h["slots"]}
 v5_name={r[0]:r[1] for r in v5["entries"]}
 reg_skill={s["abilityId"]:s for s in reg["skills"]}
 heroes=[];renames=[]
 for h in v6["heroes"]:
  code=h["heroId"].split(".")[-1];assert len(REVIEW[code])==8
  skills=[]
  for idx,s in enumerate(h["skills"]):
   a=s["abilityId"];old=v2_skill[a];execution=reg_skill[a]
   assert s["slotId"]==old["slotId"]==SLOTS[idx],(code,idx)
   assert s["displayOrder"]==idx+1 and v5_name[a]==old["nameZh"]
   contract,boundary,tests=REVIEW[code][idx].split("¦")
   renamed=s["proposedNameZh"]!=v5_name[a]
   if renamed:renames.append(dict(heroId=h["heroId"],abilityId=a,
                                  approvedNameV5=v5_name[a],proposalV61=s["proposedNameZh"],
                                  slot=idx+1))
   skills.append({
    "abilityId":a,"slot":idx+1,"slotId":SLOTS[idx],"roleZh":ROLES[idx],
    "approvedDisplayNameV5":v5_name[a],"proposedNameV61":s["proposedNameZh"],
    "renameNeedsApproval":renamed,"historicalV6EffectZh":s["descriptionZh"],
    "authorityExecutionZh":contract,"runtimeBoundariesZh":boundary,
    "playerCounterplayZh":s["playerCounterplayZh"],
    "criticalNativeTestsZh":tests,"visualDNAZh":h["heroVisualDNA"],
    "visualKeyframesFromV6ProposalZh":s["visualEffectDesign"]["visualKeyframes"],
    "conceptArtPromptZh":s["visualEffectDesign"]["mainScenePromptZh"],
    "threeAngleVisualReviewZh":VFX_THREE_VIEWS,
    "criticalVFXSemanticsZh":"预警（真实范围）→核心起手→释放/行进（如需）→权威命中或保护事实→取消/消散；关键范围低档不剔除",
    "vfxAirDistortionProposalZh":"按该技能元素与时序施加克制的空气折动或压力波；同源取消、不得参与服务器命中",
    "sfxPaletteProposalZh":AUDIO_DNA[code],
    "sfxRequiredCuesZh":SFX_CUE_STAGES,
    "animationSequenceProposalZh":ANIM_STAGES[idx],
    "presentationCanAlterServerOutcome":False,
    "level1V2ReferenceNotApproved":{
      "damage":old["baseDamage"],"hits":old["hitCount"],"cooldownSec":old["cooldownSeconds"],
      "rangeCm":old["castRangeCm"],"radiusCm":old["areaRadiusCm"],
      "momentumCost":old["momentumCost"],"energyCost":old["energyCost"],
      "v2Actions":len(old["actions"])},
    "suggestedWindupSecondsNotApproved":STARTUP_CANDIDATE[idx],
    "candidateNativePrimitiveSourceOnly":execution["proposedPrimitiveCandidate"],
    "gameplayReviewed":False,"nameChangeReviewed":not renamed,
    "balanceReviewed":False,"nativeDefinitionPath":None,"nativeGAPath":None,
    "nativeGEPath":None,"nativeNiagaraPath":None,"nativeSFXPath":None,
    "nativeMontagePath":None,"nativeIcon48Path":None,"canGrantServer":False,
    "visualHumanApproved":False})
  heroes.append({"heroId":h["heroId"],"nameZh":h["currentHeroDisplayNameZh"],
                "roleZh":h["battleRoleZh"],"visualDNAZh":h["heroVisualDNA"],
                "passive":{"nameZh":h["passive"]["nameZh"],"designZh":PASSIVES[code],
                           "occupiesSlot":False,"approved":False},
                "skills":skills})
 assert len(heroes)==12 and sum(len(h["skills"]) for h in heroes)==96
 assert len(renames)==47,(len(renames))
 return {"schemaVersion":1,"version":"V6.1-DESIGN-ONLY","status":"PENDING_USER_REVIEW",
 "sources":{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (OLD_V6,OLD_V2,V5,REG)},
 "heroCount":12,"skillCount":96,"innatePassives":12,
 "V5NamesStillProductionAuthority":True,"renamedCount":len(renames),
 "nameChangeReviewQueue":renames,
 "currentPrimitiveCandidateCounts":reg["candidateModeCounts"],
 "newUEAssetsCreated":False,"serverAbilitiesApproved":0,
 "noGameplaySystemChange":True,
 "resourcePolicyZh":"生命为绿、能量为蓝、气势为独立暖金；V2全96项能量费用为零，法术气势消耗为零；不恢复旧五行、王印、破元与共鸣",
 "teamRoleDecisionCandidateZh":"卯兔保留自疗但不任主治疗；未羊为主治疗；酉鸡晨鸣建议极小条件副治疗；戌狗为绑定护卫而非旧直接治疗；四项职责须人工确认",
 "heroes":heroes}

def render(data):
 lines=[
 "# SSLM 十二生肖技能 V6.1 完善设计方案（待审批）","",
 "> 本文件是独立设计提案，不修改V5正式显示名、96个AbilityId、12套旧占位技能集，不代表UE原生GA/VFX/蓝图制作完成。",
 "",
 "## 一、设计范围及禁止突破的边界","",
 f"**规模：**{data['heroCount']}生肖×8主动技能={data['skillCount']}项，另有{data['innatePassives']}项不占槽的固有被动。V6.1拟议名与V5现名不一致{data['renamedCount']}项，需要逐AbilityId审批。",
 "各英雄一到八槽顺序：普攻、攻击一、攻击二、终极、移动、防御、控制、特色/治疗；稳定输入Tag与原V2一致。",
 "战斗权威依赖既有GamePlatformAbilitySystem（平台GAS）/GamePlatformCombat（权威战斗），项目DivineBeastsAbilitiesRuntime只补生肖行为；GamePlatformPresentation和客户端VFX/SFX只消费结果，不以特效碰撞产生伤害。",
 "禁止恢复已废止的五行克制、王印、破元、共鸣和生肖旧阵营关系。蓝色能量条与独立暖金气势条不混淆。",
 "V2伤害、CD、距离是未平衡的历史一级候选；本稿首段前摇秒数也是仅供动画设计的候选，**均不得写入正式DataAsset作为已批准平衡**。",
 "",
 "## 二、共用服务器行为和玩家反制合同","",
 "| 检查 | 规定 |","|---|---|",
 "| 权威与回滚 | 先校验世界/来源/目标/化身代次再由GAS提交费用冷却；提交后再次复核。不可用VFX画面或UI预测决定命中。|",
 "| 部分成功 | 已命中的每一段要发HitConfirmed（命中事实）；后续失败不得抹去已发生伤害；CastAccepted（施法受理）与最终PartialOutcome（部分成功）分开记录。|",
 "| 多段/区域 | 每段唯一伤害ID、击中目标有界去重，候选最多16个有效目标；提示的半径应与服务端区域一致。|",
 "| 位移/冲锋 | 服务器导航、胶囊、坡度、动态遮挡与旧目标代次检查；不能穿封闭墙，也不能强行超高差落地。|",
 "| 持续区域/实体 | 真实门盾、假身、毒带、领域都有所有者、数量、生命、终态和世界退出释放；不靠Niagara模拟实体。|",
 "| 控制/反隐 | 控制强度与抗性递减可验证；情报只显示合法范围内模糊方向，不获得全图真实坐标。|",
 "| 表现与性能 | 45°真实战斗视角先辨识预警/方向/受击，另从正面及侧面复核；低画质保留核心表现。Niagara采用现有7类Effect Type与预算机制，四档画质和多人实际测量。|",
 "",
 "### 角色团队职能待决策","",
 "建议：未羊专职群体治疗/净化；卯兔保留小额自疗但定位闪避剑士；酉鸡以揭隐为主、晨鸣可带少量条件副治疗；戌狗以绑定队友、门盾及分担伤害替代V5的直接回血。四者设计变化需策划签收，不能默认为已批准。",
 "",
 "## 三、十二生肖逐项完善结果",""
 ]
 for h in data["heroes"]:
  lines += [f"### {h['nameZh']}（{h['roleZh']}）","",
   f"**被动「{h['passive']['nameZh']}」：**{h['passive']['designZh']}。不占八槽。",
   f"**视觉形态：**{h['visualDNAZh']}。",
   f"**音效方向：**{AUDIO_DNA[h['heroId'].split('.')[-1]]}。Cast(施法)、Release(释放)、Impact(生效)、Cancel(取消)、End(结束)各有事件映射，不是已导入声音资产。",
   "**三机位人工视觉检查：**"+"；".join(VFX_THREE_VIEWS)+"。","",
   "| 槽位／V6.1拟议名 | 技能效果与实际执行约束 | 反制与重点验收 | V2一级CD候选（秒） |",
   "|---|---|---|---:|"]
  for s in h["skills"]:
   diff="（待重新批准名称）" if s["renameNeedsApproval"] else ""
   lines.append(f"| {s['slot']} {s['proposedNameV61']}{diff} | "
     f"{s['historicalV6EffectZh']}；**权威补充：**{s['authorityExecutionZh']}；"
     f"**上限/清理：**{s['runtimeBoundariesZh']} | "
     f"{s['playerCounterplayZh']}；**验证：**{s['criticalNativeTestsZh']} | "
     f"{s['level1V2ReferenceNotApproved']['cooldownSec']} |")
  lines += ["", "**本英雄可追踪的历史名称分歧：**"+
            "；".join(f"{s['slot']}槽 {s['approvedDisplayNameV5']}→{s['proposedNameV61']}"
                 for s in h["skills"] if s["renameNeedsApproval"]) if
            any(s["renameNeedsApproval"] for s in h["skills"]) else "本英雄没有V5/V6命名分歧。",""]
 lines += [
 "## 四、装备到客户端与人工可视化审核","",
 "沿用唯一DemoHub（插件演示中心）和GamePlatformDeveloperTools审核模块，按12生肖逐一展示8槽与被动，至少提供目标木偶/移动敌人/友军、强弱控制、障碍与遮挡、距离/圈边缘、失败取消、切换英雄与世界、回放/证据、运行日志及FPS/GPU实例数的真实可操作界面。",
 "每技能固定45°斜俯视、正面、侧面三视角；可操作起手、释放、移动/飞行、命中/护盾、取消/结束。必要时可切Low/Medium/High/Epic画质，但绝不只展示图片冒充Niagara。",
 "重要资源分类：Definition/GA/GE与碰撞导航属于服务端必要内容；Niagara/图标/UI/可选SFX只进入客户端，须以正式Client/Server Build与真Cook对比证明剥离。",
 "",
 "## 五、实施优先级","",
 "P0：V5/V6.1名称47项冲突与四治疗角色定位先审批；96项独立行为和数值未获批准不得授予。P1：先做12生肖各一项代表技能的真实GA及完整VFX，逐一验证服务器权威、取消和反制。P2：补有界多段/门盾/毒路/假身/反隐/免死等复杂机制，先通过原生行为测试再扩展全96。P3：竞技1v1—5v5的四画质和十人同屏性能、真实角色创建/技能栏8格、人工签收与逐英雄发布切换。",
 "",
 "**当前状态：设计文件及数据源生成，不改已有原生技能集；96项V6.1技能均未获得生产批准。**"
 ]
 return "\n".join(lines)+"\n"

def expected():
 d=author()
 return {OUT_JSON:json.dumps(d,ensure_ascii=False,indent=2)+"\n",OUT_MD:render(d)}

def main():
 p=argparse.ArgumentParser(description="SSLM V6.1技能完善设计")
 p.add_argument("--write",action="store_true")
 p.add_argument("--check",action="store_true")
 args=p.parse_args()
 if args.write and args.check:p.error("仅允许一种模式")
 files=expected()
 if args.check:
  if any(not path.is_file() or path.read_text(encoding="utf-8")!=body for path,body in files.items()):
   raise SystemExit("V6.1方案与批准源/历史审稿数据不一致")
 if args.write:
  for path,body in files.items():
   path.parent.mkdir(parents=True,exist_ok=True)
   path.write_text(body,encoding="utf-8")
 d=author()
 print(f"SSLM_V61_DESIGN_PASS heroes={d['heroCount']} skills={d['skillCount']} "
       f"passives={d['innatePassives']} renameApprovalPending={d['renamedCount']} "
       f"nativeAssetsCreated={d['newUEAssetsCreated']} granted={d['serverAbilitiesApproved']}")
 return 0

if __name__=="__main__":
 raise SystemExit(main())
