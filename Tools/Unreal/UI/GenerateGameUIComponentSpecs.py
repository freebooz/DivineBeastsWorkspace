# -*- coding: utf-8 -*-
"""生成《神兽联盟》十大业务域和通用UI组件的Monolith声明式规格。

只写Saved目录的JSON，不创建.uasset/.umap或伪造Monolith编译记录。
C++父类在GamePlatformUI/GamePlatformArenaClient/DivineBeastsUIClient/
DivineBeastsArenaClient中拥有真实职责；Widget视觉归DBAUIPack_Core或DBAArena。
此工具可在编辑器断开时准备规格，但不能代替Monolith创建、编译、保存与回读。
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "Saved/Monolith/GameUIComponentSpecs"
OUTPUT.mkdir(parents=True, exist_ok=True)


def color(hex_rgb: str, opacity: int = 255) -> str:
    """与现有前端视觉脚本一致，将sRGB色转为Monolith接受的线性颜色。"""
    value = [int(hex_rgb[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    linear = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
              for v in value]
    return "#" + "".join(f"{round(v * 255):02X}" for v in linear) + f"{opacity:02X}"


GOLD = color("D8B77B")
TEXT = color("E7E1D6")
MUTED = color("A5AEAE")
CRIMSON = color("BD6D69")
GREEN = color("83CFA6")
BACKGROUND = color("10202C", 229)
PANEL = color("152531", 236)


def slot(anchor: str, x: float, y: float, w: float, h: float,
         align_x: float = 0, align_y: float = 0, z: int = 8):
    return {"anchorPreset": anchor, "position": {"x": x, "y": y},
            "size": {"x": w, "y": h},
            "alignment": {"x": align_x, "y": align_y}, "zOrder": z}


def widget(kind: str, identity: str, children=None, position=None,
           style=None, content=None):
    node = {"type": kind, "id": identity}
    for key, val in (("children", children), ("slot", position),
                     ("style", style), ("content", content)):
        if val is not None:
            node[key] = val
    return node


def label(identity: str, value: str, x: float, y: float,
          w: float = 260, h: float = 30, size: int = 16,
          anchor: str = "top_left", tint: str = TEXT,
          align_x: float = 0, align_y: float = 0):
    return widget("TextBlock", identity,
                  position=slot(anchor, x, y, w, h, align_x, align_y),
                  content={"text": value, "fontSize": size,
                           "fontColor": tint, "wrapMode": "Auto"})


def button(identity: str, text: str, x: float, y: float,
           w: float = 150, h: float = 40, anchor: str = "top_left",
           align_x: float = 0, align_y: float = 0):
    return widget("Button", identity,
                  [widget("TextBlock", identity + "Text",
                          content={"text": text, "fontSize": 15,
                                   "fontColor": TEXT})],
                  position=slot(anchor, x, y, w, h, align_x, align_y))


def panel(identity: str, x: float, y: float, w: float, h: float,
          anchor: str = "top_left", opacity: str = PANEL,
          align_x: float = 0, align_y: float = 0):
    return widget("Border", identity,
                  position=slot(anchor, x, y, w, h, align_x, align_y, 3),
                  style={"background": opacity,
                         "visibility": "HitTestInvisible"})


def box(kind: str, identity: str, x: float, y: float,
        w: float, h: float, anchor: str = "top_left",
        align_x: float = 0, align_y: float = 0, children=None):
    return widget(kind, identity, children or [],
                  position=slot(anchor, x, y, w, h, align_x, align_y))


def progress(identity: str, x: float, y: float, w: float, h: float,
             anchor: str = "top_left"):
    return widget("ProgressBar", identity,
                  position=slot(anchor, x, y, w, h),
                  content={"percent": 0})


def screen(name: str, parent: str, elements, description: str, directory: str,
           category: str, existing: bool = False):
    """构造符合现有build_ui_from_spec约定的资产声明，不写二进制资产。"""
    root = {"version": 1, "name": name,
            "parentClass": parent,
            "metadata": {"authoringTool": "Monolith MCP",
                         "category": category, "description": description,
                         "assetDirectory": directory,
                         "existingAssetMustBePreserved": existing,
                         "status": "仅声明式规格，未经Monolith生产及验证"},
            "rootWidget": widget(
                "CanvasPanel", "RootCanvas", elements,
                style={"visibility": "SelfHitTestInvisible"})}
    return root


COMMON = "/Script/GamePlatformUIClient."
PROJECT = "/Script/DivineBeastsUIClient."
ARENA = "/Script/DivineBeastsArenaClient."
PUB = "/DBAUIPack_Core/UI"
ARENA_ROOT = "/DBAArena/UI"

tasks = {}


def add(key: str, parent: str, directory: str, domain: str,
        note: str, elements, existing: bool = False):
    name = "WBP_DBA_UI_" + key
    tasks[key] = screen(name, parent, elements, note, directory, domain, existing)


# --- 跨游戏的视觉原子：仅使用GamePlatformUI现存或新增的中立C++基类。 ---
add("HealthBar", COMMON+"GamePlatformResourceBarWidget", PUB+"/Components",
    "Core/Combat", "生命/护盾/能量等通用资源条；项目样式为古金边框与柔和状态色；值由真实事件刷新。",
    [panel("Frame", 0, 0, 310, 40),
     progress("ProgressBar", 10, 12, 290, 16),
     label("ResourceValueText", "", 12, 3, 280, 20, 11, tint=TEXT)])

add("PlayerPortrait", COMMON+"GamePlatformPortraitWidget", PUB+"/Components",
    "Account/Combat/Social", "玩家/队友/英雄肖像，图源为软引用，空状态展示统一匿名占位符。",
    [panel("PortraitFrame", 0, 0, 106, 126),
     box("Image", "PortraitImage", 6, 6, 94, 94),
     label("PlayerName", "", 8, 103, 92, 20, 12, tint=GOLD)])

add("AbilitySlot", COMMON+"GamePlatformSlotWidget", PUB+"/Components",
    "Combat/Inventory", "技能物品槽位外观，内容由业务ViewModel真实推送，不预填技能。",
    [panel("SlotFrame", 0, 0, 64, 64),
     box("Image", "ItemIcon", 5, 5, 54, 54),
     label("ChargeCount", "", 34, 42, 26, 18, 11)])

add("SlotBar", COMMON+"GamePlatformSlotBarWidget", PUB+"/Components",
    "Combat/Inventory", "通用快捷栏与技能栏容器，32格上限，动态格子由Blueprint事件更新。",
    [panel("BarFrame", 0, 0, 530, 92),
     box("HorizontalBox", "SlotItems", 10, 10, 510, 72)])

add("Minimap", COMMON+"GamePlatformMinimapWidget", PUB+"/Components",
    "World/Arena", "圆形或方形小地图布局，真实世界到UV投影在业务适配器中完成。",
    [panel("MapFrame", 0, 0, 248, 268),
     box("Image", "MapImage", 8, 8, 232, 232),
     box("CanvasPanel", "MarkerLayer", 8, 8, 232, 232),
     label("CurrentRegion", "", 10, 242, 210, 20, 12, tint=GOLD)])

add("PartyRoster", COMMON+"GamePlatformPartyRosterWidget", PUB+"/Components",
    "Social/World/Arena", "队伍成员列表仅展示已确认队伍状态，头像、血条、在线和就绪标记由事件重绘。",
    [panel("RosterFrame", 0, 0, 266, 372),
     label("RosterHeading", "队伍成员", 15, 10, 225, 30, 19, tint=GOLD),
     box("ScrollBox", "RosterScroll", 8, 48, 250, 312,
         children=[widget("VerticalBox", "MemberRows")])])

add("Countdown", COMMON+"GamePlatformCountdownWidget", PUB+"/Components",
    "Core/Arena/Combat", "通用倒计时00:00与阶段提示；不由Widget自行推算服务器时间。",
    [panel("TimerFrame", 0, 0, 168, 72),
     label("CountdownText", "--:--", 14, 12, 142, 35, 30, tint=GOLD),
     label("CountdownStage", "", 10, 53, 148, 16, 11, tint=MUTED)])

add("StatusEffects", COMMON+"GamePlatformStatusEffectTrayWidget", PUB+"/Components",
    "Combat", "状态图标图层，通过来源快照动态显示Buff/Debuff与叠层。",
    [box("WrapBox", "EffectsGrid", 0, 0, 400, 66)])

add("QuestTracker", COMMON+"GamePlatformQuestTrackerWidget", PUB+"/Components",
    "World", "任务追踪栏只展示已经选中的公开目标和事件进度。",
    [panel("QuestFrame", 0, 0, 320, 290),
     label("QuestHeading", "任务追踪", 12, 12, 290, 32, 20, tint=GOLD),
     box("VerticalBox", "ObjectiveRows", 12, 54, 296, 220)])

add("InteractionPrompt", COMMON+"GamePlatformInteractionPromptWidget", PUB+"/Components",
    "World", "交互提示与快捷键图标区，输入设备切换后由适配器刷新。",
    [panel("PromptFrame", 0, 0, 280, 62),
     label("ActionLabel", "", 20, 14, 240, 34, 19, tint=TEXT)])

add("SettingRow", COMMON+"GamePlatformSettingRowWidget", PUB+"/Components",
    "System", "图形、音效、输入与无障碍设置项目，只投影业务服务当前值。",
    [panel("SettingFrame", 0, 0, 520, 54),
     label("SettingLabel", "", 14, 14, 220, 26),
     label("SettingValue", "", 248, 14, 160, 26, tint=GOLD),
     button("SettingEditButton", "调整", 426, 7, 82, 40)])

add("Tooltip", COMMON+"GamePlatformTooltipWidget", PUB+"/Components",
    "Core", "道具、技能、目标、地图标记公用悬浮提示，支持动态标题/描述。",
    [panel("TooltipFrame", 0, 0, 350, 180),
     box("Image", "TooltipIcon", 15, 15, 52, 52),
     label("TooltipTitle", "", 80, 20, 250, 30, 20, tint=GOLD),
     label("TooltipDescription", "", 15, 80, 320, 94, 14)])

# --- 十大业务域：公共页面/竞技页面均放在第三层，避免平台反向引用项目。 ---
add("Boot", PROJECT+"DivineBeastsBootScreen", PUB+"/Screens",
    "Core", "启动页不显示伪进度；应用流程准备结束后进入真实登录或选角。",
    [panel("BootBackdrop", 0, 0, 0, 0, "stretch_fill", opacity=BACKGROUND),
     label("GameBrand", "神兽联盟", 0, -54, 440, 70, 56,
           anchor="center", tint=GOLD, align_x=0.5, align_y=0.5),
     label("BootStage", "正在检查客户端资源…", 0, 62, 480, 30, 16,
           anchor="center", tint=MUTED, align_x=0.5, align_y=0.5)])

add("LoadingTravel", PROJECT+"DivineBeastsLoadingTravelScreen", PUB+"/Screens",
    "Core", "世界切服加载页展示真实LoadingService阶段，未知进度不显示数字百分比。",
    [panel("LoadingBackdrop", 0, 0, 0, 0, "stretch_fill", opacity=BACKGROUND),
     label("LoadingHeading", "即将进入神兽世界", 0, -20, 550, 55, 34,
           anchor="center", tint=GOLD, align_x=0.5, align_y=0.5),
     label("LoadingStage", "正在准备场景…", 0, 56, 550, 30, 16,
           anchor="center", tint=TEXT, align_x=0.5, align_y=0.5),
     progress("LoadProgress", 610, -84, 400, 12, "bottom_left")])

add("ErrorReconnect", PROJECT+"DivineBeastsErrorReconnectScreen", PUB+"/Screens",
    "Core", "错误/网络恢复通过现有Retry命令重试；ErrorText、RetryButton必须按父类命名契约。",
    [panel("ErrorBackdrop", 0, 0, 0, 0, "stretch_fill", opacity=BACKGROUND),
     panel("ErrorPanel", 0, 0, 480, 250, anchor="center", align_x=0.5, align_y=0.5),
     label("ErrorTitle", "连接出现问题", 0, -68, 430, 44, 28,
           anchor="center", tint=GOLD, align_x=0.5, align_y=0.5),
     label("ErrorText", "", 0, -4, 410, 72, 15, "center", TEXT, 0.5, 0.5),
     button("RetryButton", "重新连接", 0, 85, 200, 48, "center", 0.5, 0.5)])

add("Inventory", PROJECT+"DivineBeastsInventoryScreen", PUB+"/Screens",
    "Inventory", "装备与背包通用网格，真实物品来自GamePlatformInventoryClient缓存。",
    [panel("InventoryBackground", 0, 0, 760, 620, "center", align_x=0.5, align_y=0.5),
     label("InventoryTitle", "背 包", 0, -266, 630, 54, 32,
           anchor="center", tint=GOLD, align_x=0.5, align_y=0.5),
     button("InventoryGrid", "选择物品栏", 0, -178, 620, 58, "center", 0.5, 0.5),
     box("WrapBox", "InventorySlotGrid", 0, 38, 632, 392, "center", 0.5, 0.5)])

add("Quest", PROJECT+"DivineBeastsQuestScreen", PUB+"/Screens",
    "World", "任务详情页面复用GamePlatformQuestClient来源，不提交奖励真值。",
    [panel("QuestBackground", 0, 0, 740, 600, "center", align_x=0.5, align_y=0.5),
     label("QuestTitle", "任务日志", 0, -255, 660, 45, 30,
           anchor="center", tint=GOLD, align_x=0.5, align_y=0.5),
     button("QuestList", "任务目录", -190, -156, 260, 52, "center", 0.5, 0.5),
     box("ScrollBox", "QuestEntries", -190, 42, 260, 355, "center", 0.5, 0.5),
     label("QuestDetail", "", 160, -80, 360, 260, 15, "center",
           TEXT, 0.5, 0.5)])

add("OpenWorldHUD", PROJECT+"DivineBeastsOpenWorldHUD", PUB+"/HUD",
    "World/Combat", "开放世界信息层：玩家状态、任务、技能栏、小地图、组队状态的项目组合。",
    [panel("PlayerStatusFrame", 18, 18, 350, 116),
     label("PlayerStatusLabel", "英雄状态", 36, 28, 240, 26, 16, tint=GOLD),
     progress("HealthFill", 38, 70, 300, 13),
     panel("MapFrame", -18, 18, 245, 250, "top_right"),
     box("Image", "MinimapImage", -29, 30, 222, 222, "top_right"),
     label("QuestTrackerTitle", "任务追踪", -28, 295, 245, 30, 18,
           "top_right", GOLD),
     box("VerticalBox", "QuestEntries", -28, 335, 245, 260, "top_right"),
     panel("AbilityFrame", 0, -18, 612, 88, "bottom_center", align_x=0.5, align_y=1),
     box("HorizontalBox", "AbilitySlots", 0, -33, 582, 58,
         "bottom_center", 0.5, 1)])

add("VillageMainHUD", PROJECT+"DivineBeastsVillageHUD", PUB+"/HUD",
    "World", "新手村HUD突出教学引导、互动目标与基础角色状态。",
    [panel("PlayerStatusFrame", 18, 18, 350, 100),
     label("PlayerStatusLabel", "神兽见习者", 30, 25, 300, 25, 16, tint=GOLD),
     progress("HealthFill", 30, 70, 280, 14),
     panel("TutorialFrame", 0, 32, 470, 105, "top_center", align_x=0.5),
     label("TutorialText", "探索新手村", 0, 66, 430, 40, 20,
           "top_center", GOLD, 0.5),
     panel("AbilityFrame", 0, -17, 560, 86, "bottom_center",
           align_x=0.5, align_y=1),
     box("HorizontalBox", "AbilitySlots", 0, -30, 530, 56,
         "bottom_center", 0.5, 1)])

add("TutorialGuidance", PROJECT+"DivineBeastsTutorialHUD", PUB+"/HUD",
    "World", "教学阶段提示界面不自创关卡进度，由Village任务系统驱动。",
    [panel("GuidanceFrame", 0, 55, 540, 110, "top_center", align_x=0.5),
     label("TutorialStep", "教学", 0, 70, 500, 38, 25,
           "top_center", GOLD, 0.5),
     label("TutorialInstruction", "", 0, 120, 500, 34, 17,
           "top_center", TEXT, 0.5)])

add("TrainingControls", PROJECT+"DivineBeastsTrainingHUD", PUB+"/HUD",
    "World", "训练模式控制展示；所有重置意图仍经现有项目ViewModel提交。",
    [panel("TrainingFrame", 0, -30, 470, 108, "bottom_center", align_x=0.5, align_y=1),
     label("TrainingTitle", "训练场", 0, -112, 420, 30, 22,
           "bottom_center", GOLD, 0.5, 1),
     button("TrainingResetButton", "重置训练", -112, -56, 168, 42,
            "bottom_center", 0.5, 1)])

add("PlayerStatus", PROJECT+"DivineBeastsPlayerStatusPanel", PUB+"/Components",
    "Combat", "战斗状态条：生命/护盾/气势，只读数据由项目战斗ViewModel推送。",
    [panel("StatusBackdrop", 0, 0, 366, 148),
     label("StatusHeading", "生命 · 护盾 · 气势", 15, 10, 332, 24, 17, tint=GOLD),
     progress("HealthVisual", 15, 46, 336, 18),
     progress("ShieldVisual", 15, 82, 336, 14),
     progress("MomentumVisual", 15, 111, 336, 13)])

add("AbilityBar", PROJECT+"DivineBeastsAbilityBarPanel", PUB+"/Components",
    "Combat", "神兽联盟项目技能快捷栏，后台技能归Gameplay能力系统；可复用平台Slot部件。",
    [panel("AbilityBackdrop", 0, 0, 612, 108),
     label("AbilityTitle", "技能与快捷操作", 14, 8, 300, 27, 16, tint=GOLD),
     box("HorizontalBox", "AbilitySlotItems", 12, 41, 590, 58)])

add("Social", PROJECT+"DivineBeastsSocialScreenBase", PUB+"/Screens",
    "Social", "组队/好友项目社交入口，尚无真实后端快照时显示服务不可用。",
    [panel("SocialBackdrop", 0, 0, 850, 620, "center", align_x=0.5, align_y=0.5),
     label("SocialTitle", "社交与队伍", 0, -264, 700, 43, 30,
           "center", GOLD, 0.5, 0.5),
     button("FriendsButton", "好友", -275, -175, 190, 46,
            "center", 0.5, 0.5),
     button("PartyButton", "队伍", -66, -175, 190, 46,
            "center", 0.5, 0.5),
     box("ScrollBox", "FriendScroll", -180, 70, 390, 378,
         "center", 0.5, 0.5, children=[widget("VerticalBox", "FriendsList")]),
     label("ServiceStatus", "服务未连接", 230, -52, 290, 43, 16,
           "center", MUTED, 0.5, 0.5)])

add("SystemMenu", PROJECT+"DivineBeastsSystemScreenBase", PUB+"/Screens",
    "System", "系统设置入口复用平台无障碍/图形/声音/输入设置能力，不重复持久化状态。",
    [panel("SettingsBackdrop", 0, 0, 740, 640, "center", align_x=0.5, align_y=0.5),
     label("SettingsTitle", "系统设置", 0, -260, 650, 44, 30,
           "center", GOLD, 0.5, 0.5),
     button("VideoButton", "图像", -235, -172, 166, 44, "center", 0.5, 0.5),
     button("AudioButton", "声音", -50, -172, 166, 44, "center", 0.5, 0.5),
     button("ControlsButton", "操作", 135, -172, 166, 44, "center", 0.5, 0.5),
     box("VerticalBox", "SettingsItems", 0, 20, 635, 330,
         "center", 0.5, 0.5),
     button("ResumeButton", "返回游戏", 0, 264, 220, 48,
            "center", 0.5, 0.5)])

add("LiveOps", PROJECT+"DivineBeastsLiveOpsScreenBase", PUB+"/Screens",
    "LiveOps", "邮件/活动/公告项目UI，所有权益发放与账单由专属业务服务确认。",
    [panel("LiveOpsBackdrop", 0, 0, 840, 610, "center", align_x=0.5, align_y=0.5),
     label("LiveOpsTitle", "活动与公告", 0, -244, 730, 44, 30,
           "center", GOLD, 0.5, 0.5),
     button("NoticesButton", "公告", -225, -162, 180, 46, "center", 0.5, 0.5),
     button("EventsButton", "活动", -20, -162, 180, 46, "center", 0.5, 0.5),
     button("MailButton", "邮件", 185, -162, 180, 46, "center", 0.5, 0.5),
     box("ScrollBox", "NoticesScroll", 0, 82, 726, 326, "center", 0.5, 0.5,
         children=[widget("VerticalBox", "NoticesList")])])

# --- Arena必须从MOBA中层基类派生，但具体Widget由DBAArena项目插件持有。 ---
for key, parent, focus, title in [
    ("Matchmaking", "DivineBeastsMatchmakingScreen", "ModeList", "选择竞技模式"),
    ("MatchFoundReady", "DivineBeastsMatchFoundReadyScreen", "ReadyButton", "匹配成功"),
    ("HeroSelection", "DivineBeastsArenaHeroSelectionScreen", "HeroList", "选择竞技英雄"),
    ("Scoreboard", "DivineBeastsScoreboardScreen", "ScoreboardList", "竞技记分板"),
    ("PostMatch", "DivineBeastsPostMatchResultScreen", "ReturnWorldButton", "赛后结算")
]:
    elements = [
        panel("ArenaBackdrop", 0, 0, 930, 650, "center", align_x=0.5, align_y=0.5),
        label("ArenaTitle", title, 0, -272, 800, 55, 32,
              "center", GOLD, 0.5, 0.5),
        button(focus, {"ModeList":"竞技模式","ReadyButton":"准备就绪",
                        "HeroList":"英雄名单","ScoreboardList":"队伍成绩",
                        "ReturnWorldButton":"返回世界"}[focus],
               0, 170, 280, 50, "center", 0.5, 0.5),
        box("VerticalBox", "ArenaDataRows", 0, -24, 820, 335,
            "center", 0.5, 0.5)
    ]
    add("Arena_"+key, ARENA+parent, ARENA_ROOT+"/Screens",
        "Arena", "仅消费Arena ViewModel的可信复制事实；不计算比赛得分和胜负。", elements)

add("ArenaHUD", ARENA+"DivineBeastsArenaHUD", ARENA_ROOT+"/HUD",
    "Arena", "1v1至5v5竞技对局HUD；数据全部来自MOBA通用竞技ViewModel和战斗状态。",
    [panel("ArenaScoreFrame", 0, 16, 620, 96, "top_center", align_x=0.5),
     label("ArenaScore", "竞技状态", 0, 34, 590, 36, 22,
           "top_center", GOLD, 0.5),
     panel("ArenaTimerFrame", 0, 127, 184, 52, "top_center", align_x=0.5),
     label("ArenaTimer", "--:--", 0, 138, 150, 32, 24,
           "top_center", GOLD, 0.5),
     panel("ArenaAbilities", 0, -20, 590, 86, "bottom_center",
           align_x=0.5, align_y=1),
     box("HorizontalBox", "ArenaAbilitySlots", 0, -28, 560, 64,
         "bottom_center", 0.5, 1)])

# 已交付的真实登录和角色资源只引用，不再次生产覆盖。
existing_assets = [
    {"name":"WBP_DBA_UI_RootLayout", "path":PUB+"/Root/WBP_DBA_UI_RootLayout",
     "domain":"Core", "state":"existing_preserve"},
    {"name":"WBP_DBA_UI_Login", "path":PUB+"/Screens/WBP_DBA_UI_Login",
     "domain":"Account", "state":"existing_preserve"},
    {"name":"WBP_DBA_UI_CharacterCreate", "path":PUB+"/Screens/WBP_DBA_UI_CharacterCreate",
     "domain":"Character", "state":"existing_preserve"},
    {"name":"WBP_DBA_UI_CharacterSelect", "path":PUB+"/Screens/WBP_DBA_UI_CharacterSelect",
     "domain":"Character", "state":"existing_preserve"},
    {"name":"WBP_DBA_HeroChoice", "path":PUB+"/Components/WBP_DBA_HeroChoice",
     "domain":"Character", "state":"existing_preserve"},
    {"name":"WBP_DBA_CharacterChoice", "path":PUB+"/Components/WBP_DBA_CharacterChoice",
     "domain":"Character", "state":"existing_preserve"}
]

manifest = {"version": 1, "authoringMethod": "Monolith MCP Only",
            "status": "Specs prepared only; actual Widget Blueprints need Monolith/editor",
            "countToGenerate": len(tasks), "existingPreserve": existing_assets,
            "assets": []}

for identity, spec in tasks.items():
    path = OUTPUT / (identity + ".json")
    path.write_text(json.dumps(spec, ensure_ascii=False, indent=2), encoding="utf-8")
    manifest["assets"].append({
        "id": identity, "assetName": spec["name"],
        "mountPath": spec["metadata"]["assetDirectory"]+"/"+spec["name"],
        "parentClass": spec["parentClass"],
        "businessDomain": spec["metadata"]["category"],
        "spec": path.relative_to(ROOT).as_posix(),
        "state": "spec_only_not_blueprint"
    })

(OUTPUT / "Manifest.json").write_text(
    json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
print("GAME_UI_SPEC_COUNT", len(tasks))
print("GAME_UI_EXISTING_PRESERVE", len(existing_assets))
print("GAME_UI_SPEC_ROOT", OUTPUT)
