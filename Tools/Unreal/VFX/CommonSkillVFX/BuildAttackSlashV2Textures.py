#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""攻击弧刃V2技术纹理：沿曲线UV的橙金色阶、连续流线遮罩与低频切线法线。

项目层制作源；颜色sRGB，Masks/Normal线性。无随机噪声、无颗粒，不改设计原图。
"""
from pathlib import Path
import argparse
import json
import hashlib
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[4]
OUT=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V2/AttackSlash'


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--repair-own-output',action='store_true');args=parser.parse_args()
    OUT.mkdir(parents=True,exist_ok=True)
    if (OUT/'T_DBA_SlashV2_Color.png').exists():
        if not args.repair_own_output:raise RuntimeError('纹理已存在，拒绝盲目覆盖')
        contract=json.loads((OUT/'TextureContract.json').read_text(encoding='utf8'))
        for item in contract['files']:
            if hashlib.sha256((OUT/item['file']).read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('源图已被其他操作修改，停止覆盖')
    h,w=512,2048;v,u=np.mgrid[0:h,0:w].astype(np.float32);u/=w-1;v/=h-1
    lo=np.array([1.,.22,.015]);mid=np.array([1.,.51,.11]);hi=np.array([1.,.92,.56])
    blend=np.clip(v*1.6,0,1)[...,None];rgb=lo*(1-blend)+mid*blend
    edge=np.clip((v-.68)/.32,0,1)[...,None]**1.3;rgb=rgb*(1-edge)+hi*edge
    # 只用四条低密度长流线，全部连续且沿主刃UV，不堆高频纹理。
    flow=np.zeros_like(u)
    for center,amp,phase in ((.18,.12,0),(.38,.15,1.2),(.62,.10,2.1),(.80,.065,3.1)):
        path=center+amp*np.sin(u*np.pi*1.6+phase)
        flow+=np.exp(-((v-path)/.012)**2)*.65
    flow=np.clip(flow,0,1);rgb=np.clip(rgb+flow[...,None]*np.array([.09,.16,.09]),0,1)
    body=.08+.86*np.maximum(0,np.sin(np.pi*v))**.65
    masks=np.stack((body,flow,np.exp(-((v-.96)/.045)**2),np.ones_like(u)),axis=2)
    height=.035*np.sin(np.pi*v)+.004*np.sin(u*np.pi*3)*np.sin(np.pi*v)
    dy,dx=np.gradient(height);normal=np.stack((-dx*220,-dy*120,np.ones_like(u)),axis=2);normal/=np.linalg.norm(normal,axis=2,keepdims=True)
    normal=normal*.5+.5
    Image.fromarray((rgb*255+.5).astype('uint8'),'RGB').save(OUT/'T_DBA_SlashV2_Color.png')
    Image.fromarray((masks*255+.5).astype('uint8'),'RGBA').save(OUT/'T_DBA_SlashV2_Masks.png')
    Image.fromarray((normal*255+.5).astype('uint8'),'RGB').save(OUT/'T_DBA_SlashV2_Normal.png')
    files=[dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(OUT.glob('*.png'))]
    (OUT/'TextureContract.json').write_text(json.dumps(dict(resolution=[w,h],colorSRGB=True,maskSRGB=False,normalSRGB=False,noise=False,files=files),indent=2),encoding='utf8')
    print('SLASH_V2_TECHNICAL_TEXTURES',len(files))


if __name__=='__main__':main()
