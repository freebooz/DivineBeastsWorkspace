# -*- coding: utf-8 -*-
"""SSLM 戌狗V6：使用已授权工程内1254高清源稿创作八张独立技能图标。
原始V1图像完全只读；绘制以物件为主且强化48像素可读性。master为重新构图的
1024像素派生高清母版，不谎称独立AI重新绘制。仅author模式写源PNG和证据；
与正式UE Texture2D导入、Profile、GAS或HUD验收是独立阶段。
"""
from pathlib import Path
import json, hashlib, argparse, sys, math, shutil
ROOT=Path(__file__).resolve().parents[3]
PACK=ROOT/"Game/Plugins/DivineBeasts/ContentPacks/Heroes/DBAHeroPack_Dog"
SRC=PACK/"SourceArt/UI/SkillIcons"
OUT=SRC/"V6"
FROZEN=PACK/"Docs/DogV6_ArtFreeze_20261019.json"
V1=["T_DBA_Dog_BasicAttack_BiteStrike_V1.png",
    "T_DBA_Dog_Active01_TrackingMark_V1.png",
    "T_DBA_Dog_Active02_GuardianLeap_V1.png",
    "T_DBA_Dog_Ultimate_CelestialHunt_V1.png",
    "T_DBA_Dog_Passive_GuardianInstinct_V1.png"]
sys.path.insert(0,str(ROOT/"Saved/Tools/SnowPyDeps"))
from PIL import Image,ImageDraw,ImageFilter,ImageEnhance,ImageOps,ImageStat

SIZE=1024
GOLD=(255,215,132,255)
PALE=(255,247,210,255)
JADE=(88,239,217,255)
TEAL=(21,151,161,255)
DARK=(2,17,28,255)
RED=(244,107,101,255)

def fail(msg): raise RuntimeError("戌狗V6绘制门禁："+str(msg))
def bezier(a,b,c,steps=90):
    return [(int((1-t)**2*a[0]+2*(1-t)*t*b[0]+t*t*c[0]),
             int((1-t)**2*a[1]+2*(1-t)*t*b[1]+t*t*c[1]))
            for t in (i/steps for i in range(steps+1))]
def drawpath(layer,pts,color,width,glow=0):
    if glow:
        fog=Image.new("RGBA",(SIZE,SIZE),(0,0,0,0))
        gd=ImageDraw.Draw(fog)
        gd.line(pts,fill=(*color[:3],120),width=width+int(glow*2),joint="curve")
        blur=fog.filter(ImageFilter.GaussianBlur(glow))
        layer.alpha_composite(blur)
    d=ImageDraw.Draw(layer)
    d.line(pts,fill=color,width=width,joint="curve")
def ellipse(layer,bbox,fill=None,outline=None,width=1,glow=0):
    if glow and outline:
        fog=Image.new("RGBA",(SIZE,SIZE),(0,0,0,0))
        ImageDraw.Draw(fog).ellipse(bbox,outline=(*outline[:3],125),width=width+14)
        layer.alpha_composite(fog.filter(ImageFilter.GaussianBlur(glow)))
    ImageDraw.Draw(layer).ellipse(bbox,fill=fill,outline=outline,width=width)
def draw_arrow(layer,x1,y1,x2,y2,c=PALE,thickness=19):
    drawpath(layer,[(x1,y1),(x2,y2)],c,thickness,10)
    a=math.atan2(y2-y1,x2-x1)
    d=80
    tips=[(int(x2-d*math.cos(a-0.6)),int(y2-d*math.sin(a-0.6))),
          (int(x2-d*math.cos(a+0.6)),int(y2-d*math.sin(a+0.6)))]
    drawpath(layer,[tips[0],(x2,y2),tips[1]],c,thickness,9)

