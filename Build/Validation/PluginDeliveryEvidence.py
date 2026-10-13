#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""统一插件交付证据判定：读取官方报告与输入SHA，不运行游戏、不生成通过证据。

属于Build验证层，仅供既有Review工具汇总。自动测试、文件存在、人工观察相互独立；
空测试/过期输入/缺少报告不通过。人工身份来源仍由现有ReviewSubsystem与真实用户确认负责，
此模块只能核对外部人工记录的必要字段，不能证明签名来源或由AI代签。
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path, PureWindowsPath
from typing import Any, Iterable, Mapping


def _result(status: str, reason: str, **values: Any) -> dict[str, Any]:
    return {'status':status,'reason':reason,**values}


def _relative_file(root: Path, value: object) -> Path:
    if not isinstance(value,str) or not value.strip():
        raise ValueError('缺少工程相对路径')
    normalized=value.replace('\\','/')
    path=Path(normalized)
    if path.is_absolute() or PureWindowsPath(value).is_absolute() or '..' in path.parts or ':' in normalized:
        raise ValueError('不允许绝对路径、盘符或上级目录跳转')
    resolved=(root.resolve()/path).resolve()
    if not resolved.is_relative_to(root.resolve()):
        raise ValueError('证据符号链接越过工作区')
    if not resolved.is_file():raise ValueError('证据或输入文件不存在：'+value)
    return resolved


def _same_sha(path: Path, expected: object) -> bool:
    if not isinstance(expected,str) or len(expected)!=64:return False
    return hashlib.sha256(path.read_bytes()).hexdigest()==expected.lower()


def verify_automation(root: Path, evidence: Mapping[str,Any], inputs: Mapping[str,str],
                      required_prefixes: Iterable[str]) -> dict[str,Any]:
    """验证非空官方用例清单、真实报告哈希及当前输入哈希；不会用汇总数字掩盖失败。"""
    try:
        if not inputs:return _result('blocked','缺少验证时的输入基线')
        for source,digest in inputs.items():
            if not _same_sha(_relative_file(root,source),digest):
                return _result('blocked','验证输入已变化：'+source)
        path=_relative_file(root,evidence.get('path'))
        if path.stat().st_size>32*1024*1024:return _result('blocked','报告超过32MiB边界')
        if not _same_sha(path,evidence.get('sha256')):
            return _result('blocked','报告SHA-256不一致')
    except (OSError,ValueError,TypeError,AttributeError) as exc:
        return _result('blocked',str(exc))
    try:
        report=json.loads(path.read_text(encoding='utf-8-sig'))
        tests=report.get('tests')
        if not isinstance(tests,list) or not tests:return _result('failed','没有实际执行的测试清单')
        names=[];failures=[];warning_cases=0
        for test in tests:
            if not isinstance(test,dict):return _result('failed','测试项结构无效')
            name=test.get('fullTestPath')
            if not isinstance(name,str) or not name:return _result('failed','测试项缺少稳定身份')
            names.append(name)
            errors=int(test.get('errors',0));warnings=int(test.get('warnings',0))
            if test.get('state')!='Success' or errors!=0 or warnings<0:failures.append(name)
            if warnings>0:warning_cases+=1
        if len(names)!=len(set(names)):return _result('failed','存在重复测试身份')
        prefixes=tuple(required_prefixes)
        if not prefixes or any(not isinstance(p,str) or not p for p in prefixes):
            return _result('failed','缺少目标测试前缀，不能验证任意无关报告')
        missing=[p for p in prefixes if not any(n.startswith(p) for n in names)]
        if missing:return _result('failed','未实际执行要求的测试组',missing_prefixes=missing)
        if failures:return _result('failed','存在失败、未运行或带错误的测试',failed=len(failures),total=len(names),cases=failures)
        return _result('passed','官方用例状态、报告和输入哈希一致',passed=len(names),failed=0,
                       warning_cases=warning_cases,total=len(names))
    except (OSError,ValueError,TypeError,AttributeError) as exc:
        return _result('failed','无法读取完整官方自动化报告：'+str(exc))


