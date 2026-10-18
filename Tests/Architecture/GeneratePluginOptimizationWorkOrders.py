#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM全插件优化工作单生成器 V2。

职责：从当前67项质量派生台账编排M1—M6实施工作单，补充逐插件设计责任、
待核查业务能力、原生测试、端侧、人工操作与证据门禁。
来源：质量派生台账是静态事实；本文件显式规则只是后续整改计划，不是已实现的接口/资产。
副作用：默认仅摘要；--write仅写Docs/Implementation/PluginQualityAudit中的两份文档；
--check只读比较，拒绝陈旧或人为提升的完成状态。绝不创建UE二进制资源或签发人工通过。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Docs/Implementation/PluginQualityAudit/CurrentPluginQualityMatrix.json"
OUTPUT_DIR = SOURCE.parent
OUTPUT_JSON = OUTPUT_DIR / "PluginOptimizationWorkOrders_V2.json"
OUTPUT_MD = OUTPUT_DIR / "PluginOptimizationWorkOrders_V2.md"

# 每个代码插件只设一个主实施批次；M0为全局基线、M7为统一发布。
# 这里是待实施能力审核目标，不是对真实功能代码“已完成”的声明。
CODE_GROUPS = {
    "M1": {
        "GamePlatformCore": ("基础契约", "验证稳定身份、错误码、依赖不变量与失败结果"),
        "GamePlatformData": ("数据与租约", "复核单一资产身份、并发加载合并、Owner租约和异步取消"),
        "GamePlatformApplicationFlow": ("流程执行", "复核登录到三世界的状态切换、失败回退和重复触发"),
        "GamePlatformSession": ("跨服会话", "复验同步失败、取消、四事实Ready和ClientTravel旧代次阻断"),
        "GamePlatformOnline": ("在线基础", "验证鉴权、连接时序、失败重试、过期响应与解绑"),
        "GamePlatformServer": ("服务器生命周期", "验证就绪、准入、排空、关服回调和Agones连接边界"),
        "GamePlatformTelemetry": ("遥测", "复验Ticker有限重试、关闭幂等、脱敏及长时待处理"),
        "GamePlatformSave": ("保存", "验证权限、原子性、版本恢复与并发写入失败处理"),
        "GamePlatformEntitlement": ("权益", "验证权益来源、撤销、缓存隔离与权限失败收口"),
        "GamePlatformLiveOps": ("运营活动", "验证运营配置版本、时间区间、过期行为和资格隔离"),
        "GamePlatformLoading": ("加载与就绪", "验证资源就绪事件、超时/取消及UI加载态结束通知"),
    },
    "M2": {
        "GamePlatformWorld": ("世界上下文", "验证三种服务器角色、世界代次和区域流送的订阅生命周期"),
        "GamePlatformPCG": ("程序化内容生成", "验证Gold图、实际Spawner输出、编排、烘焙与权威边界"),
        "GamePlatformWater": ("水体", "验证服务器水域权威信息与客户端材质/模拟资源隔离"),
        "GamePlatformWeather": ("天气", "验证权威天气调度、客户端天气切换和失效快照"),
        "GamePlatformNavigation": ("导航", "验证服务器导航、动态障碍、路径失败和世界退出"),
        "GamePlatformArena": ("通用竞技", "验证1v1—5v5模式数据、竞技实例和Client/Server职责"),
        "DBAServer": ("项目服务器", "验证项目准入、权威玩法配置和三角色服务器组合"),
        "DBAWorlds": ("项目世界", "验证Village与OpenWorld实际World定义、PCG及跨世界接入"),
        "DBAArena": ("项目竞技", "验证竞技项目数据、出生/结算和真实服务器权威结果"),
    },
    "M3": {
        "GamePlatformDeveloperTools": ("编辑器审核", "基于既有ReviewCase/Report建真实证据与实名人工签收入口"),
        "GamePlatformUI": ("平台界面", "验证事件驱动层栈、焦点/输入、资源租约与页面重载"),
        "GamePlatformSettings": ("统一设置", "验证Provider参数应用、设备配置回退与权限隔离"),
        "GamePlatformInput": ("输入", "验证键鼠/手柄/触屏输入映射、焦点冲突与失活回收"),
        "GamePlatformDebug": ("开发诊断", "验证开发模式开关、命令资格与Shipping隔离"),
        "GamePlatformLocalization": ("本地化", "核实真实文本与运行时切语言能力；模块壳不得冒充功能"),
        "GamePlatformCommerceUI": ("交易界面", "验证展示端与钱包/订单权威隔离、敏感信息清理"),
        "DBAClient": ("项目客户端", "完善唯一DemoHub、项目UI接线、异常反馈与真实操作入口"),
    },
    "M4": {
        "GamePlatformVFX": ("视觉特效", "验证唯一Niagara服务、7类Effect Type、NDC降级与实例池"),
        "GamePlatformSFX": ("音频", "验证播放/停止、空间化、资源缺失与音频实例回收"),
        "GamePlatformCamera": ("摄像机", "验证游戏/角色/战斗镜头、混合、阻挡与复位"),
        "GamePlatformSurface": ("环境表面", "验证雪、苔藓、湿润的共享材质函数与端侧隔离"),
        "GamePlatformPresentation": ("中立表现语义", "验证中立请求、提供者注册、取消和目录回退"),
        "GamePlatformAnimation": ("动画", "验证动作蒙太奇/BlendSpace、根运动权威与动画事件取消"),
        "MobaPresentation": ("MOBA表现语义", "验证竞技事件映射、不播放Niagara及卸载竞技后独立世界"),
    },
    "M5": {
        "GamePlatformAbilitySystem": ("能力系统", "复核GAS授权、冷却、费用、取消与服务器权威"),
        "GamePlatformAI": ("人工智能", "验证感知、目标选择、导航和任务取消"),
        "GamePlatformCharacter": ("角色", "验证身份与运动组件、切角色、生命周期与复制"),
        "GamePlatformCombat": ("战斗", "验证权威伤害/治疗、命中重复、碰撞与表现解耦"),
        "GamePlatformEquipment": ("装备", "验证装备与背包事务、权限、撤销与角色同步"),
        "GamePlatformGameplay": ("玩法装配", "验证通用玩法启动、停止和跨世界回收"),
        "GamePlatformInteraction": ("交互", "验证交互资格、距离、状态机和多人同时操作"),
        "GamePlatformInventory": ("背包", "验证事务一致性、版本、幂等、撤销和资源隔离"),
        "GamePlatformProgression": ("成长", "验证服务器进度事实、加点/升级与多次事件去重"),
        "GamePlatformQuest": ("任务", "验证目标进度、奖励幂等和失败补偿"),
        "GamePlatformLobby": ("通用大厅", "核实OpenWorld大厅真实消费者及模块壳缺口，不创建独立大厅服务器"),
        "GamePlatformVillage": ("通用新手村", "核实Village教学/训练实际消费者及模块壳缺口"),
        "DBAGameplay": ("项目玩法", "验证生肖英雄数据/真实技能、GAS权限与Village出生流程"),
    },
}