def sword_claw(layer,x,y,direction=1,length=480):
    # 白金尖锐爪尖，外沿金白双层反光；上粗下细。
    tip=(x+direction*length,y-length//3)
    shape=[(x-80*direction,y+120),(x+55*direction,y-20),
           (tip[0],tip[1]),(x+70*direction,y+60),(x+25*direction,y+150)]
    glow=Image.new("RGBA",(SIZE,SIZE),(0,0,0,0))
    ImageDraw.Draw(glow).polygon(shape,fill=(102,245,227,130))
    layer.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))
    d=ImageDraw.Draw(layer)
    d.polygon(shape,fill=(230,253,244,232),outline=(255,217,130,255),width=10)
    drawpath(layer,[(x,y+66),tip],GOLD,13,6)

def shield(layer,cx=520,cy=520,r=290):
    # 可见黄金镶边的六面盾：独立主体，不复用犬头肖像。
    pts=[(cx,cy-r),(cx+int(r*.72),cy-int(r*.57)),(cx+int(r*.83),cy+int(r*.2)),
         (cx+int(r*.57),cy+int(r*.67)),(cx,cy+r),
         (cx-int(r*.57),cy+int(r*.67)),(cx-int(r*.83),cy+int(r*.2)),
         (cx-int(r*.72),cy-int(r*.57))]
    d=ImageDraw.Draw(layer)
    d.polygon(pts,fill=(9,65,71,185))
    d.line(pts+[pts[0]],fill=(68,244,212,225),width=37,joint="curve")
    d.line(pts+[pts[0]],fill=GOLD,width=19,joint="curve")
    inner=[(cx+int((x-cx)*.73),cy+int((y-cy)*.74)) for x,y in pts]
    d.line(inner+[inner[0]],fill=(248,250,223,225),width=9,joint="curve")
    drawpath(layer,bezier((cx-150,cy+40),(cx,cy-80),(cx+154,cy+40)),PALE,13,5)

def gate(layer,scale=1.0,cy=520):
    cx=515
    w=int(300*scale); top=int(cy-300*scale);bottom=int(cy+345*scale)
    left,right=cx-w,cx+w
    d=ImageDraw.Draw(layer)
    d.rounded_rectangle((left,top,right,bottom),radius=max(3,int(25*scale)),
                        fill=(4,35,48,205),outline=GOLD,width=max(3,int(28*scale)))
    d.rectangle((left+int(26*scale),top+int(60*scale),right-int(26*scale),bottom-int(27*scale)),
                fill=(14,82,77,170),outline=(102,243,204,230),width=max(2,int(13*scale)))
    top_points=[(left-int(95*scale),top+int(100*scale)),(cx,top-int(90*scale)),
                (right+int(95*scale),top+int(100*scale)),(right+int(20*scale),top+int(55*scale)),
                (cx,top-int(12*scale)),(left-int(20*scale),top+int(55*scale))]
    d.polygon(top_points,fill=(94,66,23,235),outline=PALE,width=max(3,int(12*scale)))
    d.line([(cx,top+int(65*scale)),(cx,bottom-int(90*scale))],fill=GOLD,width=max(3,int(11*scale)))
    ellipse(layer,(cx-int(70*scale),cy-int(70*scale),cx+int(70*scale),cy+int(70*scale)),
            fill=(15,101,89,235),outline=GOLD,width=max(3,int(13*scale)),glow=10)
    # 中央犬爪抽象徽纹，而非二次绘制动物头像。
    for xx,yy,rr in [(-30,-22,14),(0,-37,14),(30,-22,14),(0,16,35)]:
        ellipse(layer,(cx+int((xx-rr)*scale),cy+int((yy-rr)*scale),
                       cx+int((xx+rr)*scale),cy+int((yy+rr)*scale)),
                fill=PALE)