def verify_content_container(csv_path: Path, target: str) -> dict[str,Any]:
    """检查最终UnrealPak清单：正式天气声音依赖完整，Server和正式包不含开发审核资源。"""
    import csv
    import io
    if target not in ('Client','Server'):return _result('blocked','必须明确Windows Client或Server目标')
    try:
        raw=Path(csv_path).read_bytes()
        reader=csv.DictReader(io.StringIO(raw.decode('utf-8-sig')),skipinitialspace=True)
        fields=reader.fieldnames or []
        clean=[name.strip() for name in fields]
        if len(clean)!=len(set(clean)) or not {'Filename','Platform'}.issubset(clean):
            return _result('blocked','容器清单缺少唯一Filename/Platform字段')
        file_key=fields[clean.index('Filename')];platform_key=fields[clean.index('Platform')]
        names=set()
        for row in reader:
            if None in row or not row.get(file_key) or not row.get(platform_key):
                return _result('blocked','容器清单行不完整')
            if row[platform_key].strip().casefold()!=('Windows'+target).casefold():
                return _result('blocked','容器平台与目标不匹配')
            names.add(row[file_key].strip().replace('\\','/').casefold())
        if not names:return _result('blocked','容器清单为空')
    except (OSError,ValueError,csv.Error) as exc:return _result('blocked',str(exc))
    base='/plugins/divinebeasts/contentpacks/presentation/dbasfxpack_core/content/weather/'
    required=[]
    if target=='Client':
        required=[base+'definitions/da_dba_sfx_weather_rain.uasset',base+'definitions/da_dba_sfx_weather_snow.uasset',
                  base+'audio/sw_dba_weather_rainheavy_loop.uasset',base+'audio/sw_dba_weather_wind_loop.uasset']
    # RainLight仅被开发试听定义引用，不为了通过检查强迫将未使用的原型声音加入正式包。
    missing=[suffix for suffix in required if not any(name.endswith(suffix) for name in names)]
    review=[name for name in names if '/development/foundation/pluginreviews/' in name or '/development/foundation/definitions/p7sfx/' in name]
    audio=[name for name in names if '/dbasfxpack_core/' in name]
    source_art=[name for name in audio if '/sourceart/' in name]
    forbidden=sorted(set(review+source_art+(audio if target=='Server' else [])))
    return _result('failed' if missing or forbidden else 'passed','最终容器依赖与开发/端侧排除检查',
                   target=target,required_count=len(required),missing=missing,forbidden=forbidden,
                   audio_pack_files=len(audio),container_files=len(names),csv_sha256=hashlib.sha256(raw).hexdigest(),
                   manual_acceptance='pending')


def verify_manual(record: Mapping[str,Any]) -> dict[str,Any]:
    """只校验人提供的必要字段，不创建人工记录；调用方必须独立保证真实人工确认来源。"""
    if record.get('explicit_human_confirmation') is not True:
        return _result('not_run','待真实审核者明确确认，自动化不可代签')
    reviewer=record.get('reviewer')
    if not isinstance(reviewer,str) or not reviewer.strip():return _result('blocked','审核人为空')
    evidence=record.get('evidence')
    if not isinstance(evidence,list) or not evidence or not all(isinstance(x,str) and x.strip() for x in evidence):
        return _result('blocked','缺少人工观察证据')
    status=record.get('status')
    if status=='Failed':return _result('failed','人工审核驳回')
    if status!='Passed':return _result('not_run','人工审核尚未给出通过结论')
    return _result('passed','已核对外部人工记录必要字段；身份来源由现有审核服务负责')


def combine_gates(gates: Mapping[str,Mapping[str,Any]], required: Iterable[str]) -> dict[str,Any]:
    """仅聚合已经分别核验的门禁；任何缺失、待审或无理由的不适用均不能整体完成。"""
    required=tuple(required)
    outstanding=[]
    for name in required:
        item=gates.get(name)
        if not item:
            outstanding.append({'gate':name,'status':'not_run','reason':'缺少门禁结果'})
            continue
        status=item.get('status')
        if status=='passed':continue
        if status=='not_applicable' and isinstance(item.get('reason'),str) and item['reason'].strip():continue
        outstanding.append({'gate':name,**item})
    return {'complete':bool(required) and not outstanding,'outstanding':outstanding,'required':list(required)}


def main() -> int:
    """正式发布脚本只读调用容器子门禁；失败也落本次报告，禁止留下旧成功文件。"""
    import argparse
    parser=argparse.ArgumentParser(description='插件内容最终容器依赖与端侧排除门禁')
    parser.add_argument('--container-csv',type=Path,required=True)
    parser.add_argument('--target',choices=('Client','Server'),required=True)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    result=verify_content_container(args.container_csv,args.target)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    print('PLUGIN_CONTENT_CONTAINER',result['status'],args.target,
          'required',result.get('required_count',0),'missing',len(result.get('missing',[])),
          'forbidden',len(result.get('forbidden',[])))
    return 0 if result['status']=='passed' else 1


if __name__=='__main__':
    raise SystemExit(main())