HERO_NAMES = {
    "Rat": "子鼠", "Ox": "丑牛", "Tiger": "寅虎", "Rabbit": "卯兔",
    "Dragon": "辰龙", "Snake": "巳蛇", "Horse": "午马", "Goat": "未羊",
    "Monkey": "申猴", "Rooster": "酉鸡", "Dog": "戌狗", "Boar": "亥猪",
}

PACK_GOALS = {
    "DBAContentPack_Common": "公共角色、模型/材质、共享资源与服务器权威依赖审核",
    "DBAFrontEndPack": "登录、英雄选择与创建的真实Widget/场景及客户端权限",
    "DBAUIPack_Core": "项目页面、HUD、主题、事件交互、焦点与真实原生UI质量审核",
    "DBAPresentationPack_Core": "项目VFX目录/定义与真实Niagara/材质消费者、性能和资源许可",
    "DBASFXPack_Core": "项目音效资产、试听/停止、混音与服务器资源排除",
    "DBAWorldPack_Village": "新手村地图、森林农田/道路PCG、碰撞导航/出生与人工观察",
    "DBAWorldPack_MainArena": "主竞技场地图/掩体视野、五种竞技人数与静态环境性能",
}

# 真实功能案例待确认前，保留P0/P1/P2/P3来源等级，任务排序只是实施顺序。
SPECIAL_ALREADY_CHANGED = {
    "GamePlatformSession": "已有同步失败/取消与事实回调保护源码；需要最新UE自动化与双客户端网络验收",
    "GamePlatformTelemetry": "已有重试句柄触发即移除与Shutdown幂等源码；需要UE原生回归和长时指标",
    "GamePlatformSave": "已加存储层写入限额、备份临时清理和进程内跨GameInstance同槽互斥；旧实例取消时后台线程仍持有租约；原生用例源码已补，仍需最新版UE UHT/模块链接、真实并发IO、跨进程隔离及人工确认",
    "GamePlatformVFX": "已有鸟群按物种频率刷新优化；需UE Niagara实例性能与人工画面对照",
    "GamePlatformPCG": "已有金标准资产和部分原生验证；正式Village/OpenWorld接线、烘焙与Cook仍待证据",
    "DBAClient": "已有DemoHub及诊断地图快捷入口；仅正式案例和人工审核闭环满足后才可提升",
    "GamePlatformUI": "已有正式界面案例编目；仍需真实Monolith资产、输入与人工视图验收",
    "GamePlatformDeveloperTools": "已有ReviewCase/ReviewReport/ReviewSubsystem；不可创建平行审核框架",
}

