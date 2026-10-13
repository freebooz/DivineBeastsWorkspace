#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""仅通过已核对的Monolith MCP修复平台默认输入DataTable与中立父类，不更改项目UI页面。

读取编辑器身份和实际属性后，备份两件原始资产；全部写入使用Monolith blueprint_query
的受支持reparent_blueprint/set_data_table_rows/compile_blueprint/save_asset动作。
不以裸Python修改UI资产，不调用有额外Gamma错误的Widget截图路径。
默认只读；--apply正式修改；独立UE原生DefaultActionsAsset测试才是最终判定。
"""
from __future__ import annotations
import argparse
import hashlib
import json
import shutil
from pathlib import Path
from urllib.request import Request,urlopen

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Saved/Validation/PluginP7P9_20261012/UIInputMonolith'
BASE='/GamePlatformUI/Input/'
FILES=ROOT/'Game/Plugins/GamePlatform/Presentation/GamePlatformUI/Content/Input'
COUNT=0


def call(tool,args,label):
    global COUNT
    COUNT+=1;OUT.mkdir(parents=True,exist_ok=True)
    payload={'jsonrpc':'2.0','id':COUNT,'method':'tools/call','params':{'name':tool,'arguments':args}}
    req=Request('http://127.0.0.1:9316/mcp',json.dumps(payload).encode('utf-8'),{'Content-Type':'application/json'})
    with urlopen(req,timeout=90) as r:response=json.load(r)
    (OUT/(str(COUNT).zfill(2)+'_'+label+'.json')).write_text(json.dumps({'request':payload,'response':response},ensure_ascii=False,indent=2),encoding='utf-8')
    if response.get('error') or response.get('result',{}).get('isError'):raise RuntimeError('MCP失败：'+label)
    data=[json.loads(c['text']) for c in response['result']['content'] if c.get('type')=='text'][0]
    if any(data.get(k) is False for k in ('ok','success','bSuccess')) or data.get('errors'):
        raise RuntimeError('动作失败：'+label+' '+json.dumps(data,ensure_ascii=False))
    print('P7_MONOLITH',label,'success')
    return data


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--apply',action='store_true');args=parser.parse_args()
    status=call('monolith_status',{},'Status')
    if status.get('project_name')!='DivineBeastsArena' or not status.get('server_running'):
        raise RuntimeError('服务不是正式神兽联盟编辑器')
    call('editor_query',{'action':'run_python','params':{'command':
        "import unreal\nassert '-P7OwnedMonolithUIInput' in unreal.SystemLibrary.get_command_line()\nassert 'DivineBeastsWorkspace' in unreal.Paths.project_dir()\nassert hasattr(unreal,'GamePlatformUIInputData')\nprint('P7_MONOLITH_OWNED_EDITOR_VERIFIED')",'unattended':True}},'Identity')
    defaults=call('blueprint_query',{'action':'get_cdo_properties','params':{'asset_path':BASE+'BP_GP_UIInputData'}},'ReadDefaults')
    rows=call('blueprint_query',{'action':'read_data_table','params':{'asset_path':BASE+'DT_GP_UIActions','include_schema':False}},'ReadRows')
    if {r['row_name'] for r in rows.get('rows',[])}!={'Back','Accept'}:
        raise RuntimeError('发现非预期输入行，拒绝覆盖')
    if not args.apply:return
    backup=OUT/'Backup';backup.mkdir(parents=True,exist_ok=True)
    hashes={}
    for filename in ('BP_GP_UIInputData.uasset','DT_GP_UIActions.uasset'):
        source=FILES/filename;target=backup/filename
        digest=hashlib.sha256(source.read_bytes()).hexdigest()
        if target.exists():
            raise RuntimeError('已有恢复备份，本脚本不得盲目重放写入；应复核已执行动作后恢复')
        shutil.copyfile(source,target);hashes[filename]=digest
    (OUT/'BeforeHashes.json').write_text(json.dumps(hashes,indent=2),encoding='utf-8')
    # FKey的ImportText是Escape/Enter等键名字面量，不是(KeyName=...)；旧值被错误解析成左括号。
    updates=[]
    for name,key,pad in (('Back','Escape','Gamepad_FaceButton_Right'),('Accept','Enter','Gamepad_FaceButton_Bottom')):
        updates.append({'row_name':name,'mode':'update','values':{
            'KeyboardInputTypeInfo':'(Key='+key+')',
            'DefaultGamepadInputTypeInfo':'(Key='+pad+')'}})
    params={'asset_path':BASE+'DT_GP_UIActions','rows':updates,'strict':True,'dry_run':True,'save':False}
    call('blueprint_query',{'action':'set_data_table_rows','params':params},'ValidateInputPatch')
    call('blueprint_query',{'action':'reparent_blueprint','params':{'asset_path':BASE+'BP_GP_UIInputData',
         'new_parent_class':'/Script/GamePlatformUIClient.GamePlatformUIInputData'}},'ReparentClientOnlyInput')
    params.update(dry_run=False,save=True)
    call('blueprint_query',{'action':'set_data_table_rows','params':params},'WriteInputKeys')
    call('blueprint_query',{'action':'compile_blueprint','params':{'asset_path':BASE+'BP_GP_UIInputData'}},'CompileInputData')
    call('blueprint_query',{'action':'save_asset','params':{'asset_path':BASE+'BP_GP_UIInputData'}},'SaveInputData')
    after=call('blueprint_query',{'action':'get_cdo_properties','params':{'asset_path':BASE+'BP_GP_UIInputData'}},'ReadDefaultsAfter')
    if after.get('parent_class')!='GamePlatformUIInputData':raise RuntimeError('父类改变未得到Monolith回读确认')
    call('blueprint_query',{'action':'read_data_table','params':{'asset_path':BASE+'DT_GP_UIActions','include_schema':False}},'ReadRowsAfter')
    (OUT/'MutationReceipt.json').write_text(json.dumps({'status':'saved_via_monolith','monolithVersion':status.get('version'),
        'assets':[BASE+'BP_GP_UIInputData',BASE+'DT_GP_UIActions'],'afterHashes':{
            name:hashlib.sha256((FILES/name).read_bytes()).hexdigest() for name in hashes},
        'independentNativeValidation':'pending','manualAcceptance':'pending'},ensure_ascii=False,indent=2),encoding='utf-8')
    print('P7_MONOLITH_UI_INPUT_SAVED_RELOAD_REQUIRED')
if __name__=='__main__':main()
