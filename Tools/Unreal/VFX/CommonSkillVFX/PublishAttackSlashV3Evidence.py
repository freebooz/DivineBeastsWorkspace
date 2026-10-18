#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""发布凝锋同镜头原生媒体、无损时间线及源/原生资产哈希；不改截图RGB或生成假截图。"""
from pathlib import Path
import argparse,hashlib,json,re,shutil,subprocess
from PIL import Image
import numpy as np

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/CommonSkillVFX/AttackSlashV3'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V3/AttackSlash'
CONTENT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/CommonSkillVFX/Review/V3/AttackSlash'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def validate_video(path,frames,fps,ffmpeg,native_run):
    """核对实际MP4时间信息及全部解码帧，防止把旧视频仅列入哈希后误当本次输出。"""
    header=subprocess.run([ffmpeg,'-hide_banner','-i',str(path)],capture_output=True,text=True,errors='replace',creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0)).stderr
    rate=re.search(r'\s(\d+(?:\.\d+)?) fps\b',header);duration=re.search(r'Duration:\s*(\d+):(\d+):([\d.]+)',header)
    assert rate and abs(float(rate[1])-fps)<.01 and duration,'视频FPS或时长信息缺失/不匹配'
    seconds=int(duration[1])*3600+int(duration[2])*60+float(duration[3]);assert abs(seconds-len(frames)/fps)<.035
    # 在编码输入的YUV420p空间比较，避免把RGB↔YUV色彩量化误差误判为视频帧内容错位。
    # 无损RGB合同仍由下面的PNG/WebP逐像素检查独立承担。
    size=frames[0].width*frames[0].height*3//2
    kwargs=dict(stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
    proc=subprocess.Popen([ffmpeg,'-hide_banner','-loglevel','error','-i',str(path),'-map','0:v:0','-vsync','0','-f','rawvideo','-pix_fmt','yuv420p','-'],**kwargs)
    reference=subprocess.Popen([ffmpeg,'-hide_banner','-loglevel','error','-framerate',str(fps),'-i',str(native_run/'Frame_%03d.png'),'-frames:v',str(len(frames)),'-f','rawvideo','-pix_fmt','yuv420p','-'],**kwargs)
    errors=[]
    try:
        for index,expected in enumerate(frames):
            raw=proc.stdout.read(size);assert len(raw)==size,'视频解码帧数/分辨率与原生PNG不符'
            source=reference.stdout.read(size);assert len(source)==size,'原生PNG序列YUV转换失败'
            actual=np.frombuffer(raw,dtype=np.uint8)
            difference=np.abs(actual.astype(np.int16)-np.frombuffer(source,dtype=np.uint8).astype(np.int16))
            mean=float(difference.mean());p99=float(np.percentile(difference,99))
            assert mean<1.5 and p99<16, '视频内容与最终原生帧不符：frame='+str(index)
            errors.append(mean)
        assert not proc.stdout.read(1),'视频包含额外帧'
        stderr=proc.stderr.read().decode('utf8',errors='replace');assert proc.wait(timeout=30)==0,stderr
        assert not reference.stdout.read(1),'原生转换包含额外帧'
        stderr=reference.stderr.read().decode('utf8',errors='replace');assert reference.wait(timeout=30)==0,stderr
    finally:
        if proc.poll() is None:proc.kill();proc.wait()
        if reference.poll() is None:reference.kill();reference.wait()
    return dict(file=path.name,decodedFrames=len(frames),fps=fps,durationSeconds=seconds,comparisonSpace='encoder YUV420p',maxFrameMeanYUVError=max(errors),lossyEncoding=True,allDecodedFramesComparedToNative=True)
def main():
    a=argparse.ArgumentParser();a.add_argument('--run',required=True)
    a.add_argument('--reuse-media',action='store_true',help='保留已有媒体，仍逐帧验证RGB、时间线及峰值原图')
    a.add_argument('--ffmpeg',default=shutil.which('ffmpeg') or shutil.which('hffmpeg'),help='用于验证实际视频内容的ffmpeg可执行文件')
    args=a.parse_args()
    run=(OUT/'RenderRuns'/args.run).resolve();assert run.parent==(OUT/'RenderRuns').resolve()
    data=json.loads((run/'Result.json').read_text(encoding='utf8'));assert data['status']=='captured' and len(data['frames'])==33 and data['resolution']==[1536,1024]
    assert all(Path(r['image']).resolve()==(run/('Frame_'+str(i).zfill(3)+'.png')).resolve() for i,r in enumerate(data['frames']))
    frames=[Image.open(r['image']).convert('RGB') for r in data['frames']];durations=([33,33,34]*11)
    assert all(f.size==(1536,1024) for f in frames)
    assert args.ffmpeg,'请使用--ffmpeg指定本机ffmpeg，以核对实际视频帧'
    videos=[validate_video(OUT/'AttackSlashV3_Native.mp4',frames,30,args.ffmpeg,run),validate_video(OUT/'AttackSlashV3_3xSlow.mp4',frames,10,args.ffmpeg,run)]
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
    process=json.loads((OUT/'ColdProcessResult.json').read_text(encoding='utf-8-sig'))
    assert process['PID']==cold['pid'] and process['ExitCode']==0,'冷加载报告与成功退出的根进程不对应'
    lifecycle=json.loads((OUT/'Lifecycle.json').read_text(encoding='utf8'));assert lifecycle['status']=='passed'
    air=json.loads((OUT/'AirWaveVerification.json').read_text(encoding='utf8'));assert air['passed'] and air['controlIdentical']
    for f in air['frames']:assert sha(Path(f['file']))==f['sha256'],'空气波动对照帧已变动'
    guide=json.loads((SOURCE/'ReferenceGuide.json').read_text(encoding='utf8'))
    manifest=dict(tool='built-in image_gen',referenceRole='visual style reference only',referenceFile='Reference_凝锋.png',referenceSHA256=sha(SOURCE/'Reference_凝锋.png'),textureFile='T_DBA_SlashV3_Energy_Color.png',textureSHA256=sha(SOURCE/'T_DBA_SlashV3_Energy_Color.png'),sourceRGBA=[2172,724],nativeSRGB=True,nativeCompression='BC7',nativeBuiltSize=[2048,512],promptFile='EnergyTexturePrompt.txt',sourcePixelEditing=False,nativeScreenshotsAreGeneratedArt=False,manualAccepted=False)
    (SOURCE/'EnergyTextureManifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
    files=[dict(file=str(p.relative_to(ROOT)).replace('\\','/'),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(CONTENT.rglob('*')) if p.suffix in ('.uasset','.umap')]
    assert cold['packagesUnchangedDuringValidation'] and cold['nativePackageSHA256']=={f['file']:f['sha256'] for f in files},'原生包不是最后一次通过冷验证的字节版本'
    report=dict(candidateSkills=1,revision='凝锋V3',styleRevision=guide.get('styleRevision'),manualAccepted=False,nativeColdReloaded=True,niagaraCompileErrors=cold['diagnostics']['error_count'],niagaraCompileWarnings=cold['diagnostics']['warning_count'],tenInstanceLifecyclePassed=True,renderRun=args.run,nativeFrames=33,encodedWebPFrames=len(expected),losslessRGBAndTimelineVerified=True,cameraMatchMethod='reference silhouette guide; inferred camera, not original camera metadata',camera=guide['camera'],lookAt=guide['lookAt'],horizontalFOV=guide['fov'],resolution=[1536,1024],defaultDuration=.85,emitterCount=1,particlesPerInstance=1,materialSlots=guide['materialSlots'],meshTriangles=guide['triangles'],filamentHalfWidthMaxCm=guide['filamentHalfWidthMaxCm'],airWave=guide['airWaveEnvelope'],sourceImage=manifest,assets=files,packagedGPUVerified=False,clientCookVerified=False,serverCookVerified=False,zodiacBindingsChanged=False,media=[dict(file=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(OUT.glob('AttackSlashV3_*.mp4'))+sorted(OUT.glob('AttackSlashV3_Native.png'))+sorted(OUT.glob('AttackSlashV3_Native.webp'))])
    report['coldProcessPID']=cold['pid'];report['coldProcessExitCode']=process['ExitCode'];report['coldValidationSHA256']=sha(OUT/'ColdValidation.json')
    report['videoValidation']=videos;report['airWaveVerificationSHA256']=sha(OUT/'AirWaveVerification.json')
    (OUT/'Verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('V3_NATIVE_EVIDENCE_VERIFIED',len(files),'native assets; 33 lossless native RGB frames')
if __name__=='__main__':main()
