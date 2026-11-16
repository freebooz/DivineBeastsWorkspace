#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM通用VFX Monolith串行作者配方。所有UE资产均由Monolith原生动作写入。

不新建播放器、不改变技能；仅处理独占CommonSkillVFX命名空间，保留每次请求/响应。
"""
from pathlib import Path
import argparse
import importlib.util
import json

ROOT=Path(__file__).resolve().parents[4]
DEST='/DBAPresentationPack_Core/VFX/CommonSkillVFX'
MODULE=DEST+'/Niagara/NM_DBA_CommonVFXMotion'
SOURCE=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/SourceArt/VFX/CommonSkillVFX/V1/Sources.json'
HLSL=r'''#if GPU_SIMULATION
// 纯表现，位置厘米；外部参数只调视觉，不决定服务器伤害或位移。
float a=max(0.0,Age);float life=max(.15,Life);float t=saturate(a/life);
float fade=saturate(a/.08)*saturate((life-a)/.22);
float3 pos=float3(0,0,0);float3 sc=float3(Radius/120.0,Radius/120.0,Radius/120.0);
float turn=0;float alpha=fade;float4 color=Layer==1?Secondary:Primary;
if(Kind==0){turn=-.28+.55*t;sc*=.75+.25*saturate(t/.35);}
if(Kind==1){sc.x*=Length/400.0;sc.yz*=.95+.05*sin(a*8.0);}
if(Kind==2 && Layer==0){sc.xy*=.30+1.0*t;sc.z*=.7+.3*sin(3.141592*t);}
if(Kind==3){sc.x*=Length/400.0;sc.y*=Width/120.0;}
if(Kind>=4 && Kind<=7){sc*=.99+.01*sin(a*2.0);if(Layer==0)alpha*=.85;}
if(Kind==8 && Layer==0)turn=a*.18;
if(Kind==9 && Layer==0)turn=a*.7;
if(Kind==10 && Layer==0)turn=-a*.10;
if(Kind==11 && Layer==0){turn=a*.6;pos.z=sin(t*3.141592)*15;}
if(Kind==12){sc*=.5+.5*saturate(t/.32);}
if(Kind==13 && Layer==0){pos.z=8*sin(a*1.4);turn=a*.10;}
if(Kind==14 && Layer==0){pos.z=3*sin(a*1.1);}
if(Kind==15){sc.x*=Length/400.0;sc.y*=Width/120.0;}
if(Kind==16){sc.x*=Length/400.0;if(Layer==0)pos.x=-25*(1-t);}
if(Kind==17 || Kind==18)sc.x*=Length/400.0;
if(Kind==19 && Layer==0){sc.x*=.9+.1*sin(a*6.0);}
Particles.Position=pos;Particles.Scale=sc;
Particles.MeshOrientation=float4(0,0,sin(turn*.5),cos(turn*.5));
Particles.Color=float4(color.rgb*Intensity,alpha*color.a);
#endif'''


def protocol():
    spec=importlib.util.spec_from_file_location('common_vfx_protocol',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
    p.EVIDENCE=ROOT/'Saved/Validation/CommonSkillVFX/Authoring/Protocol'
    p.editor_python("import unreal\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\nassert unreal.SystemLibrary.get_engine_version().startswith('5.8.')\nprint('COMMON_VFX_EDITOR_VERIFIED')")
    return p


def engine(p,phase,keys=None):
    file=str(Path(__file__).with_name('AuthorCommonSkillVFXUE.py')).replace('\\','/')
    code='import runpy\nrunpy.run_path('+repr(file)+',init_globals='+repr(dict(PHASE=phase,KEYS=keys))+')'
    r=p.editor_python(code)
    print(phase,keys or 'all',json.dumps(r,ensure_ascii=False)[-500:],flush=True)


def ensure_module(p):
    r=p.editor_python("import unreal\nprint('MODULE_EXISTS',unreal.EditorAssetLibrary.does_asset_exist('"+MODULE+"'))")
    if 'MODULE_EXISTS True' not in json.dumps(r):
        inputs=[{'name':n,'type':t} for n,t in [('Age','float'),('Life','float'),('Radius','float'),('Length','float'),('Width','float'),('Intensity','float'),('Primary','color'),('Secondary','color'),('Kind','int'),('Layer','int')]]
        p.niagara('create_module_from_hlsl',name='CommonVFXMotion',save_path=MODULE,hlsl=HLSL,inputs=inputs,outputs=[],description='SSLM项目通用VFX纯表现运动，独立于生肖、MOBA和Gameplay权威')
        p.editor_python("import unreal\na=unreal.load_asset('"+MODULE+"')\nnodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith('"+MODULE+".')]\nassert len(nodes)==1\nn=nodes[0]\no=n.get_editor_property('exposure_options')\no.set_editor_property('exposed',True)\no.set_editor_property('required',True)\no.set_editor_property('hidden',False)\nn.set_editor_property('exposure_options',o)\nassert unreal.EditorAssetLibrary.save_loaded_asset(a,False)")


def color_default(hexvalue):
    vals=[int(hexvalue[i:i+2],16)/255 for i in (0,2,4)]
    vals=[v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in vals]
    # Monolith的用户Color默认值使用JSON对象字符串，非UE ImportText颜色格式。
    return json.dumps(dict(r=vals[0],g=vals[1],b=vals[2],a=1))


def author(p,row):
    key=row['key'];path=DEST+'/Niagara/NS_DBA_Common_'+key
    existing=p.editor_python("import unreal\nprint('SYSTEM_EXISTS',unreal.EditorAssetLibrary.does_asset_exist('"+path+"'))")
    if 'SYSTEM_EXISTS True' in json.dumps(existing):print('SKIP_EXISTING',key,flush=True);return
    parameters=[dict(name='User.'+n,type='float',default=str(v)) for n,v in [('Duration',row['duration']),('Radius',120),('Length',400),('Width',120),('Intensity',1),('Speed',400)]]
    for name,value in [('PrimaryColor',row['referenceSRGB']),('SecondaryColor','D5AA65' if row['category'] in ('Attack','Defense') else row['referenceSRGB'])]:parameters.append(dict(name='User.'+name,type='color',default=color_default(value)))
    emitters=[]
    for i,part in enumerate(('Core','Rim','Accent')):
        mat='Shell' if part=='Core' and key in ('Defense_Shield','Defense_Dome','Defense_Wall') else 'Rim' if part=='Rim' else 'Energy'
        emitters.append(dict(name=part,asset='/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal',properties={'SimTarget':'GPUComputeSim','bLocalSpace':'True'},
            modules=[dict(stage='emitter_update',script='/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous',inputs={'Spawn Count':'1'}),
                     dict(stage='particle_update',script=MODULE,bindings={'Age':'Particles.Age','Life':'User.Duration','Radius':'User.Radius','Length':'User.Length','Width':'User.Width','Intensity':'User.Intensity','Primary':'User.PrimaryColor','Secondary':'User.SecondaryColor'},inputs={'Kind':str(row['index']),'Layer':str(i)})],
            renderers=[dict({'class':'Mesh'},material=DEST+'/Materials/M_DBA_CommonVFX_'+mat)]))
    p.niagara('create_system_from_spec',save_path=path,spec=dict(user_parameters=parameters,emitters=emitters))
    for part in ('Core','Rim','Accent'):
        renderers=p.niagara('list_renderers',asset_path=path,emitter=part)['renderers']
        for renderer in reversed(renderers):
            if renderer['type']=='sprite':p.niagara('remove_renderer',asset_path=path,emitter=part,renderer_index=renderer['index'])
        p.niagara('set_renderer_mesh',asset_path=path,emitter=part,renderer_index=0,mesh=DEST+'/Meshes/SM_DBA_Common_'+key+'_'+part)
        p.niagara('set_module_input_binding',asset_path=path,emitter=part,module_node='InitializeParticle',input='Lifetime',binding='User.Duration')
        p.niagara('set_static_switch_value',asset_path=path,emitter=part,module_node='EmitterState',input='Life Cycle Mode',value='Self')
        p.niagara('set_emitter_loop_profile',asset_path=path,emitter=part,loop_behavior='Once',loop_duration=.15)
        p.niagara('set_fixed_bounds',asset_path=path,emitter=part,min=[-1800,-1000,-400],max=[1800,1000,1200],enabled=True)
    p.niagara('set_fixed_bounds',asset_path=path,min=[-1800,-1000,-400],max=[1800,1000,1200],enabled=True)
    d=p.niagara('get_system_diagnostics',asset_path=path,compile_first=True)
    if d.get('error_count',1):raise RuntimeError('原生编译失败：'+key+' '+json.dumps(d,ensure_ascii=False))
    p.niagara('save_system',asset_path=path,only_if_dirty=False)
    out=ROOT/'Saved/Validation/CommonSkillVFX/Authoring'/('System_'+key+'.json')
    out.write_text(json.dumps(dict(key=key,path=path,diagnostics=d,manualAccepted=False),ensure_ascii=False,indent=2),encoding='utf8')
    print('COMMON_VFX_SYSTEM_SAVED',key,'errors='+str(d.get('error_count')),'warnings='+str(d.get('warning_count')),flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--phase',choices=('import','materials','systems','definitions'),required=True);parser.add_argument('--keys',nargs='*');args=parser.parse_args()
    p=protocol();rows=json.loads(SOURCE.read_text(encoding='utf8'))['templates']
    if args.keys:rows=[r for r in rows if r['key'] in args.keys]
    if args.phase!='systems':engine(p,args.phase,args.keys);return
    ensure_module(p)
    for row in rows:author(p,row)


if __name__=='__main__':main()
