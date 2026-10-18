#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""核对原生序列并发布无改色/无锐化的审核媒体与资产证据清单。"""
from pathlib import Path
import argparse,hashlib,json,shutil
from PIL import Image

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV2'


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--run',required=True);args=parser.parse_args()
    run=(OUT/'RenderRuns'/args.run).resolve();assert run.parent==(OUT/'RenderRuns').resolve()
    result=json.loads((run/'Result.json').read_text(encoding='utf8'));assert result['status']=='captured' and len(result['frames'])==31
    frames=[Image.open(Path(row['image'])).convert('RGB') for row in result['frames']]
    assert all(f.size==(1440,900) for f in frames)
    durations=[33,33,34]*10+[33]
    frames[0].save(OUT/'AttackSlashV2_Native.webp',save_all=True,append_images=frames[1:],duration=durations,loop=0,lossless=True,quality=100,method=4)
    shutil.copyfile(Path(result['frames'][10]['image']),OUT/'AttackSlashV2_Native.png')
    # 编码器不改原始逐帧PNG；WebP必须逐帧保持原生RGB像素。
    # WebP会合并连续完全相同的末尾空场帧；按RGB和时长验证整段时间线。
    runs=[]
    for source,duration in zip(frames,durations):
        pixels=source.tobytes()
        if runs and runs[-1][0]==pixels:runs[-1][1]+=duration
        else:runs.append([pixels,duration])
    webp=Image.open(OUT/'AttackSlashV2_Native.webp');assert webp.n_frames==len(runs)
    for i,(pixels,duration) in enumerate(runs):
        webp.seek(i);decoded=webp.convert('RGB')
        assert decoded.tobytes()==pixels,'动画无损RGB校验失败：'+str(i)
        assert webp.info['duration']==duration,'动画时间线校验失败：'+str(i)
    cold=json.loads((OUT/'ColdValidation.json').read_text(encoding='utf8'));assert cold['passed']
    lifecycle=json.loads((OUT/'Lifecycle.json').read_text(encoding='utf8'));assert lifecycle['status']=='passed'
    source_root=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V2/AttackSlash'
    content=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/CommonSkillVFX/Review/V2/AttackSlash'
    assets=[dict(file=str(p.relative_to(ROOT)).replace('\\','/'),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(content.rglob('*')) if p.is_file() and p.suffix in ('.uasset','.umap')]
    files=[dict(file=p.name,bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(OUT.glob('AttackSlashV2_Native.*'))]
    report=dict(nativeCandidateCount=1,manualAccepted=False,nativeCompiled=True,nativeColdReloaded=True,tenInstanceLifecyclePassed=True,
        renderRun=args.run,sequenceFrames=len(frames),encodedWebPFrames=len(runs),losslessWebPVerified=True,renderResolution=[1440,900],performanceMeasured=False,
        clientCookVerified=False,serverCookVerified=False,zodiacBindingsChanged=False,media=files,assets=assets,
        sourceContract=json.loads((source_root/'ShapeContract.json').read_text(encoding='utf8')))
    (OUT/'Verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print('ATTACK_SLASH_V2_REVIEW_EVIDENCE_PUBLISHED',len(assets),'native assets; 31 lossless frames; manual pending')


if __name__=='__main__':main()
