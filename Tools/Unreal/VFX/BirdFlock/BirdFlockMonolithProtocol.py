#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""鸟群资产作者专用Monolith本机协议：记录完整响应，失败中止且不自动重放写操作。
不负责资源身份/运行时加载，不是第二套资产管理器；仅编辑器制作工具使用。
"""
import json
from pathlib import Path
from urllib.request import Request, urlopen
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = ROOT / 'Saved/Validation/BirdFlockP4'

def call(tool: str, arguments: dict):
    """调用已核对schema的本机Monolith工具；保存请求和响应再检查真实错误。"""
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    request = {'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':tool,'arguments':arguments}}
    raw = json.dumps(request, ensure_ascii=False).encode('utf-8')
    with urlopen(Request('http://127.0.0.1:9316/mcp', data=raw,
                        headers={'Content-Type':'application/json'}, method='POST'), timeout=90) as r:
        response = json.load(r)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    (EVIDENCE / (stamp+'_'+tool+'.json')).write_text(
        json.dumps({'request':request,'response':response}, ensure_ascii=False, indent=2), encoding='utf-8')
    if response.get('error'):
        raise RuntimeError(str(response['error']))
    result = response.get('result',{})
    if result.get('isError'):
        raise RuntimeError(str(result.get('content')))
    texts = [c['text'] for c in result.get('content',[]) if c.get('text')]
    if not texts:
        raise RuntimeError('Monolith响应未包含结果')
    try:
        value = json.loads(texts[0])
    except json.JSONDecodeError:
        value = {'text':texts[0]}
    if isinstance(value,dict) and (value.get('success') is False or value.get('ok') is False or value.get('error') or value.get('failed_steps',0)):
        raise RuntimeError(json.dumps(value,ensure_ascii=False))
    return value

def niagara(action: str, **params):
    return call('niagara_query', {'action':action,'params':params})

def editor_python(source: str):
    return call('editor_query', {'action':'run_python','params':{'command':source,'unattended':True}})

if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description='按已核对Schema调用Monolith，留存完整证据')
    parser.add_argument('tool')
    parser.add_argument('arguments',help='JSON格式工具参数')
    args=parser.parse_args()
    print(json.dumps(call(args.tool,json.loads(args.arguments)),ensure_ascii=False,indent=2))
