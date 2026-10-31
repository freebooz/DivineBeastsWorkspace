"""资源归属发布门禁：复用Build的真实IoStore解析，读取有限迁移合同，失败关闭。"""
import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[3]


def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);sys.modules[name]=result;spec.loader.exec_module(result)
    return result


contract=module('resource_ownership_contract',Path(__file__).with_name('ResourceOwnershipContract.py'))
native_csv=module('resource_ownership_iostore',ROOT/'Build/Game/ServerPresentationPolicy.py')
DOC=contract.load_contract()
PROFILES={'FrontEndClient':'Client','VillageDevelopmentClient':'Client','PluginDemoClient':'Client',
    'DedicatedServer':'Server','VillageServer':'Server','VillageDevelopmentServer':'Server','FoundationStandalone':'Server'}


def under(package,root):
    root=root.casefold();return package==root or package.startswith(root+'/')


def validate(csv_path,profile,target=None):
    """旧身份与禁止目录的任何载荷拒绝；运行软依赖必须唯一正长度ExportBundleData。"""
    if profile not in PROFILES:raise ValueError('未知资源发布profile')
    role=target or PROFILES[profile]
    if role not in ('Client','Server') or (profile!='FoundationStandalone' and role!=PROFILES[profile]):
        raise ValueError('发布profile与目标端不一致')
    paths=[Path(csv_path)] if isinstance(csv_path,(str,Path)) else list(map(Path,csv_path))
    if not paths:raise ValueError('缺少容器清单')
    entries=[entry for path in paths for entry in native_csv.read_iostore_csv(path,role)]
    packages=[];exports=Counter();forbidden=[]
    retired={m['source'].casefold() for m in DOC['moves']}
    migrated={m['target'].casefold() for m in DOC['moves']}
    demo_allowed={m['target'].casefold() for m in DOC['moves'] if m['group']=='terrain_review'}
    for entry in entries:
        if not entry.filename:continue
        name=entry.filename
        if name.casefold().endswith(('.ubulk','.uexp','.uptnl')):name=name.rsplit('.',1)[0]+'.uasset'
        package=native_csv.package_from_artifact(name)
        if entry.chunk_type=='ExportBundleData' and package is None:raise ValueError('无法解析真实导出包名：'+name)
        if package is None:continue
        if entry.platform!='Windows'+role:raise ValueError('资源载荷平台不匹配：'+entry.filename)
        packages.append(package)
        if entry.chunk_type=='ExportBundleData':exports[package]+=1
        banned=package in retired or any(under(package,p) for p in DOC['retired_roots'])
        if any(under(package,p) for p in DOC['never_cook_roots']):
            banned |= not (profile=='PluginDemoClient' and package in demo_allowed)
        if role=='Server' and package in migrated:banned=True
        if banned:forbidden.append(entry.filename)
    if not exports:raise ValueError('容器没有任何真实资源导出')
    required=[m['target'] for m in DOC['moves'] if profile=='VillageDevelopmentClient' and m['group'] in ('plants_runtime','terrain_color')]
    missing=[p for p in required if exports[p.casefold()]!=1]
    return {'Passed':not forbidden and not missing,'Profile':profile,'Target':role,
        'RequiredCount':len(required),'MissingOrDuplicate':missing,'Forbidden':sorted(set(forbidden)),
        'GroupExportCounts':{g:sum(exports[m['target'].casefold()]>0 for m in DOC['moves'] if m['group']==g) for g in contract.GROUPS},
        'AssetExportCount':sum(exports.values()),'ContainerEvidence':[{'Csv':str(p.resolve()),'Sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in paths],
        'ContractSha256':hashlib.sha256(contract.MANIFEST.read_bytes()).hexdigest(),'ManualVisualAccepted':False}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--csv',type=Path,action='append')
    parser.add_argument('--stage',type=Path);parser.add_argument('--unreal-pak',type=Path)
    parser.add_argument('--profile',required=True);parser.add_argument('--report',type=Path,required=True)
    parser.add_argument('--target',choices=('Client','Server'))
    args=parser.parse_args()
    try:
        paths=args.csv
        if args.stage:
            stage=args.stage.resolve();folder=args.report.parent.resolve()
            if args.csv or not args.unreal_pak or not stage.is_relative_to(ROOT/'Saved') or not folder.is_relative_to(ROOT/'Saved'):
                raise ValueError('Stage与证据必须位于当前工程Saved，且不能混用CSV输入')
            containers=sorted(stage.rglob('*.utoc'))
            if not containers:raise ValueError('真实Stage无IoStore容器，不能作为验收')
            paths=[]
            for index,container in enumerate(containers):
                proof=folder/('Container'+str(index));proof.mkdir(parents=True,exist_ok=False)
                csv_path=proof/'Container.csv'
                with (proof/'stdout.log').open('wb') as output,(proof/'stderr.log').open('wb') as errors:
                    run=subprocess.run([str(args.unreal_pak),'-ListContainer='+str(container),'-Csv='+str(csv_path)],stdout=output,stderr=errors,timeout=120,check=False)
                if run.returncode:raise ValueError('UnrealPak容器读取失败：'+str(container))
                log=(proof/'stdout.log').read_text(encoding='utf-8-sig',errors='replace')+(proof/'stderr.log').read_text(encoding='utf-8-sig',errors='replace')
                if native_csv.ERROR_LOG.search(log):raise ValueError('UnrealPak容器日志报告错误')
                paths.append(csv_path)
        result=validate(paths or [],args.profile,args.target)
    except (OSError,ValueError,subprocess.TimeoutExpired,native_csv.PolicyError) as error:result={'Passed':False,'Error':str(error),'Profile':args.profile}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False));raise SystemExit(0 if result['Passed'] else 1)
