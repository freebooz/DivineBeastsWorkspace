# -*- coding: utf-8 -*-
"""校验本轮Monolith初始化审查与真实PIE证据，不写UE资源或修改工具原始提示。

仅验证已定位的8系统19项和地面冰箭；首次全量采样加修复后两系统采样合并。
尺寸写入、实际非零粒子、Ribbon跨度、渲染像素和冰箭水平高度缺一即失败。
输出Saved JSON及人工可读联系图；不替代正式客户端联机或人工美术验收。
"""
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw

ROOT=Path(__file__).resolve().parents[4]
DIRECTORY=ROOT/'Saved/Validation/VFXInitialization20261016'


def main():
    """保留原始告警；只有实际证据齐全才将某项记录为已核实的名称启发式误报。"""
    audit=json.loads((DIRECTORY/'InitializationInputs.json').read_text(encoding='utf8'))
    initial=json.loads((DIRECTORY/'CaptureInitial.json').read_text(encoding='utf8'))
    latest=json.loads((DIRECTORY/'Capture.json').read_text(encoding='utf8'))
    assert initial['status']==latest['status']=='captured'
    cases={v['system']:v for v in initial['cases']};cases.update({v['system']:v for v in latest['cases']})
    assert len(cases)==9 and len(audit['emitters'])==19
    baseline=Image.open(DIRECTORY/'Frames/Baseline.png').convert('RGB')
    sheet=Image.new('RGB',(960,3*206),(12,18,25));draw=ImageDraw.Draw(sheet)
    pixels={}
    for index,(path,case) in enumerate(cases.items()):
        frames=[]
        for frame in case['frames']:
            assert frame['simCacheValid'],path
            picture=Image.open(frame['image']).convert('RGB')
            difference=ImageChops.difference(picture,baseline)
            count=sum(max(p)>12 for p in difference.getdata());frames.append((count,picture))
            frame['changedPixels']=count
        count,picture=max(frames,key=lambda v:v[0]);assert count>0,path
        pixels[path]=count;x=(index%3)*320;y=(index//3)*206
        sheet.paste(picture.resize((320,180)),(x,y))
        draw.text((x+3,y+181),path.rsplit('/',1)[-1].replace('NS_','')[:37],fill='white')
    records=[]
    for record in audit['emitters']:
        assert record['hasSpawnWrite'] and record['boundAttribute']==record['attribute'],record['emitter']
        samples=[f['emitters'].get(record['emitter'],{}) for f in cases[record['system']]['frames']]
        intentional=record.get('explicitNoSpawn',False)
        if intentional:
            assert all(s.get('particles',0)==0 for s in samples),record['emitter']
        else:
            assert any(s.get('positiveVisibleDimensions',0)>0 for s in samples),record['emitter']
            if record['attribute']=='RibbonWidth':assert any(s.get('ribbonSpanCm',0)>1 for s in samples),record['emitter']
        records.append(dict(system=record['system'],emitter=record['emitter'],spawnWriteVerified=True,
                            classification='AuthoredZeroSpawn' if intentional else 'VisibleRendererVerified'))
    ice=next(v for k,v in cases.items() if k.endswith('/NS_DBA_IceArrow_Cast'))
    for frame in ice['frames']:
        assert set(frame['emitters'])=={'GroundOuter','GroundInner'}
        for data in frame['emitters'].values():
            assert data['particles']>0 and all(abs(z)<=8.01 for z in data['groundZ'])
            assert all(t<0.001 for t in data['verticalTilt'])
    assert ice['frames'][-1]['ageSeconds']>=2.0
    report=dict(passed=True,rawMonolithWarnings=audit['rawWarningCount'],verifiedItems=records,
                unresolvedInitializationItems=0,visibleSystems=len(cases),maximumChangedPixels=pixels,
                iceGroundOnlyAtTwoSeconds=True,manualGameplayAccepted=False)
    (DIRECTORY/'CaptureFinal.json').write_text(json.dumps(dict(status='captured',cases=list(cases.values())),indent=2),encoding='utf8')
    (DIRECTORY/'Verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    sheet.save(DIRECTORY/'ContactSheet.png')
    print('RENDERER_VERIFIED',len(records),'rawWarnings',audit['rawWarningCount'],'systems',len(cases))


if __name__=='__main__':main()
