# -*- coding: utf-8 -*-
"""生成东方神话前端的Monolith声明式布局文档，不创建或伪造任何UE二进制资产。
公共UI视觉归DBAUIPack_Core，命令及生命周期归DBAClient；输出在Saved供Monolith消费和审核。
背景源图独立登记，输入／按钮为固定逻辑像素；只有背景随视口铺满，不缩放输入控件。
"""
import json
from pathlib import Path

OUTPUT = Path(__file__).resolve().parents[3] / "Saved/Monolith/FrontEndMythicSpecs"

def color(rgb, alpha=255):
    """期望sRGB转换为工具接收的线性HEX，避免深色变灰蓝、金色变白。"""
    values = [int(rgb[i:i+2], 16)/255 for i in (0,2,4)]
    linear = [v/12.92 if v <= 0.04045 else ((v+0.055)/1.055)**2.4 for v in values]
    return "#"+"".join(f"{round(v*255):02X}" for v in linear)+f"{alpha:02X}"

GOLD, WHITE, MUTED = color("D1AE70"), color("EEE8DC"), color("98A4AF")

def node(kind, identity, children=None, slot=None, style=None, content=None):
    """节点身份使用英文；节点不保存业务权威、密码或会话数据。"""
    value = {"type":kind, "id":identity}
    for key,item in (("children",children),("slot",slot),("style",style),("content",content)):
        if item is not None: value[key]=item
    return value

def place(anchor,x,y,w,h,ax=0,ay=0,z=10):
    """固定控件锚点与尺寸，独立于视口的背景缩放。"""
    return {"anchorPreset":anchor,"position":{"x":x,"y":y},"size":{"x":w,"y":h},"alignment":{"x":ax,"y":ay},"zOrder":z}

def text(identity,value,size=16,tint=WHITE,slot=None):
    """中文游戏文案与项目字体；空错误文案由只读快照事件填入。"""
    return node("TextBlock",identity,slot=slot,content={"text":value,"fontSize":size,"fontColor":tint,"wrapMode":"Auto"})

def panel(identity,slot,tint=None):
    return node("Border",identity,slot=slot,style={"background":tint or color("0B121A",235),"visibility":"HitTestInvisible"})

def button(identity,label,slot):
    return node("Button",identity,[text(identity+"Label",label,16,WHITE,{"hAlign":"Center","vAlign":"Center"})],slot=slot)

def document(name,parent,children):
    """保留真实原生父类身份，根布局不自行决定登录／角色流程。"""
    return {"version":1,"name":name,"parentClass":"/Script/DivineBeastsUIClient."+parent,
        "metadata":{"authoringTool":"Monolith MCP","description":"东方神话前端；空白登录输入、现代MMO角色布局、固定控件与事件驱动。"},
        "rootWidget":node("CanvasPanel","RootCanvas",children,style={"visibility":"SelfHitTestInvisible"})}

def login():
    """首屏为用户登录，原生命名控件合同与敏感输入清理逻辑保持不变。"""
    children=[node("Image","MythicBackground",slot=place("stretch_fill",0,0,0,0,z=0),style={"visibility":"HitTestInvisible"},
        content={"brushPath":"/DBAUIPack_Core/UI/Textures/T_DBA_MythicLogin"}),
        text("BrandTitle","神兽联盟",44,GOLD,place("top_left",42,38,400,64)),
        text("BrandSubtitle","DIVINE BEASTS",13,MUTED,place("top_left",46,108,380,28)),
        text("WorldTagline","山海为界 · 万灵共生",19,WHITE,place("bottom_left",44,-64,440,36,0,1)),
        text("WorldCaption","开启属于你的神话旅程",12,MUTED,place("bottom_left",46,-30,440,25,0,1)),
        panel("LoginShadow",place("center_right",-40,5,354,418,1,0.5),color("000000",170)),
        panel("LoginPanel",place("center_right",-48,0,344,410,1,0.5)),
        panel("LoginTopRule",place("center_right",-48,-205,344,2,1,0),GOLD),
        text("LoginHeading","欢迎归来",27,WHITE,place("center_right",-78,-172,284,45,1,0)),
        text("LoginSubtitle","登录你的神兽联盟账号",13,MUTED,place("center_right",-78,-119,284,28,1,0)),
        text("AccountLabel","账号",13,GOLD,place("center_right",-78,-72,284,24,1,0)),
        node("EditableTextBox","AccountInput",slot=place("center_right",-78,-43,284,42,1,0),content={"placeholder":"请输入账号","fontSize":16,"fontColor":WHITE}),
        text("PasswordLabel","密码",13,GOLD,place("center_right",-78,21,284,24,1,0)),
        node("EditableTextBox","PasswordInput",slot=place("center_right",-78,49,284,42,1,0),content={"placeholder":"请输入密码","fontSize":16,"fontColor":WHITE}),
        button("LoginButton","登  录",place("center_right",-78,117,284,44,1,0)),
        text("LoginHint","登录后选择角色，开启新手村冒险",12,MUTED,place("center_right",-78,174,284,26,1,0)),
        text("BusyIndicator","正在登录…",12,GOLD,place("center_right",-78,98,284,20,1,0)),
        text("MaintenanceText","服务器维护中，请稍后再试",13,GOLD,place("center_right",-78,-103,284,30,1,0)),
        text("ErrorText","",13,color("E69586"),place("center_right",-78,199,284,44,1,0))]
    return document("WBP_DBA_UI_Login","DivineBeastsLoginScreen",children)