def oath(layer):
    # 由两个互不重叠的法印与单链连接，对准真实48图功能。
    for x in (282,742):
        ellipse(layer,(x-158,340,x+158,656),fill=(10,65,69,170),outline=GOLD,width=27,glow=16)
        ellipse(layer,(x-116,382,x+116,614),outline=JADE,width=12)
        drawpath(layer,[(x-60,503),(x,442),(x+60,503),(x,574),(x-60,503)],
                 PALE,20,10)
    drawpath(layer,bezier((420,496),(515,400),(605,496)),GOLD,31,14)
    drawpath(layer,bezier((420,533),(515,644),(605,533)),PALE,15,8)
    for x,y in [(470,475),(558,472),(476,560),(555,568)]:
        ellipse(layer,(x-15,y-15,x+15,y+15),fill=JADE)

def base_image(src_name):
    f=SRC/src_name
    if not f.is_file(): fail("缺少真实V1源稿："+str(f))
    with Image.open(f) as im:
        if min(im.size)<1024: fail("原始素材低于1024："+str(f))
        master=im.convert("RGB").resize((SIZE,SIZE),Image.Resampling.LANCZOS)
    # 温暖玄青环境光，保留源稿1254纹理细节。
    master=ImageEnhance.Contrast(master).enhance(1.09)
    master=ImageEnhance.Color(master).enhance(.90)
    layer=master.convert("RGBA")
    layer.alpha_composite(Image.new("RGBA",(SIZE,SIZE),(5,22,29,35)))
    return layer

def finish(layer,idx):
    # 克制的边缘暗化不牺牲中心主体细节。
    mask=Image.radial_gradient("L").resize((SIZE,SIZE))
    mask=mask.point(lambda x:min(115,int(x*.53)))
    dark=Image.new("RGBA",(SIZE,SIZE),(0,9,17,255))
    layer=Image.composite(dark,layer,mask).convert("RGB")
    return layer