PHASE_LABELS = {
    "M1": "运行期生命周期与安全",
    "M2": "客户端/专服构建与三世界",
    "M3": "唯一演示中心与人工审核",
    "M4": "视觉表现与环境",
    "M5": "权威玩法与场景",
    "M6": "19个内容包逐项交付",
}

VALID_TARGETS = {
    "M1": "双端/服务端/编辑器按实际模块职责",
    "M2": "Editor/Client/Dedicated Server分别核验",
    "M3": "Editor与Client；审核仅Editor，UI不得引用编辑器模块",
    "M4": "客户端表现与编辑器资源；服务端仅校验权威最小依赖",
    "M5": "Client/Server/Editor；权威测试必须专服或网络",
    "M6": "内容消费者端侧与真Client/Server Cook产物",
}


def read_source() -> tuple[dict, bytes]:
    raw = SOURCE.read_bytes()
    return json.loads(raw.decode("utf-8-sig")), raw


def code_assignments() -> dict[str, tuple[str, str, str]]:
    result = {}
    for phase, group in CODE_GROUPS.items():
        for plugin_id, (domain, goal) in group.items():
            if plugin_id in result:
                raise ValueError("代码插件有重复批次：" + plugin_id)
            result[plugin_id] = (phase, domain, goal)
    return result


