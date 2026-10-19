"""已导入英雄技能图标64/48技术验收及只读HUD规格生成。

标准Python在工作空间运行；只读取清单/源PNG，写Saved技术证据和Monolith UISpec。
对源图SHA、真实尺寸、重复与近似情况做客观检查，不修改原稿、不生产uasset、
不将技术统计当人工美术签收。HUD规格只显示元数据，没有按钮、输入或技能授予。
"""
from pathlib import Path
import argparse
import hashlib
import json
from PIL import Image,ImageDraw,ImageFont,ImageStat

ROOT=Path(__file__).resolve().parents[3]

def position(x,y,width,height):
    """固定逻辑像素布局；不添加整页缩放补偿字号或尺寸。"""
    return {'anchorPreset':'top_left','position':{'x':x,'y':y},'size':{'x':width,'y':height}}

def node(kind,name,x,y,width,height,content=None,style=None):
    """仅声明视觉控件，真实WidgetTree由Monolith事务建造。"""
    value={'type':kind,'id':name,'slot':position(x,y,width,height)}
    if content is not None:value['content']=content
    if style is not None:value['style']=style
    return value

def difference_hash(image):
    """dHash仅检测需要人工再看的近似图，不定义未经批准的艺术合格阈值。"""
    data=list(image.convert('L').resize((9,8),Image.Resampling.LANCZOS).get_flattened_data())
    return sum(int(data[y*9+x]>data[y*9+x+1])<<(y*8+x) for y in range(8) for x in range(8))

def main():
    """完成清单内全部PNG门禁后保存原尺寸审查图和无交互的HUD预览规格。"""
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--plan',type=Path,required=True)
    args=parser.parse_args()
    plan=json.loads(args.plan.read_text(encoding='utf-8'))
    output=args.plan.parent
    fonts=Path('C:/Windows/Fonts/msyh.ttc')
    font=ImageFont.truetype(str(fonts),16)
    reports=[]
    for hero in plan['heroes']:
        sheet=Image.new('RGB',(1064,274),(12,23,32))
        draw=ImageDraw.Draw(sheet)
        draw.text((16,8),hero['heroId']+'  64×64 / 48×48；仅美术预览、不可施放',font=font,fill=(235,229,208))
        hashes={48:[],64:[]}
        similarity=[]
        stats=[]
        children=[node('Border','ReviewBackground',0,0,1064,274,style={'background':'#0C1720F2','visibility':'HitTestInvisible'}),
            node('TextBlock','ReviewHeading',16,8,1020,32,content={'text':hero['heroId']+'  仅美术预览、不可施放','fontSize':18,'fontColor':'#EAE3CFFF'}),
            node('TextBlock','ReviewSize64',16,44,1020,22,content={'text':'64×64显示（运行纹理48像素；双线性放大）','fontSize':16,'fontColor':'#C5C8C3FF'}),
            node('TextBlock','ReviewSize48',16,142,1020,22,content={'text':'48×48显示（运行纹理原生尺寸）','fontSize':16,'fontColor':'#C5C8C3FF'})]
        for index,row in enumerate(hero['entries']):
            for size,top in [(64,68),(48,168)]:
                path=(ROOT/row['png'+str(size)]).resolve()
                if not path.is_relative_to(ROOT):raise RuntimeError('图标源越界')
                digest=hashlib.sha256(path.read_bytes()).hexdigest()
                if digest!=row['sha'+str(size)]:raise RuntimeError('源SHA不符：'+str(path))
                with Image.open(path) as image:
                    image.load()
                    if image.size!=(size,size):raise RuntimeError('源PNG真实尺寸不符')
                    rgb=image.convert('RGB')
                    sheet.paste(rgb,(16+index*130,top))
                    luma=rgb.convert('L')
                    stats.append({'order':row['order'],'size':size,'sha256':digest,'lumaStdDev':ImageStat.Stat(luma).stddev[0]})
                    if size==48:similarity.append(difference_hash(rgb))
                hashes[size].append(digest)
                texture=row['texturePath']+'.'+row['texturePath'].rsplit('/',1)[-1]
                children.append(node('Image','Icon%d_%d'%(size,index+1),16+index*130,top,size,size,
                    content={'brushPath':texture},style={'visibility':'HitTestInvisible'}))
            draw.text((16+index*130,220),row['displayNameZh'],font=font,fill=(235,229,208))
            children.append(node('TextBlock','SkillName'+str(index+1),16+index*130,220,126,25,
                content={'text':row['displayNameZh'],'fontSize':16,'fontColor':'#EAE3CFFF'}))
        if any(len(set(values))!=len(values) for values in hashes.values()):raise RuntimeError('同英雄存在字节完全相同的技能图')
        pairs=[{'first':i+1,'second':j+1,'dHashDistance':(similarity[i]^similarity[j]).bit_count()}
            for i in range(8) for j in range(i+1,8) if (similarity[i]^similarity[j]).bit_count()<=6]
        sheet.save(output/(hero['hero']+'_Source64_48.png'))
        spec={'version':1,'name':'WBP_DBA_'+hero['hero']+'_SkillIconReviewV6','parentClass':'/Script/GamePlatformUIClient.GamePlatformComponentWidget',
            'metadata':{'purposeZh':'用户授权的只读技能图标审查；不接入施法输入','authoringTool':'Monolith MCP'},
            'rootWidget':{'type':'CanvasPanel','id':'RootCanvas','style':{'visibility':'SelfHitTestInvisible'},'children':children}}
        (output/(hero['hero']+'_ReviewSpec.json')).write_text(json.dumps(spec,ensure_ascii=False,indent=2),encoding='utf-8')
        reports.append({'hero':hero['hero'],'png64':8,'png48':8,'technicalGate':'PASS','nearPairsForHumanReview':pairs,'stats':stats,'humanArtApproved':False})
    # 单英雄导入与多英雄批次共用工具，计数来自实际检查，不能沿用首轮四英雄的常量。
    checked=sum(len(report['stats']) for report in reports)
    (output/'SmallSizeAudit.json').write_text(json.dumps({'heroes':reports,'sourceFilesChecked':checked,'humanArtApproved':False},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'heroes':len(reports),'sourceFilesChecked':checked,'technicalGate':'PASS','nearPairs':{r['hero']:r['nearPairsForHumanReview'] for r in reports}},ensure_ascii=False))

if __name__=='__main__':main()