def create(i):
    if i==0:
        a=base_image(V1[0])
        drawpath(a,bezier((190,830),(480,1010),(875,650)),GOLD,16,5)
    elif i==1:
        a=ImageOps.mirror(base_image(V1[0]))
        drawpath(a,bezier((825,813),(530,325),(186,378)),GOLD,32,16)
        draw_arrow(a,830,290,540,410,RED,24)
    elif i==2:
        a=base_image(V1[1])
        # 两组向右的金玉斩击，保留paw作为技能归属线索。
        sword_claw(a,300,580,1,385)
        sword_claw(a,420,780,1,365)
    elif i==3:
        a=base_image(V1[3])
        a=Image.blend(a,Image.new("RGBA",(SIZE,SIZE),(3,21,28,255)),.46)
        gate(a,scale=1.1,cy=522)
        drawpath(a,bezier((123,901),(500,990),(877,901)),JADE,28,16)
    elif i==4:
        a=base_image(V1[2])
        drawpath(a,bezier((120,865),(250,955),(782,768)),GOLD,28,18)
        drawpath(a,bezier((290,974),(460,899),(820,715)),PALE,12,10)
    elif i==5:
        a=base_image(V1[4])
        a=Image.blend(a,Image.new("RGBA",(SIZE,SIZE),(2,24,35,255)),.27)
        shield(a,cx=530,cy=535,r=298)
        drawpath(a,bezier((138,880),(500,965),(880,854)),GOLD,17,9)
    elif i==6:
        a=base_image(V1[1])
        a=Image.blend(a,Image.new("RGBA",(SIZE,SIZE),(3,25,34,255)),.42)
        gate(a,scale=.72,cy=510)
        draw_arrow(a,920,590,735,560,RED,34)
        drawpath(a,bezier((718,410),(680,530),(715,720)),JADE,28,12)
    else:
        a=base_image(V1[4])
        a=Image.blend(a,Image.new("RGBA",(SIZE,SIZE),(2,21,28,255)),.54)
        oath(a)
    return finish(a,i)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode",choices=["inspect","author"],default="inspect")
    args=parser.parse_args()
    data=json.loads(FROZEN.read_text(encoding="utf-8"))
    rows=data["skills"]
    if len(rows)!=8 or [r["slotId"] for r in rows]!=[
        "Primary","Slot1","Slot2","Slot4","Slot3","Slot5","Slot6","Slot7"]:
        fail("技能身份未冻结或位置错误")
    src_hashes={n:hashlib.sha256((SRC/n).read_bytes()).hexdigest() for n in V1}
    specs=[]
    for i,r in enumerate(rows):
        asset=r["textureAssetName"]
        specs.append([(OUT/"Masters"/(asset+"_Master_1024.png"),1024),
                      (OUT/"64"/(asset+"_64x64.png"),64),
                      (OUT/"48"/(asset+".png"),48)])
    if args.mode=="inspect":
        print(json.dumps({"mode":"inspect","sources":["V1:"+x for x in V1],
                          "sourceSha256":src_hashes,
                          "artCount":len(rows),"outputs":[str(a) for s in specs for a,_ in s],
                          "nextAction":"author"},ensure_ascii=False))
        return
    # 预检不允许覆盖其他任务已存在的V6素材。
    for group in specs:
        for path,_ in group:
            if path.exists(): fail("V6目标已存在，拒绝覆盖："+str(path))
    assets=[]
    for i,(skill,targets) in enumerate(zip(rows,specs)):
        main_image=create(i)
        saved={}
        for path,size in targets:
            path.parent.mkdir(parents=True,exist_ok=True)
            image=main_image if size==1024 else main_image.resize((size,size),Image.Resampling.LANCZOS)
            # RGB而非索引色，sRGB导入避免透明像素色边。
            image.save(path,format="PNG",optimize=True)
            saved[str(size)]={"path":str(path.relative_to(ROOT)).replace("\\","/"),
                              "sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                              "width":size,"height":size}
        assets.append({"index":i+1,"slot":skill["slotId"],"abilityId":skill["abilityId"],
                       "v6NameZh":skill["v6NameZh"],"v5NameZh":skill["productionV5NameZh"],
                       "textureAssetName":skill["textureAssetName"],
                       "texturePath":skill["texturePath"],"files":saved,
                       "artSource":"RECOMPOSED_FROM_EXISTING_V1_1254",
                       "humanArtworkApproved":False})
    # 保护源稿，重核SHA确保没有修改。
    for n,h in src_hashes.items():
        if hashlib.sha256((SRC/n).read_bytes()).hexdigest()!=h:
            fail("创作过程中V1保护源稿发生变化："+n)
    # 64和48两排无文字原尺寸审核图，必须保留实际像素。
    board=Image.new("RGB",(8*132,238),(9,25,34))
    draw=ImageDraw.Draw(board)
    for i,a in enumerate(assets):
        b=SRC/"V6/64"/(a["textureAssetName"]+"_64x64.png")
        c=SRC/"V6/48"/(a["textureAssetName"]+".png")
        board.paste(Image.open(b).convert("RGB"),(i*132+32,35))
        board.paste(Image.open(c).convert("RGB"),(i*132+40,137))
    qa=ROOT/"Saved/Validation/DogV6Art20261020"
    qa.mkdir(parents=True,exist_ok=True)
    board.save(qa/"DogV6_RealPixelSizeAudit.png")
    report={"heroId":"Hero.Zodiac.Dog","artCount":8,
            "sourceArtNativeSize":[1254,1254],"actualMasterSize":[1024,1024],
            "masterProvenance":"V1_NATIVE_1254_RECOMPOSED_NOT_INDEPENDENT_AI_GENERATION",
            "review64Count":8,"runtime48Count":8,
            "uiProfileUpdated":False,"unrealTexture2DImported":False,"humanArtApproved":False,
            "v1SourceSHA256":src_hashes,"assets":assets}
    (PACK/"Docs/DogV6_DerivedArtManifest_20261020.json").write_text(
      json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"artwork":"SAVED","master":8,"review64":8,"runtime48":8,
                      "sourceUnchanged":True,"manifest":str(PACK/"Docs/DogV6_DerivedArtManifest_20261020.json"),
                      "humanApproval":"PENDING","nativeUE":"NOT_IMPORTED"},ensure_ascii=False))

if __name__=="__main__":
    main()
