#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""群落编辑器制作协议。只连接显式拥有的Monolith编辑器，记录请求/响应，失败不自动重放。"""
import argparse
import json
from pathlib import Path
from urllib.request import Request,urlopen
from datetime import datetime,timezone
ROOT=Path(__file__).resolve().parents[4]
EVIDENCE=ROOT/'Saved/Validation/Fauna/Authoring'
def call(tool,arguments):
    EVIDENCE.mkdir(parents=True,exist_ok=True)
    request={'jsonrpc':'2.0','id':1,'method':'tools/call','params':{'name':tool,'arguments':arguments}}
    with urlopen(Request('http://127.0.0.1:9316/mcp',json.dumps(request,ensure_ascii=False).encode('utf-8'),
                         {'Content-Type':'application/json'}),timeout=120) as stream:
        response=json.load(stream)
    stamp=datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    (EVIDENCE/(stamp+'_'+tool+'.json')).write_text(json.dumps({'request':request,'response':response},ensure_ascii=False,indent=2),encoding='utf-8')
    if response.get('error'): raise RuntimeError(str(response['error']))
    result=response.get('result',{})
    if result.get('isError'): raise RuntimeError(str(result.get('content')))
    texts=[c['text'] for c in result.get('content',[]) if c.get('text')]
    if not texts: raise RuntimeError('Monolith未返回结果')
    try: value=json.loads(texts[0])
    except json.JSONDecodeError: value={'text':texts[0]}
    if isinstance(value,dict) and (value.get('success') is False or value.get('ok') is False or value.get('error') or value.get('failed_steps',0)):
        raise RuntimeError(json.dumps(value,ensure_ascii=False))
    return value
def niagara(action,**params): return call('niagara_query',{'action':action,'params':params})
def editor_python(source): return call('editor_query',{'action':'run_python','params':{'command':source,'unattended':True}})
def assert_owned_editor():
    return editor_python("import unreal\nassert '-FaunaOwnedAuthoring' in unreal.SystemLibrary.get_command_line()\nassert 'DivineBeastsWorkspace' in unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())\nprint('FAUNA_EDITOR_IDENTITY_VERIFIED')")
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('tool');p.add_argument('arguments');a=p.parse_args()
    print(json.dumps(call(a.tool,json.loads(a.arguments)),ensure_ascii=False,indent=2))
