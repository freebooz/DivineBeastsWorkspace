# -*- coding: utf-8 -*-
"""把UE原生固定步进帧封装成视频；不调色、不锐化，不用概念图替换任何帧。"""
import subprocess,json,sys
from pathlib import Path
def run(folder):
 folder=Path(folder).resolve();ff=Path.home()/'AppData/Local/Microsoft/WinGet/Links/ffmpeg.exe';probe=ff.with_name('ffprobe.exe')
 report={'source':'native UE SceneCapture PNG; simulation step 1/60 s; not real-time recorder FPS','colourEnhancement':False,'clips':[]}
 def cmd(args):
  r=subprocess.run([str(ff),'-y','-hide_banner','-loglevel','error']+args,capture_output=True,text=True)
  if r.stderr:(folder/'EncodingWarnings.txt').open('a',encoding='utf8').write(r.stderr)
  assert r.returncode==0,r.stderr[-1000:]
 for typ in ['Fire','Ice']:
  clips=[]
  for view in ['Wide','Close','Cast']:
   frames=folder/(typ+'_'+view);assert len(list(frames.glob('*.png')))==84
   clip=folder/(typ+'_'+view+'.mp4');cmd(['-framerate','60','-i',str(frames/'%04d.png'),'-c:v','libx264','-preset','fast','-crf','16','-pix_fmt','yuv420p','-movflags','+faststart',str(clip)]);clips.append(clip)
  lst=folder/(typ+'_Concat.txt');lst.write_text('\n'.join("file '"+p.name+"'" for p in clips),encoding='utf8')
  output=folder/(typ+'_PalmReview_60fps.mp4');cmd(['-f','concat','-safe','0','-i',str(lst),'-c','copy','-movflags','+faststart',str(output)])
  slow=folder/(typ+'_PalmReview_HalfSpeed.mp4');cmd(['-i',str(output),'-vf','setpts=2*PTS','-r','60','-c:v','libx264','-preset','fast','-crf','16','-pix_fmt','yuv420p','-movflags','+faststart',str(slow)])
  for p in [*clips,output,slow]:
   data=json.loads(subprocess.check_output([str(probe),'-v','error','-count_frames','-select_streams','v:0','-show_entries','stream=width,height,nb_read_frames,r_frame_rate,duration','-of','json',str(p)],text=True))
   s=data['streams'][0];assert s['width']==1920 and s['height']==1080 and int(s['nb_read_frames'])>0
   report['clips'].append({'file':str(p),**s})
 (folder/'VideoInfo.json').write_text(json.dumps(report,indent=2),encoding='utf8');print(json.dumps(report))
if __name__=='__main__':run(sys.argv[1])
