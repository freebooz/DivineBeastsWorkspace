# -*- coding: utf-8 -*-
"""只归纳UE原生CSV和SimCache记录，不把脚本耗时当成GPU耗时。"""
import csv,json,sys,statistics
from pathlib import Path
def summarize(folder):
 folder=Path(folder);report=json.loads((folder/'Result.json').read_text());rows=[]
 for s in report['stages']:
  p=Path(s['csv'])
  if not p.exists():continue
  # UE CSV的帧数据后会再次写表头及元数据；只读取明确请求的300帧。
  data=list(csv.DictReader(p.open(encoding='utf-8-sig')))[:300];stats={}
  wanted=['FrameTime','GPUTime','GameThreadTime','RenderThreadTime','GPU/Translucency','GPU/Distortion','Exclusive/RenderThread/Niagara']
  for k in wanted:
   a=[]
   for r in data:
    try:v=float(r.get(k,''))
    except (ValueError,TypeError):continue
    if v>=0:a.append(v)
   if a:
    a.sort();stats[k]={'samples':len(a),'meanMs':round(statistics.mean(a),4),'p95Ms':round(a[min(len(a)-1,int(len(a)*.95))],4),'maxMs':round(max(a),4)}
  rows.append({'type':s['type'],'quality':s['quality'],'systems':s['systemsRequested'],'actualCountsPerPalm':s['actualCounts'],'stats':stats})
 out={'context':report['context'],'claim':'Editor lab timings only; GPUTime includes whole editor and SceneCapture; Translucency is a whole pass, not isolated spell GPU time','rows':rows}
 (folder/'Summary.json').write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf8');print(json.dumps(out,ensure_ascii=False))
if __name__=='__main__':summarize(sys.argv[1])