def build_workorders(matrix: dict, matrix_bytes: bytes) -> dict:
    known = {p["pluginId"]: p for p in matrix["plugins"]}
    codes = code_assignments()
    real_code = {name for name, p in known.items() if p["kind"] == "code"}
    if real_code != set(codes):
        raise ValueError("代码批次身份与当前真实台账不一致: missing=" +
                         ",".join(sorted(real_code - set(codes))) +
                         " obsolete=" + ",".join(sorted(set(codes) - real_code)))
    content_names = {name for name, p in known.items() if p["kind"] == "content_pack"}
    expected_content = set(PACK_GOALS) | {"DBAHeroPack_" + suffix for suffix in HERO_NAMES}
    if expected_content != content_names:
        raise ValueError("内容包真实身份已变化，必须人工评审更新工作单规则")
    if matrix.get("issues"):
        raise ValueError("当前台账报告了结构问题，禁止生成貌似正常的工作单")

    jobs = []
    for identity, p in known.items():
        if p["kind"] == "content_pack":
            phase, domain = "M6", "内容包"
            if identity.startswith("DBAHeroPack_"):
                hero = identity.removeprefix("DBAHeroPack_")
                goal = f"审核{HERO_NAMES[hero]}真实角色网格、动画、材质、技能资源、软引用/许可和端侧Cook"
            else:
                goal = PACK_GOALS[identity]
        else:
            phase, domain, goal = codes[identity]
        priority = p["priority"]
        visual = bool(p["visualReviewRequired"])
        registered = int(p["registeredScenarios"])
        diagnostic = p["diagnosticMapPackage"]
        needs_formal = visual and registered == 0
        if diagnostic and not visual:
            raise ValueError("非视觉域不能凭诊断地图升级正式验收：" + identity)
        if p["humanApproved"]:
            raise ValueError("静态工作单不能继承伪造的人工通过状态：" + identity)
        if p["diagnosticMapIsFormalScenario"]:
            raise ValueError("诊断地图被误记为正式案例：" + identity)

        visual_task = (
            "基于真实UE资源与现有DemoHub制作可启动、参数调整、执行、停止、重置、返回的正式操作案例；"
            "仅由Monolith制作项目Widget资产。诊断地图可复用，但不能算正式功能完成"
            if needs_formal else
            "核对已登记正式案例与实际功能及异常流程的对应关系，执行真实UE操作和人工审核"
            if visual else
            "非视觉机制以原生测试和必要的可操作服务诊断台验收，不为形式制作空3D地图"
        )
        base_tasks = [
            {"id": f"{identity}-01", "kind": "功能与边界审查",
             "task": "读取实际公开API、消费者、运行端侧、现有测试和资源引用；确定真实功能分母及所有权范围"},
            {"id": f"{identity}-02", "kind": "最小代码/资源整改",
             "task": goal + "；只修改已复现的缺陷或真实缺失，不建立平行模块"},
            {"id": f"{identity}-03", "kind": "人工界面与场景接入",
             "task": visual_task},
            {"id": f"{identity}-04", "kind": "原生与发布证据",
             "task": "按职责执行定向UE5.8模块/蓝图加载、正常失败取消回归、双端依赖/Cook和条件性能采样，保留原生日志/版本指纹"},
            {"id": f"{identity}-05", "kind": "人工审核与交付收口",
             "task": "由既有GamePlatformDeveloperTools ReviewSubsystem登记当前版本Pending待审证据；"
                     "真实Reviewer显式判断并追加修订后才可能进入人工通过"},
        ]
        jobs.append({
            "workOrderId": "SSLM-PLG-" + identity,
            "pluginId": identity,
            "priorityFromAudit": priority,
            "primaryBatch": phase,
            "batchDescription": PHASE_LABELS[phase],
            "domain": domain,
            "layer": p["layer"],
            "pluginKind": p["kind"],
            "descriptorPath": p["descriptor"],
            "declaredModuleCount": p["moduleCount"],
            "moduleBuildRules": p["moduleBuildRules"],
            "sourceFileCount": p["cppFiles"] + p["headerFiles"],
            "nativeAssetCount": p["nativeAssets"],
            "nativeMapCount": p["mapAssets"],
            "moduleNativeTestFileCount": p["nativeTestFiles"],
            "visualReviewRequired": visual,
            "existingDiagnosticMap": diagnostic,
            "diagnosticIsFormalScenario": False,
            "formalScenarioCount": registered,
            "formalScenarioGap": needs_formal,
            "humanApproval": "not_verified",
            "completionStatus": "not_implemented_by_this_plan",
            "runtimePerformanceStatus": "not_measured_by_this_plan",
            "firstStepFocus": goal,
            "alreadyChangedNotes": SPECIAL_ALREADY_CHANGED.get(identity, ""),
            "targetReview": VALID_TARGETS[phase],
            "sourcePerformanceCandidates": p["performanceReviewCandidates"],
            "tasks": base_tasks,
            "acceptanceEvidence": [
                "真实接口/消费者与中文职责说明",
                "当前Build.cs/端侧依赖及源码改动差异",
                "当期UE5.8实际编译链接/必要自动化结果",
                "如适用的真实蓝图、地图冷重载和资源消费者",
                "如适用的Client/Server真Cook容器资源差异",
                "实际CPU/GPU采样或明确不适用及审核理由",
                "如需视觉/交互的DemoHub正式操作及Editor人工待审报告",
            ],
            "blockingConditions": [
                "未确认的功能分母不能计算覆盖率",
                "诊断地图不是正式可操作案例",
                "没有当期UE/性能/人工证据不得宣称ProductionReady",
            ],
        })

    jobs.sort(key=lambda x: (x["primaryBatch"], 0 if x["priorityFromAudit"] == "P0" else 1,
                             x["pluginId"].casefold()))
    phases = dict(Counter(x["primaryBatch"] for x in jobs))
    return {
        "schemaVersion": 2,
        "status": "DESIGN_WORK_ORDERS_ONLY_NOT_IMPLEMENTATION",
        "sourceFile": SOURCE.relative_to(ROOT).as_posix(),
        "sourceSha256": hashlib.sha256(matrix_bytes).hexdigest(),
        "sourceLimitations": "来自静态审查；功能分母未知。所有工作单均未由本脚本签署完成，所有人工签收保持未验证。",
        "globalPrerequisites": ["M0 核对全部当前身份与功能分母", "AGENTS.md 及真实工程/源码版本",
                                "保护并行任务工作树与活动UE资产编辑器"],
        "finalAcceptance": "M7 当前版本的正式UE编译/测试/Cook/性能/人工签收全维度通过才可考虑ProductionReady",
        "totalPluginWorkOrders": len(jobs),
        "codePluginWorkOrders": sum(x["pluginKind"] == "code" for x in jobs),
        "contentPackWorkOrders": sum(x["pluginKind"] == "content_pack" for x in jobs),
        "formalScenarioGaps": sum(x["formalScenarioGap"] for x in jobs),
        "visualReviewRequired": sum(x["visualReviewRequired"] for x in jobs),
        "diagnosticShortcutPlugins": sum(bool(x["existingDiagnosticMap"]) for x in jobs),
        "existingFormalScenarioRegistrations": sum(x["formalScenarioCount"] for x in jobs),
        "humanApproved": 0,
        "phaseCounts": phases,
        "workOrders": jobs,
    }


