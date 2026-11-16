"""本地受控Monolith驱动；只连接已核对自有Editor，不使用HWebCodex。"""
from pathlib import Path
import argparse,json,runpy,uuid

root=Path(__file__).resolve().parents[3]
args=argparse.ArgumentParser();args.add_argument('phase',choices=['profiles','demo','visuals']);opt=args.parse_args()
launch=json.loads((root/'Saved/Validation/ProjectileMotion20261101/Editor.Launch.json').read_text('utf-8-sig'))
out=root/'Saved/Validation/ProjectileMotion20261101'/('Integration-'+str(uuid.uuid4()));out.mkdir(parents=True)
source=runpy.run_path(str(root/'Tools/Unreal/VFX/SpellProjectiles/IntegrateFireballNiagaraMonolith.py'))
source['Author'].__init__.__globals__['OUT']=out
p=source['Author'](9320,launch['PID'])
p.python('import unreal as u,os; assert os.getpid()=='+str(launch['PID'])+'; assert "-SSLMFireballMaterialsOwned" in u.SystemLibrary.get_command_line()')
cfg=json.loads((Path(__file__).with_name('ProjectileFlightProfiles.json')).read_text('utf-8'))
if opt.phase=='profiles':
    curve=cfg['demo_directory']+'/C_DBA_ProjectileSpeed_EaseOut'
    p.python('import unreal as u\np='+repr(curve)+'\nif not u.EditorAssetLibrary.does_asset_exist(p):\n folder,name=p.rsplit("/",1); assert u.AssetToolsHelpers.get_asset_tools().create_asset(name,folder,u.CurveFloat,u.CurveFloatFactory())')
    # 二次Ease-Out：Hermite端点切线2→0，得到2t-t²，速度与加速度均连续。
    keys=[{'InterpMode':'RCIM_Cubic','TangentMode':'RCTM_User','Time':0,'Value':0,'ArriveTangent':2,'LeaveTangent':2},
          {'InterpMode':'RCIM_Cubic','TangentMode':'RCTM_User','Time':1,'Value':1,'ArriveTangent':0,'LeaveTangent':0}]
    p.call('blueprint_query',{'action':'set_property_at_path','params':{'asset_path':curve,'path':'FloatCurve.Keys','value':keys,'strict':True,'save':True}})
    result=p.python(Path(__file__).with_name('AuthorProjectileFlightProfilesUE58.py').read_text('utf-8'))
elif opt.phase=='demo':
    result=p.python(Path(__file__).with_name('AuthorProjectileFlightDemoUE58.py').read_text('utf-8'))
else:
    result={}
    for stage in ('Cast','Flight','Impact'):
        path=p.system(stage)
        diagnostics=p.niagara('get_system_diagnostics',asset_path=path,compile_first=True)
        assert diagnostics['error_count']==0 and diagnostics['warning_count']==0,diagnostics
        p.niagara('save_system',asset_path=path)
        parameters=json.loads(p.niagara('get_user_parameters',asset_path=path)['result'])
        expected_types={'float':'NiagaraFloat','vec3':'Vector3f','int':'NiagaraInt32'}
        for expected in cfg['niagara_parameters']:
            actual=[row for row in parameters if row['name']==expected['name']]
            assert len(actual)==1 and actual[0]['type']==expected_types[expected['type']],actual
            if expected['type']=='vec3':assert json.loads(actual[0]['value'])=={'x':1,'y':0,'z':0},actual
        diagnostics['motion_user_parameters']=parameters
        result[stage]=diagnostics
(out/'Result.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
print(json.dumps({'phase':opt.phase,'evidence':str(out),'result':result},ensure_ascii=False))
