#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群发布产物门禁：检查UnrealPak真实IoStore清单，不以源资源存在或Cook退出0冒充发布成功。

Client必须包含三种新Flight网格/材质/纹理/Niagara/WorldDefinition、正式蓝图和平台运行依赖；
Client还必须拒绝历史原型及其附属块、源美术、开发审核图及其外置对象；Server不得含任何鸟群纯视觉资源。
严格核对Windows目标平台；只读CSV并写验证报告，无网络和源资源写入。
Niagara模块编辑器图不要求作为独立运行包存在，其编译结果由System/Emitter持有。
"""
from pathlib import Path
import argparse
import csv
import hashlib
import io
import json


def validate(csv_path: Path, target: str) -> dict:
    """只读验证Windows目标：输入格式/平台错误抛ValueError，内容违规返回failed。"""
    if target not in ('Client','Server'):
        raise ValueError('必须明确选择Client或Server，未知目标不能作为空规则放行')
    csv_path=Path(csv_path)
    if not csv_path.is_file():
        raise FileNotFoundError('缺少最终IoStore清单：'+str(csv_path))
    # 解析与哈希使用同一份字节，避免并发修改导致证据哈希和检查内容不同。
    raw=csv_path.read_bytes()
    reader=csv.DictReader(io.StringIO(raw.decode('utf-8-sig')),skipinitialspace=True)
    fields=reader.fieldnames or []
    names=[v.strip() for v in fields]
    if len(names)!=len(set(names)) or not {'Filename','Platform'}.issubset(names):
        raise ValueError('IoStore清单必须有唯一Filename和Platform列')
    field=fields[names.index('Filename')]
    platform_field=fields[names.index('Platform')]
    expected_platform='Windows'+target
    filenames=set()
    for line,row in enumerate(reader,2):
        filename=row.get(field)
        platform=row.get(platform_field)
        if None in row or not filename or not filename.strip() or not platform:
            raise ValueError('容器清单行不完整：'+str(line))
        if platform.strip().casefold()!=expected_platform.casefold():
            raise ValueError('容器平台与目标不匹配：第%d行，期望%s，实际%s'%(line,expected_platform,platform))
        filenames.add(filename.strip().replace('\\','/'))
    if not filenames:
        raise ValueError('容器清单没有任何运行文件')
    canonical={p.casefold() for p in filenames}
    birds=sorted(p for p in filenames if '/birdflock/' in p.casefold())
    source_art=sorted(p for p in birds if '/sourceart/' in p.casefold())
    # 同时覆盖普通地图及__ExternalActors__/__ExternalObjects__下的世界分区附属包。
    review_maps=sorted(p for p in birds if '/development/birdflock/' in p.casefold())
    # 根Meshes中的原型及ubulk/uexp等附属块全部禁入；Flight子目录不命中此规则。
    legacy_files=sorted(p for p in birds if '/birdflock/meshes/sm_dba_bird_' in p.casefold())
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
        missing=[suffix for suffix in required if not any(p.endswith(suffix.casefold()) for p in canonical)]
        legacy=[p for p in legacy_files if p.casefold().endswith('.uasset')]
    forbidden=birds if target=='Server' else sorted(set(source_art+review_maps+legacy_files))
    report={'status':'passed' if not missing and not forbidden else 'failed',
        'schemaVersion':2,'target':target,'expectedPlatform':expected_platform,
        'containerCsv':str(csv_path),'csvSha256':hashlib.sha256(raw).hexdigest(),
        'containerFiles':len(filenames),'birdAssetFiles':len(birds),'requiredCount':len(required),
        'missing':missing,'forbidden':forbidden,'legacyUnusedMeshes':legacy,
        'sourceArtFiles':source_art,'developmentReviewFiles':review_maps,'legacyFiles':legacy_files,
        'legacyNote':'历史原型及附属块禁止进入正式包；源文件保留，不通过删除源文件掩盖引用。',
        'validatedSuffixes':required,'manualAcceptance':'pending'}
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--csv',required=True,type=Path)
    parser.add_argument('--target',required=True,choices=('Client','Server'))
    parser.add_argument('--report',required=True,type=Path)
    args=parser.parse_args()
    try:
        report=validate(args.csv,args.target)
    except (OSError,ValueError,csv.Error) as error:
        # 输入错误也覆盖为本次失败报告，防止上层沿用旧成功结果。
        report={'schemaVersion':2,'status':'failed','target':args.target,'containerCsv':str(args.csv),
                'inputError':str(error),'missing':[],'forbidden':[],'legacyUnusedMeshes':[],
                'requiredCount':0,'manualAcceptance':'pending'}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print('BIRD_CONTAINER_VALIDATION',report['status'],args.target,
          'required',report['requiredCount'],'missing',len(report['missing']),
          'forbidden',len(report['forbidden']),'legacy',len(report['legacyUnusedMeshes']))
    if report['status']!='passed':
        raise SystemExit(1)


if __name__=='__main__':
    main()
