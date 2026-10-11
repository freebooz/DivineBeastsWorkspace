#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群发布产物门禁：检查UnrealPak真实IoStore清单，不以源资源存在或Cook退出0冒充发布成功。

Client必须包含三种新Flight网格/材质/纹理/Niagara/WorldDefinition、正式蓝图和平台运行依赖；
Server不得包含任何鸟群纯视觉资源。只读CSV，输出Saved验证报告，无网络和源资源写入。
Niagara模块编辑器图不要求作为独立运行包存在，其编译结果由System/Emitter持有。
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json


def validate(csv_path: Path, target: str):
    if not csv_path.is_file():
        raise FileNotFoundError('缺少最终IoStore清单：'+str(csv_path))
    with csv_path.open(encoding='utf-8-sig',newline='') as stream:
        reader=csv.DictReader(stream,skipinitialspace=True)
        if not reader.fieldnames or 'Filename' not in [v.strip() for v in reader.fieldnames]:
            raise ValueError('不是UE IoStore容器导出CSV')
        field=next(v for v in reader.fieldnames if v.strip()=='Filename')
        filenames={row[field].strip().replace('\\','/') for row in reader if row.get(field)}
    if not filenames:
        raise ValueError('容器清单没有任何运行文件')
    birds=sorted(p for p in filenames if '/BirdFlock/' in p)
    source_art=sorted(p for p in birds if '/SourceArt/' in p)
    required=[];missing=[];legacy=[]
    if target=='Client':
        root='/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/Environment/BirdFlock'
        for species in ('Seagull','Swallow','Eagle'):
            required.extend([
                root+'/Meshes/Flight/SM_DBA_Bird_'+species+'.uasset',
                root+'/Textures/T_DBA_Bird_'+species+'_Albedo.uasset',
                root+'/Materials/MI_DBA_Bird_'+species+'.uasset',
                root+'/Niagara/NS_DBA_BirdFlock_'+species+'.uasset',
                root+'/Definitions/DA_DBA_VFX_BirdFlock_'+species+'.uasset'])
        required.append(root+'/Blueprints/BP_DBA_BirdFlockAmbient.uasset')
        gp='/Plugins/GamePlatform/Presentation/GamePlatformVFX/Content/Environment/BirdFlock'
        required.extend([gp+'/Materials/M_GP_BirdFlock_Master.uasset',
            gp+'/Niagara/NS_GP_BirdFlock_Base.uasset',
            gp+'/EffectTypes/NET_GP_BirdFlock_Ambient.uasset'])
        missing=[suffix for suffix in required if not any(p.endswith(suffix) for p in filenames)]
        legacy=[p for p in birds if '/Meshes/SM_DBA_Bird_' in p and p.endswith('.uasset')]
    forbidden=birds if target=='Server' else source_art
    report={'status':'passed' if not missing and not forbidden else 'failed',
        'target':target,'containerCsv':str(csv_path),'csvSha256':hashlib.sha256(csv_path.read_bytes()).hexdigest(),
        'containerFiles':len(filenames),'birdAssetFiles':len(birds),'requiredCount':len(required),
        'missing':missing,'forbidden':forbidden,'legacyUnusedMeshes':legacy,
        'legacyNote':'旧原型网格不再被正式Niagara引用；若被AlwaysCook纳入则仅提示包体清理，不当作现行飞行网格。',
        'validatedSuffixes':required,'manualAcceptance':'pending'}
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv',required=True,type=Path)
    parser.add_argument('--target',required=True,choices=('Client','Server'))
    parser.add_argument('--report',required=True,type=Path)
    args=parser.parse_args()
    report=validate(args.csv,args.target)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_CONTAINER_VALIDATION',report['status'],args.target,
          'required',report['requiredCount'],'missing',len(report['missing']),
          'forbidden',len(report['forbidden']),'legacy',len(report['legacyUnusedMeshes']))
    if report['status']!='passed':
        raise RuntimeError('鸟群容器门禁失败：'+json.dumps(report,ensure_ascii=False))


if __name__=='__main__':
    main()