def character(create):
    """透明中央预览区域接受鼠标拖动；面板和按钮位于更高层，保证正常选择与输入。"""
    children=[text("BrandTitle","神兽联盟",26,GOLD,place("top_left",28,20,270,40)),
        text("BrandSubtitle","DIVINE BEASTS",10,MUTED,place("top_left",30,60,250,20)),
        text("PageTitle","创建英雄" if create else "选择角色",22,WHITE,place("top_right",-28,28,252,36,1,0)),
        panel("TopRule",place("stretch_top",24,82,24,1,z=5),color("927953",170)),
        node("Border","PreviewDragSurface",slot=place("top_center",0,90,400,296,0.5,0,z=1),style={"background":"#00000000","visibility":"Visible"}),
        text("PreviewHelp","按住鼠标左键拖动，旋转角色",11,MUTED,place("bottom_center",0,-88,290,20,0.5,1)),
        text("SelectedHeroName","",22,GOLD,place("bottom_center",0,-154,290,34,0.5,1)),
        button("RotateLeftButton","◀",place("bottom_center",-48,-112,36,28,0.5,1)),
        button("RotateRightButton","▶",place("bottom_center",48,-112,36,28,0.5,1)),
        button("LogoutButton","返回登录",place("bottom_left",28,-28,128,34,0,1)),
        text("StatusText","",12,MUTED,place("bottom_right",-28,-78,252,40,1,1)),
        text("ErrorText","",12,color("E69586"),place("bottom_right",-28,-20,252,42,1,1))]
    if create:
        children += [panel("HeroPanel",place("top_left",24,96,268,322)),
            text("HeroHeading","十二生肖",18,GOLD,place("top_left",40,108,240,28)),
            text("HeroInstruction","选择守护你的神兽",12,MUTED,place("top_left",40,142,240,22)),
            node("WrapBox","HeroChoices",slot=place("top_left",36,172,244,246)),
            node("ComboBoxString","HeroOptions",slot=place("top_left",40,172,236,36)),
            panel("CreationInfoPanel",place("top_right",-24,96,200,216,1,0)),
            text("CreationInfoTitle","新的旅程",18,GOLD,place("top_right",-40,114,168,30,1,0)),
            text("CreationInfoBody","选择英雄与角色名称，\n从新手村开始冒险。",14,MUTED,place("top_right",-40,164,168,105,1,0)),
            node("EditableTextBox","CharacterNameInput",slot=place("bottom_center",-83,-28,220,38,0.5,1),content={"placeholder":"角色名称","fontSize":16,"fontColor":WHITE}),
            button("CreateButton","创建角色",place("bottom_center",155,-28,130,38,0.5,1))]
    else:
        children += [panel("RosterPanel",place("top_right",-24,96,284,322,1,0)),
            text("RosterHeading","你的角色",18,GOLD,place("top_right",-40,111,252,32,1,0)),
            node("ScrollBox","CharacterScroll",[node("VerticalBox","CharacterChoices")],slot=place("top_right",-40,157,252,246,1,0)),
            node("ComboBoxString","CharacterList",slot=place("top_right",-40,157,252,36,1,0)),
            text("SelectedCharacterName","",26,WHITE,place("bottom_center",0,-194,290,38,0.5,1)),
            button("SelectButton","进入游戏",place("bottom_center",0,-26,200,42,0.5,1))]
    suffix="CharacterCreate" if create else "CharacterSelect"
    return document("WBP_DBA_UI_"+suffix,"DivineBeasts"+suffix+"Screen",children)

OUTPUT.mkdir(parents=True,exist_ok=True)
for name,spec in {"Login":login(),"CharacterCreate":character(True),"CharacterSelect":character(False)}.items():
    (OUTPUT/(name+".json")).write_text(json.dumps(spec,ensure_ascii=False,indent=2),encoding="utf-8")
    print(name)