def to_markdown(data: dict) -> str:
    lines = [
        "# SSLM 插件优化设计 V2.0：67个插件实施工作单",
        "",
        "> 本文由当前真实67插件质量台账自动派生，按照M1—M6分组；每个条目都是未来待执行任务，"
        "不是已实施或人工审核通过。M0对所有插件执行基线检查，M7负责统一生产门禁。",
        "",
        f"- 当前工作单：{data['totalPluginWorkOrders']}；机制插件：{data['codePluginWorkOrders']}；"
        f"内容包：{data['contentPackWorkOrders']}；需要人工可视化：{data['visualReviewRequired']}；"
        f"尚缺正式操作案例：{data['formalScenarioGaps']}；人工通过：0。",
        f"- 已有诊断地图快捷入口涉及{data['diagnosticShortcutPlugins']}个插件；"
        "诊断地图不作为正式可操作功能的完成证据。",
        "",
        "## 各批次实施顺序",
        "",
        "| 批次 | 中文职责 | 工作单数 | 执行依据 |",
        "|---|---|---:|---|",
    ]
    for batch in sorted(PHASE_LABELS):
        lines.append(f"| {batch} | {PHASE_LABELS[batch]} | {data['phaseCounts'].get(batch,0)} "
                     "| 优先本批明确风险；前置M0、最终M7 |")
    lines += [
        "",
        "## 每插件独立工作单",
        "",
        "| 工作单 | 阶段 | 优先级 | 真实插件 | 核心优化与补全目标 | 人工操作入口 | 正式案例数 | 状态 |",
        "|---|---|---|---|---|---|---:|---|",
    ]
    for p in data["workOrders"]:
        visual = ("有诊断地图，仍待正式接线" if p["existingDiagnosticMap"]
                  else "正式案例待制作" if p["formalScenarioGap"]
                  else "已有登记案例待运行" if p["visualReviewRequired"]
                  else "按职责采用可操作诊断面板/原生测试")
        lines.append(
            f"| {p['workOrderId']} | {p['primaryBatch']} | {p['priorityFromAudit']} | "
            f"{p['pluginId']} | {p['firstStepFocus']} | {visual} | "
            f"{p['formalScenarioCount']} | 未实施/未签收 |")
    lines += [
        "",
        "## 所有工作单统一执行步骤",
        "",
        "1. **01 功能与边界审查**：读取实际公开接口、已存在消费者、资产引用、端侧与已有测试，"
        "先确认功能分母。现有插件描述不能代替实际实现。",
        "2. **02 最小代码/资源整改**：以实际复现的缺陷与测试为依据增量改代码；"
        "项目Widget Blueprint（控件蓝图）与布局必须使用Monolith，不能写文本假资产。",
        "3. **03 人工界面与案例接入**：复用唯一DemoHub（演示中心）和真实开发地图；"
        "参数、执行、停止、重置和返回必须连接真实插件能力，禁止空按钮与自动人工通过。",
        "4. **04 原生与发布证据**：按照当前UE5.8版本区分语法、UBT链接、UE Automation、"
        "Client/Server真实Cook、GPU/CPU实际测量，提供完整RunId与指纹。",
        "5. **05 人工审核与交付**：由既有GamePlatformDeveloperTools ReviewSubsystem（人工审核子系统）"
        "生成待审核报告；真人逐证据确认，旧版本的通过不得投影为当前通过。",
        "",
        "## 阻断与证据来源",
        "",
        "- 优先级来自当前静态审查：P1多为缺可操作演示，并不代表对应插件有已确定代码故障。",
        "- 编目及功能身份由真实源决定；本文件不在PluginDemoManifest（演示唯一作者源）写入未经确认的功能或案例。",
        "- 不新增OpenWorld假地图，不恢复旧插件，不在当前活动UE编辑器中竞争写蓝图、关卡或表现资源。",
        "- 本工作单状态固定为未实施；任何真实完成状态由实际代码/构建/运行/审核证据另行追加和验证。",
        "",
        "详见：PluginQualityOptimizationDesign_V2_20261018.md（总体优化设计）及"
        "PluginQualityOptimizationExecutionPlan_V2_20261018.md（M0—M7执行计划）。",
    ]
    return "\n".join(lines) + "\n"


