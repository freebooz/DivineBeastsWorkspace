#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""第三批证据与逐插件审核索引，只读当前工程及本批真实结果，写Saved报告。

复用PluginDeliveryEvidence判定和现有65个UGamePlatformReviewCase，不创建第二套审核器。
输入SHA变化、报告缺少、零测试及人未确认不会升级成完成；旧P5/P6或历史R3不当本批通过。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
from PluginDeliveryEvidence import verify_automation,verify_manual,verify_content_container

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'Saved/Validation/PluginP7P9_20261012'

def read(path):
    p=ROOT/path
    if not p.is_file():return {}
    return json.loads(p.read_text(encoding='utf-8-sig'))

def relative(path):
    p=Path(path)
    return p.relative_to(ROOT).as_posix() if p.is_absolute() else p.as_posix()

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def verify_packaged_runtime(root: Path, base: Path, release: dict) -> dict:
    """校验当前Cook产生的实际客户端、独立官方Automation报告与发布RunId一一匹配。

    仅验证打包原生三项逻辑回归，不能替代音质、登录、联机、性能或人工验收。
    所有引用必须限制在当前工作区的真实文件，旧目录/替身汇总不通过。
    """
    from pathlib import PureWindowsPath
    root=root.resolve()
    base=Path(base)
    relative=base/'PackagedRuntimeVerified'
    summary_file=root/relative/'Summary.json'
    official=root/relative/'index.json'
    if base.is_absolute() or '..' in base.parts:
        return {'status':'blocked','reason':'审核根目录不允许外部路径'}
    if not summary_file.is_file() or not official.is_file():
        return {'status':'blocked','reason':'缺少本批已验证的包内原生报告或汇总'}
    try:
        summary=json.loads(summary_file.read_text(encoding='utf-8-sig'))
        expected_report=(relative/'index.json').as_posix()
        if summary.get('report')!=expected_report:
            return {'status':'blocked','reason':'汇总引用了旧版或不同目录的包内报告'}
        if summary.get('status')!='passed' or type(summary.get('processExit')) is not int or summary['processExit']!=0:
            return {'status':'failed','reason':'包内客户端执行未成功'}
        if type(summary.get('failed')) is not int or summary['failed']!=0:
            return {'status':'failed','reason':'包内汇总存在失败测试'}
        report=json.loads(official.read_text(encoding='utf-8-sig'))
        tests=report.get('tests')
        if not isinstance(tests,list) or not tests or type(summary.get('tests')) is not int or summary['tests']!=len(tests):
            return {'status':'failed','reason':'包内测试缺失或汇总数量不一致'}
        required={
            'GamePlatform.SFX.Review.ActorOwnershipAndRejection',
            'GamePlatform.VFX.BirdFlock.ActorInstanceLifecycle',
            'GamePlatform.VFX.BirdFlock.CountAndSeed',
        }
        names=[]
        warnings=0
        for case in tests:
            if not isinstance(case,dict):
                return {'status':'failed','reason':'包内用例结构无效'}
            name=case.get('fullTestPath')
            if not isinstance(name,str) or not name.strip():
                return {'status':'failed','reason':'包内用例缺少名称'}
            names.append(name)
            if case.get('state')!='Success' or int(case.get('errors',0))!=0:
                return {'status':'failed','reason':'包内官方报告有失败或错误用例','case':name}
            warnings+=int(case.get('warnings',0)>0)
        if len(names)!=len(set(names)) or not required.issubset(names):
            return {'status':'failed','reason':'包内用例重复或缺少三项必要原生回归'}
        if int(report.get('failed',0))!=0:
            return {'status':'failed','reason':'包内官方报告失败统计不为零'}
        run_id=release.get('clientCookRunId')
        if not isinstance(run_id,str) or not run_id.strip() or '/' in run_id or '\\' in run_id or '..' in run_id:
            return {'status':'blocked','reason':'当前客户端Cook运行身份无效'}
        expected_executable=Path('Saved/Validation/FoundationM0')/run_id/'Cook-FrontEndClient/Stage/WindowsClient/DivineBeastsArena/Binaries/Win64/DivineBeastsArenaClient.exe'
        actual=summary.get('executable')
        if not isinstance(actual,str) or PureWindowsPath(actual).is_absolute() or actual.replace('\\','/')!=expected_executable.as_posix():
            return {'status':'blocked','reason':'包内可执行程序不是当前Cook运行产物'}
        executable=(root/expected_executable).resolve()
        if not executable.is_relative_to(root) or not executable.is_file() or executable.stat().st_size==0:
            return {'status':'blocked','reason':'当前Cook发布的客户端程序不存在或为空'}
        return {'status':'passed','reason':'当前包内官方3项用例、运行身份和客户端产物相符',
                'tests':len(tests),'warningCases':warnings,'report':expected_report,
                'reportSha256':sha(official),'executable':expected_executable.as_posix(),
                'executableBytes':executable.stat().st_size,'manualAcceptance':'pending'}
    except (OSError,ValueError,TypeError,KeyError,AttributeError) as exc:
        return {'status':'blocked','reason':'包内证据解析异常：'+str(exc)}


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    base=OUT.relative_to(ROOT).as_posix()
    native_path=OUT/'CompleteEditorNative/index.json'
    native_inputs=read(base+'/FinalNativeInputs.json')
    groups=read(base+'/DeclaredTestGroups.json')
    prefixes=tuple(sorted({p+'.' for g in groups for p in g['prefixes']}))
    native=verify_automation(ROOT,{'path':relative(native_path),'sha256':sha(native_path)},native_inputs,prefixes)
    if native['status']!='passed':raise RuntimeError('当前114项原生报告不再满足输入/状态门禁：'+str(native))
    native_data=read(relative(native_path))
    grouped={}
    for g in groups:
        cases=[t for t in native_data['tests'] if any(t['fullTestPath'].startswith(p+'.') for p in g['prefixes'])]
        grouped[g['plugin']]={'tests':len(cases),'success':sum(t['state']=='Success' and not t.get('errors',0) for t in cases),
                              'warningCases':sum(t.get('warnings',0)>0 for t in cases),'prefixes':g['prefixes']}
    assets=read(base+'/Validate_assets.json')
    changed=[p for p,h in assets.get('inputs',{}).items() if not (ROOT/p).is_file() or sha(ROOT/p)!=h]
    if assets.get('status')!='passed' or changed:raise RuntimeError('内容包回读证据失效：'+str(changed[:8]))
    bypack={r['plugin']:r for r in assets['contentPacks']}
    native_cases={r['plugin']:r for r in assets['cases']}
    release=read(base+'/FinalReleaseRuns.json')
    client_csv=ROOT/('Saved/Validation/FoundationM0/'+release.get('clientCookRunId','missing')+'/Cook-FrontEndClient/ContainerInspection/ClientContainer.csv')
    client=verify_content_container(client_csv,'Client')
    server_csv=ROOT/release.get('serverContainerCsv','missing.csv')
    server=verify_content_container(server_csv,'Server')
    if client['status']!='passed' or server['status']!='passed':raise RuntimeError('最终客户端或服务端声音容器未通过')
    package=verify_packaged_runtime(ROOT,Path(base),release)
    if package['status']!='passed':
        raise RuntimeError('当前客户端包内回归证据缺失或不属于本次Cook：'+str(package))
    sfx=read(base+'/SFXRuntime/SFXRuntimeReview.json')
    back=read(base+'/BackendReview.json')
    targets=read(base+'/FinalTargetBuilds.json')
    old=read('Saved/Validation/PluginCompletionAudit_20261012/PluginCompletionAssessed.json')
    old_plugins={p['name']:p for p in old.get('plugins',[])}
    rows=[]
    for desc in sorted((ROOT/'Game/Plugins').rglob('*.uplugin')):
        if any(p in ('Binaries','Intermediate','Saved') for p in desc.parts):continue
        data=json.loads(desc.read_text(encoding='utf-8-sig'));name=desc.stem
        is_content='/ContentPacks/' in desc.as_posix()
        if name not in native_cases:raise RuntimeError('当前插件没有真实审核用例：'+name)
        is_target=name in grouped
        existing=old_plugins.get(name,{})
        maps=list(existing.get('associatedMaps',[]))
        if name=='GamePlatformSFX' or name=='DBASFXPack_Core':maps=['/Game/Development/Foundation/PluginReviews/Maps/L_GP_SFXReview']
        if name.startswith('DBAHeroPack_') or name=='DBAContentPack_Common':maps=['/Game/Development/Foundation/PluginReviews/Maps/L_DBA_HeroShowcase']
        roles={'Client':[], 'Server':[], 'Editor':[]}
        for module in data.get('Modules',[]):
            typ=module.get('Type','Runtime')
            for role in roles:
                eligible=(typ=='Runtime' or (typ=='ClientOnly' and role in ('Client','Editor')) or (typ=='ServerOnly' and role in ('Server','Editor')) or ('Editor' in typ and role=='Editor'))
                if eligible:roles[role].append(module['Name'])
        manual={'status':'NotRun','reviewer':'','explicit_human_confirmation':False,'evidence':[]}
        entry={'plugin':name,'description':data.get('Description',existing.get('chineseRole','')),
            'layer':'DivineBeastsContent' if is_content else ('MobaCommon' if '/MobaCommon/' in desc.as_posix() else ('DivineBeasts' if '/DivineBeasts/' in desc.as_posix() else 'GamePlatform')),
            'descriptor':relative(desc),'descriptorSha256':sha(desc),'reviewCase':native_cases[name]['case'],
            'moduleApplicability':roles,'demonstrationMaps':maps,'manualRecord':manual,
            'manualGate':verify_manual(manual),'completed':False}
        entry['gates']={
            'G0':{'status':'passed','evidence':base+'/ArchitectureFinal.log','scope':'当前65描述/47代码/18内容包与依赖结构'},
            'G1':{'status':'not_applicable' if is_content else ('passed' if is_target else 'not_run'),
                  'reason':'纯内容插件无独立C++编译' if is_content else ('本批受影响Editor模块及实际Client/Server目标编译' if is_target else '不以主目标成功外推未单独覆盖的插件'),
                  'evidence':base+'/FinalTargetBuilds.json'},
            'G2':{'status':'passed' if is_content else 'partial',
                  'reason':'实际内容包逐资产加载及SHA复核' if is_content else '真实ReviewCase存在；整个插件所有蓝图/资源仍需按用例独立验收',
                  'evidence':base+'/Validate_assets.json'},
            'G3':{'status':'passed' if is_target else ('not_applicable' if is_content else 'not_run'),
                  'reason':'本批实际原生测试' if is_target else ('纯内容包采用G2资源验证与G4场景审核，不凭空创建原生测试' if is_content else '本批未执行其全部适用用例'),
                  'testResult':grouped.get(name)},
            'G4':{'status':'partial' if maps or is_target else 'not_run',
                  'reason':'有共享/专项入口或原生逻辑证据，目标设备性能、正式业务集成与全部视觉仍未统一人工验收'},
            'G5':{'status':'partial','reason':'本批Client/Server Cook和新声音/开发资源门禁通过；人工签审仍未完成',
                  'evidence':base+'/FinalReleaseRuns.json'}}
        if is_content:entry['assetEvidence']=bypack.get(name,{})
        if name.startswith('DBAHeroPack_'):
            entry['artQuality']='原型；共用Mannequin/Quinn及项目原型材质，不等于最终生肖英雄3D资源'
        if name in ('GamePlatformLobby','GamePlatformVillage','GamePlatformLocalization'):
            entry['implementation']='R0条件保留/预留，不因ReviewCase创建而改为已实现'
        if name=='GamePlatformSFX':entry['runtimeEvidence']={'path':base+'/SFXRuntime/SFXRuntimeReview.json','status':sfx.get('status'),'checks':len(sfx.get('checks',[]))}
        rows.append(entry)
    if len(rows)!=len(native_cases) or len(rows)!=65:raise RuntimeError('实际插件、审核用例和台账数量不一致')
    content_count=sum(r['layer']=='DivineBeastsContent' for r in rows)
    if content_count!=18:raise RuntimeError('内容包注册数量异常')
    report={'schemaVersion':1,'date':'2026-10-12','scope':'第三批P7–P9','pluginCount':65,'codePluginCount':47,'contentPackCount':18,
       'editorNative':native,'gameWorldNative':read(base+'/VerifiedNativeGroups.json'),
       'packagedRuntime':package,'realAudioLifecycle':{'status':sfx.get('status'),'checks':len(sfx.get('checks',[]))},
       'contentLoadedAssets':assets['loadedAssets'],'reviewCases':65,'contentStable':not changed,
       'backendSandbox':back,'currentTargetBuilds':targets,'clientContainer':client,'serverContainer':server,
       'technicalDeliveryVerified':native['status']=='passed' and client['status']=='passed' and server['status']=='passed' and package.get('status')=='passed',
       'allPluginsComplete':False,'humanAcceptance':'pending','rows':rows,
       'limitations':['原生测试不等于全部功能或真实后端事务闭环','目标设备帧耗/混音/画面布局未由人签收','十二生肖展示是现有原型，不是最终12套模型','P5/P6竞技和可信准入未完成项仍独立保留']}
    (OUT/'PluginDeliveryCatalog.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    lines=['# 第三批：65插件审核入口与当前证据状态','',
           '现有65份真实ReviewCase对应47代码插件和18内容包。下列完成状态均未代签；部分逻辑插件应以命令行/沙箱而非空地图验收。','']
    for r in rows:
        lines+=['## '+r['plugin'],r['description'],
            '审核用例：`'+r['reviewCase']+'`。',
            '演示入口：'+('；'.join('`'+p+'`' for p in r['demonstrationMaps']) if r['demonstrationMaps'] else '逻辑/后台插件使用原生自动化或安全沙箱；无独立视觉地图。'),
            '本批原生测试：'+(str(r['gates']['G3']['testResult']['success'])+'/'+str(r['gates']['G3']['testResult']['tests']) if r['gates']['G3']['testResult'] else r['gates']['G3']['reason'])+'。',
            '门禁：'+'；'.join(k+'='+v['status'] for k,v in r['gates'].items())+'；人工=待确认。',r.get('artQuality',r.get('implementation','')),'']
    (OUT/'PluginReviewIndex.md').write_text('\n'.join(lines),encoding='utf-8')
    print('P9_CATALOG_GENERATED',len(rows),'native',native['passed'],'content',assets['loadedAssets'],'allComplete',False)
    if not report['technicalDeliveryVerified']:raise SystemExit(1)
if __name__=='__main__':main()
