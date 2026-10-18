#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""发布凝锋同镜头原生媒体、无损时间线及源/原生资产哈希；不改截图RGB或生成假截图。"""
from pathlib import Path
import argparse,hashlib,json,shutil
from PIL import Image

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash'
CONTENT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/CommonSkillVFX/Review/V3/AttackSlash'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    a=argparse.ArgumentParser();a.add_argument('--run',required=True)
    a.add_argument('--reuse-media',action='store_true',help='保留已有媒体，仍逐帧验证RGB、时间线及峰值原图')
    args=a.parse_args()
    run=(OUT/'RenderRuns'/args.run).resolve();assert run.parent==(OUT/'RenderRuns').resolve()
    data=json.loads((run/'Result.json').read_text(encoding='utf8'));assert data['status']=='captured' and len(data['frames'])==33 and data['resolution']==[1536,1024]
    frames=[Image.open(r['image']).convert('RGB') for r in data['frames']];durations=([33,33,34]*11)
    assert all(f.size==(1536,1024) for f in frames)
    if not args.reuse_media:
        frames[0].save(OUT/'AttackSlashV3_Native.webp',save_all=True,append_images=frames[1:],duration=durations,loop=0,lossless=True,quality=100,method=4)
    expected=[]
    for f,d in zip(frames,durations):
        raw=f.tobytes()
        if expected and expected[-1][0]==raw:expected[-1][1]+=d
        else:expected.append([raw,d])
    encoded=Image.open(OUT/'AttackSlashV3_Native.webp');assert encoded.n_frames==len(expected)
    for i,(pixels,d) in enumerate(expected):
        encoded.seek(i);assert encoded.convert('RGB').tobytes()==pixels and encoded.info['duration']==d
    if not args.reuse_media:shutil.copyfile(Path(data['frames'][11]['image']),OUT/'AttackSlashV3_Native.png')
    assert sha(OUT/'AttackSlashV3_Native.png')==sha(Path(data['frames'][11]['image']))
    cold=json.loads((OUT/'ColdValidation.json').read_text(encoding='utf8'));assert cold['passed']
    lifecycle=json.loads((OUT/'Lifecycle.json').read_text(encoding='utf8'));assert lifecycle['status']=='passed'
    guide=json.loads((SOURCE/'ReferenceGuide.json').read_text(encoding='utf8'))
    manifest=dict(tool='built-in image_gen',referenceRole='visual style reference only',referenceFile='Reference_凝锋.png',referenceSHA256=sha(SOURCE/'Reference_凝锋.png'),textureFile='T_DBA_SlashV3_Energy_Color.png',textureSHA256=sha(SOURCE/'T_DBA_SlashV3_Energy_Color.png'),sourceRGBA=[2172,724],nativeSRGB=True,nativeCompression='BC7',nativeBuiltSize=[2048,512],promptFile='EnergyTexturePrompt.txt',sourcePixelEditing=False,nativeScreenshotsAreGeneratedArt=False,manualAccepted=False)
    (SOURCE/'EnergyTextureManifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
    files=[dict(file=str(p.relative_to(ROOT)).replace('\\','/'),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(CONTENT.rglob('*')) if p.suffix in ('.uasset','.umap')]
    assert cold['packagesUnchangedDuringValidation'] and cold['nativePackageSHA256']=={f['file']:f['sha256'] for f in files},'原生包不是最后一次通过冷验证的字节版本'
    report=dict(candidateSkills=1,revision='凝锋V3',manualAccepted=False,nativeColdReloaded=True,niagaraCompileErrors=cold['diagnostics']['error_count'],niagaraCompileWarnings=cold['diagnostics']['warning_count'],tenInstanceLifecyclePassed=True,renderRun=args.run,nativeFrames=33,encodedWebPFrames=len(expected),losslessRGBAndTimelineVerified=True,cameraMatchMethod='reference silhouette guide; inferred camera, not original camera metadata',camera=guide['camera'],lookAt=guide['lookAt'],horizontalFOV=guide['fov'],resolution=[1536,1024],defaultDuration=.85,emitterCount=1,particlesPerInstance=1,materialSlots=3,meshTriangles=guide['triangles'],filamentHalfWidthMaxCm=guide['filamentHalfWidthMaxCm'],sourceImage=manifest,assets=files,packagedGPUVerified=False,clientCookVerified=False,serverCookVerified=False,zodiacBindingsChanged=False,media=[dict(file=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(OUT.glob('AttackSlashV3_*.mp4'))+sorted(OUT.glob('AttackSlashV3_Native.png'))+sorted(OUT.glob('AttackSlashV3_Native.webp'))])
    (OUT/'Verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('V3_NATIVE_EVIDENCE_VERIFIED',len(files),'native assets; 33 lossless native RGB frames')
if __name__=='__main__':main()