def as_files() -> dict[Path, str]:
    matrix, raw = read_source()
    result = build_workorders(matrix, raw)
    return {
        OUTPUT_JSON: json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        OUTPUT_MD: to_markdown(result),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="SSLM全插件优化工作单，只读或生成文档")
    parser.add_argument("--write", action="store_true", help="仅生成Docs中的JSON及中文Markdown工作单")
    parser.add_argument("--check", action="store_true", help="只读核对两份派生工作单与当前台账一致")
    parser.add_argument("--summary", action="store_true", help="打印数量与批次摘要")
    args = parser.parse_args()
    if args.write and args.check:
        parser.error("--write与--check不能同时指定")
    files = as_files()
    if args.write:
        for path, contents in files.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(contents, encoding="utf-8")
    if args.check:
        errors = [str(path) for path, body in files.items()
                  if not path.exists() or path.read_text(encoding="utf-8") != body]
        if errors:
            print("工作单与真实插件质量台账不一致或文件缺失：" + ", ".join(errors), file=sys.stderr)
            return 1
    parsed = json.loads(files[OUTPUT_JSON])
    if args.summary or not (args.write or args.check):
        print(f"SSLM_PLUGIN_WORKORDERS_READY={parsed['totalPluginWorkOrders']} "
              f"CODE={parsed['codePluginWorkOrders']} CONTENT={parsed['contentPackWorkOrders']} "
              f"VISUAL={parsed['visualReviewRequired']} FORMAL_GAPS={parsed['formalScenarioGaps']} "
              f"HUMAN_APPROVED={parsed['humanApproved']}")
        print("PHASE_COUNTS=" + json.dumps(parsed["phaseCounts"], ensure_ascii=False, sort_keys=True))
        if args.check:
            print("WORKORDERS_MATCH_CURRENT_MATRIX=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
