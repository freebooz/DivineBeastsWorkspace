# -*- coding: utf-8 -*-
"""发布最终原生帧的原速/慢放视频、无损预览及保存包哈希；不改截图像素。"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,math,shutil,subprocess
from PIL import Image
import numpy as np
ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Saved/Validation/CommonSkillVFX/DirectedMissileV1'
PACK=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core'
SOURCE=PACK/'SourceArt/VFX/CommonSkillVFX/V1/DirectedMissile'
CONTENT=PACK/'Content/VFX/CommonSkillVFX/Review/V1/DirectedMissile'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser();p.add_argument('--run',required=True);p.add_argument('--ffmpeg',required=True);a=p.parse_args()
    run=(OUT/'RenderRuns'/a.run).resolve();assert run.parent==(OUT/'RenderRuns').resolve()
    capture=json.loads((run/'Result.json').read_text(encoding='utf8'))
    assert capture['status']=='captured' and len(capture['frames'])==60 and capture['resolution']==[1656,1002]
    frames=[Image.open(run/('Frame_'+str(i).zfill(3)+'.png')).convert('RGB') for i in range(60)]
    assert all(f.size==(1656,1002) for f in frames)
    spec=importlib.util.spec_from_file_location('native_media_validator',Path(__file__).with_name('PublishAttackSlashV3Evidence.py'));lib=importlib.util.module_from_spec(spec);spec.loader.exec_module(lib)
    videos=[]
    for label,fps in [('Native',30),('3xSlow',10)]:
        dest=run/('DirectedMissileV1_'+label+'.mp4')
        subprocess.run([a.ffmpeg,'-hide_banner','-loglevel','error','-y','-framerate',str(fps),'-i',str(run/'Frame_%03d.png'),'-frames:v','60','-c:v','libopenh264','-threads','4','-b:v','40M','-rc_mode','off','-qmin','8','-qmax','12','-pix_fmt','yuv420p','-movflags','+faststart',str(dest)],check=True,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        videos.append(lib.validate_video(dest,frames,fps,a.ffmpeg,run))
    durations=[33,33,34]*20
    frames[0].save(run/'DirectedMissileV1_Native.webp',save_all=True,append_images=frames[1:],duration=durations,loop=0,lossless=True,quality=100,method=4)
    expected=[]
    for f,d in zip(frames,durations):
        raw=f.tobytes()
        if expected and expected[-1][0]==raw:expected[-1][1]+=d
        else:expected.append([raw,d])
    webp=Image.open(run/'DirectedMissileV1_Native.webp');assert webp.n_frames==len(expected)
    for i,(raw,d) in enumerate(expected):
        webp.seek(i);assert webp.convert('RGB').tobytes()==raw and webp.info['duration']==d
    peak=run/'DirectedMissileV1_Native.png';shutil.copyfile(run/'Frame_021.png',peak);assert sha(peak)==sha(run/'Frame_021.png')
    ab=json.loads((OUT/'AirCaptureRuns.json').read_text(encoding='utf8'));assert ab['status']=='captured' and ab['originalMaterialRestored']
    air_paths=[Path(r['path'])/'Frame_000.png' for r in ab['runs']]
    offa,offb,on=[np.asarray(Image.open(p).convert('RGB')).astype(np.int16) for p in air_paths]
    assert np.array_equal(offa,offb),'空气关闭对照有变化，先排查时间/镜头/随机性'
    changed=np.max(np.abs(on-offa),axis=2)>0;assert int(changed.sum())>0,'原生空气开关未产生可见差异'
    air=dict(passed=True,controlIdentical=True,changedPixels=int(changed.sum()),changedFraction=float(changed.mean()),strengths=ab['strengths'],frames=[dict(file=str(p),sha256=sha(p)) for p in air_paths])
    (OUT/'AirWaveVerification.json').write_text(json.dumps(air,indent=2),encoding='utf8')
    cold=json.loads((OUT/'ColdValidation.json').read_text(encoding='utf8'));process=json.loads((OUT/'ColdProcessResult.json').read_text(encoding='utf-8-sig'))
    assert cold['passed'] and cold['packagesUnchangedDuringValidation'] and process['PID']==cold['pid'] and process['ExitCode']==0
    life=json.loads((OUT/'Lifecycle.json').read_text(encoding='utf8'));assert life['status']=='passed' and life['temporaryActorsRemoved']
    phase_file=OUT/'NativePhaseBindings.json';phase=json.loads(phase_file.read_text(encoding='utf8'))
    reads=phase['nativeModuleInputReads'];assert [r['emitter'] for r in reads]==['GoldTrail','AmberTrail','SoftGoldTrail']
    assert all(abs(float(r['value'])-math.radians(d))<.000001 for r,d in zip(reads,[0,60,120]))
    files=[dict(file=str(p.relative_to(ROOT)).replace('\\','/'),bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(CONTENT.rglob('*')) if p.suffix in ('.uasset','.umap')]
    assert cold['nativePackageSHA256']=={f['file']:f['sha256'] for f in files},'最终原生包在冷验证后变更'
    textures=[]
    for kind in ('Head','Trail'):
        path=SOURCE/('T_DBA_MissileV1_'+kind+'_Color.png');im=Image.open(path);assert im.mode=='RGBA'
        textures.append(dict(file=path.name,sha256=sha(path),sourceRGBA=list(im.size),promptFile=kind+'TexturePrompt.txt',promptSHA256=sha(SOURCE/(kind+'TexturePrompt.txt')),nativeSRGB=True,nativeCompression='BC7',nativeBuiltSize=[2048,512],sourcePixelEditing=False))
    manifest=dict(tool='built-in image_gen',referenceFile='Reference_DirectedMissile.png',referenceSHA256=sha(SOURCE/'Reference_DirectedMissile.png'),textures=textures,nativeScreenshotsAreGeneratedArt=False,manualAccepted=False)
    (SOURCE/'TextureManifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
    guide=json.loads((SOURCE/'ReferenceGuide.json').read_text(encoding='utf8'))
    report=dict(revision=guide.get('styleRevision','proportion_three_trails_60deg_01'),manualAccepted=False,nativeColdReloaded=True,coldProcessPID=cold['pid'],coldProcessExitCode=process['ExitCode'],niagaraCompileErrors=cold['diagnostics']['error_count'],niagaraCompileWarnings=cold['diagnostics']['warning_count'],renderRun=a.run,nativeFrames=60,encodedWebPFrames=len(expected),losslessRGBAndTimelineVerified=True,resolution=[1656,1002],emitterCount=4,trailCount=3,trailPhasesDegrees=[0,60,120],trailWidthCm=26,meshTriangles=guide['triangles'],meshMaterialSlots=3,tenInstanceLifecyclePassed=True,airWaveNativeABValidated=True,assets=files,sourceTextures=manifest,videoValidation=videos,productionBindingsChanged=False,packagedGPUVerified=False,clientCookVerified=False,serverCookVerified=False,qualityVariantsVerified=False)
    report['media']=[dict(file=str(p),bytes=p.stat().st_size,sha256=sha(p)) for p in [peak,run/'DirectedMissileV1_Native.mp4',run/'DirectedMissileV1_3xSlow.mp4',run/'DirectedMissileV1_Native.webp']]
    report['nativePhaseBindingsSHA256']=sha(phase_file)
    report['sourceModelSHA256']={n:sha(SOURCE/n) for n in ('DirectedMissileV1.blend','SM_DBA_DirectedMissile_V1.fbx','ReferenceGuide.json')}
    (OUT/'Verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8');print('MISSILE_NATIVE_EVIDENCE_VERIFIED',len(files),'native packages; 60 original frames')
if __name__=='__main__':main()
