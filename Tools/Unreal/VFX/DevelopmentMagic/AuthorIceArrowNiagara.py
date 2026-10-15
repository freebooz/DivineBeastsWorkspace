#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""SSLM冰箭分阶段Niagara作者：只复用既有Monolith协议和平台VFX播放器。
先确认本任务编辑器和母材质，再创建共享GPU外观模块及独立系统。每一步保存请求/响应，
失败立即停止，不删除/重建已存在的系统。所有运动仅为外观，不写服务器碰撞、伤害或控制。
--system选择单阶段；省略时按Cast→GroundRune→Launch→Flight→Impact→FrostResidue执行。
"""
from pathlib import Path
import argparse, importlib.util, json
ROOT=Path(__file__).resolve().parents[4]
DEST='/DBAPresentationPack_Core/VFX/IceArrow'
MODULE=DEST+'/Niagara/NM_DBA_IceArrowMotion'
HLSL=r'''#if GPU_SIMULATION
// 全部位置均为局部厘米，复用真实Actor/根组件的运动；不会替代权威弹道。
float a=max(0.0,Age); int i=Index%128; float q=i*2.399963+a*1.7;
float fade=saturate(a/0.065)*saturate((Life-a)/0.18);
float3 position=float3(0,0,0); float3 scale=float3(1,1,1);
float4 rotation=float4(0,0,0,1); float2 size=float2(38,38);
float3 tint=float3(0.50,0.85,1.15); float opacity=fade;
// 施法以脚底锚点为原点：地面内外环反向旋转，冰核在角色前方凝聚。
if(Mode==0 || Mode==1){
 float turn=a*(Mode==0?0.33:-0.52); position=float3(0,0,2+Mode*0.8);
 scale=float3(2.65,2.65,1)*min(1.0,0.7+a*3.0);rotation=float4(0,0,sin(turn*.5),cos(turn*.5));
 opacity=fade*(Mode==0?0.80:0.95);
}
else if(Mode==2){position=float3(38,0,104+sin(a*4)*3);scale=float3(.22,.22,.45)*(0.3+saturate(a/.55)*.7);rotation=float4(0,sin(.785398),0,cos(.785398));}
else if(Mode==3){float r=52-saturate(a/.75)*25;position=float3(25+cos(q)*r,sin(q)*r,86+sin(q*.8)*23);scale=float3(.11,.11,.25)*fade;rotation=normalize(float4(sin(q*.5)*.25,cos(q*.5)*.25,0,1));}
else if(Mode==4){float r=25+(i%24)*2.4-saturate(a)*20;position=float3(18+cos(q)*r,sin(q)*r,15+((i*17)%95)+a*20);size=float2(3.5+(i%4),3.5+(i%4));opacity*=.60;}
else if(Mode==5){position=float3(42,0,103);size=float2(56,56)*(0.2+saturate(a/.55));opacity*=.40;}
// 飞行：自定义晶体箭身、三个弯曲流带、寒雾与碎晶组成可读体积，不用基础圆锥。
else if(Mode==10){scale=float3(.95,.95,.95);position=float3(0,0,0);opacity=1;}
else if(Mode==11){float t=(i%3)*2.094395+a*.75;rotation=float4(sin(t*.5),0,0,cos(t*.5));position=float3(-25,0,0);scale=float3(1,1,1);opacity=.45*fade;}
else if(Mode==12){float f=(i%18)/17.0;position=float3(-20-f*210,cos(q)*12*(.25+f),sin(q)*12*(.25+f));size=float2(40+f*26,36+f*30);opacity*=.11*(1-f);}
else if(Mode==13){float f=(i%14)/13.0;position=float3(-15-f*175,cos(q)*18,sin(q)*18);scale=float3(.045,.045,.12);rotation=normalize(float4(.2,sin(q),cos(q),1));opacity*=.65;}
else if(Mode==14){size=float2(95,95)*(1+a*3);opacity=saturate(1-a/.14);}
// 命中：极短白芯→外推碎晶→短寿命冰冠→寒雾。局部范围只表达单次命中的冲量。
else if(Mode==20){size=float2(165,165)*(0.45+a*2);opacity=saturate(1-a/.14);tint=float3(1.3,1.5,1.7);}
else if(Mode==21){float r=15+a*190;position=float3(cos(q)*r,sin(q)*r,15+a*(110+(i%5)*12)-145*a*a);scale=float3(.08,.08,.21)*saturate((Life-a)/.30);rotation=normalize(float4(sin(q),cos(q),sin(q*.7),1));}
else if(Mode==22){position=float3(0,0,-18);float grow=saturate(a/.13);scale=float3(.8,.8,.72)*grow;opacity=saturate((Life-a)/.38);}
else if(Mode==23){float r=12+a*78;position=float3(cos(q)*r,sin(q)*r,8+a*20);size=float2(88,88)*(0.45+a*.75);opacity=fade*.19;}
// 地面残霜由独立向下静态地形查询放置，不把漂浮命中点伪装为地面。
else if(Mode==24){position=float3(0,0,2);scale=float3(2.8,2.8,1)*(0.18+saturate(a/.28)*.82);opacity=fade*.60;}
else if(Mode==25){position=float3(0,0,3);float turn=-a*.28;rotation=float4(0,0,sin(turn*.5),cos(turn*.5));scale=float3(2.3,2.3,1)*(0.3+saturate(a/.18)*.7);opacity=fade*.55;}
Particles.Position=position;Particles.Scale=scale;Particles.MeshOrientation=rotation;
Particles.SpriteSize=size;Particles.SpriteRotation=(i*137.5)+Age*27;
Particles.Color=float4(tint,opacity);
#endif'''
# 名称、模式、数量、寿命秒、材质实例、网格；空网格表示Niagara原生Sprite。
SYSTEMS={
 'Cast':[('GroundOuter',0,1,1.0,'GroundOuter','Plane'),('GroundInner',1,1,1.0,'GroundInner','Plane'),('Core',2,1,1.0,'Crystal','CrystalShard'),('Orbit',3,7,1.0,'Crystal','CrystalShard'),('Snow',4,24,1.0,'Snow',None),('GatherGlow',5,1,1.0,'ImpactFlash',None)],
 'GroundRune':[('Outer',0,1,1.0,'GroundOuter','Plane'),('Inner',1,1,1.0,'GroundInner','Plane')],
 'Launch':[('LaunchFlash',14,1,.18,'ImpactFlash',None),('ReleaseShards',21,7,.35,'Crystal','CrystalShard')],
 'Flight':[('CrystalArrow',10,1,3.0,'Crystal','CrystalLance'),('FrostRibbons',11,3,3.0,'Trail','TrailRibbon'),('ColdMist',12,18,3.0,'Mist',None),('OrbitShards',13,14,3.0,'Crystal','CrystalShard'),('LaunchFlash',14,1,.18,'ImpactFlash',None)],
 'Impact':[('ImpactCore',20,1,.18,'ImpactFlash',None),('BurstShards',21,14,.85,'ImpactCrystal','CrystalShard'),('IceCrown',22,1,.70,'ImpactCrystal','IceCrown'),('ImpactMist',23,10,.95,'Mist',None)],
 'FrostResidue':[('FrozenGround',24,1,1.3,'FrozenGround','Plane'),('FrostRing',25,1,.55,'GroundOuter','Plane'),('FloorMist',23,7,.8,'Mist',None)]
}

def protocol():
    """既有Monolith协议只指向当前本机编辑器；身份错误立即停止，不改端口重试其它实例。"""
    spec=importlib.util.spec_from_file_location('ice_monolith',ROOT/'Tools/Unreal/VFX/Fauna/FaunaMonolithProtocol.py')
    p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)
    p.EVIDENCE=ROOT/'Saved/Validation/IceArrow/NiagaraAuthoring'
    p.editor_python("import unreal\nassert '-IceArrowOwnedAuthoring' in unreal.SystemLibrary.get_command_line()\nassert unreal.Paths.get_project_file_path().replace('\\\\','/').endswith('/DivineBeastsWorkspace/Game/DivineBeastsArena.uproject')\nprint('ICE_ARROW_EDITOR_OWNERSHIP_OK')")
    return p

def ensure_module(p):
    """共享纯外观脚本只创建一次；已存在时只加载，不覆盖已审查原生图。"""
    result=p.editor_python("import unreal\nprint('ICE_MOTION_EXISTS',unreal.EditorAssetLibrary.does_asset_exist('"+MODULE+"'))")
    exists=any('ICE_MOTION_EXISTS True' in str(x) for x in result.get('output',[]))
    if not exists:
        p.niagara('create_module_from_hlsl',name='IceArrowMotion',save_path=MODULE,hlsl=HLSL,
          inputs=[{'name':'Age','type':'float'},{'name':'Index','type':'int'},{'name':'Mode','type':'int'},{'name':'Life','type':'float'}],outputs=[],description='SSLM冰箭纯视觉：脚底法阵/晶体箭身/流带/碎冰/命中；位置厘米，时间秒，不参与伤害')
    p.editor_python("import unreal\na=unreal.load_asset('"+MODULE+"')\nassert a is not None\nassert unreal.EditorAssetLibrary.save_loaded_asset(a,False)")

def author_one(p,stage):
    """先真实创建发射器，再绑定材质/网格和寿命，GPU包围盒有限，编译后保存。"""
    path=DEST+'/Niagara/NS_DBA_IceArrow_'+stage
    p.editor_python("import unreal\nassert not unreal.EditorAssetLibrary.does_asset_exist('"+path+"'),'阶段资产已存在，拒绝盲目重建'")
    emitters=[]
    for name,mode,count,life,mat,mesh in SYSTEMS[stage]:
        props={'SimTarget':'GPUComputeSim','bLocalSpace':'True'}
        modules=[{'stage':'emitter_update','script':'/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous','inputs':{'Spawn Count':str(count)}},
            {'stage':'particle_spawn','script':MODULE,'bindings':{'Index':'Particles.UniqueID'},'inputs':{'Mode':str(mode),'Age':'0','Life':str(life)}},
            {'stage':'particle_update','script':MODULE,'bindings':{'Age':'Particles.Age','Index':'Particles.UniqueID'},'inputs':{'Mode':str(mode),'Life':str(life)}}]
        e={'name':name,'asset':'/Niagara/DefaultAssets/Templates/Emitters/Minimal.Minimal','properties':props,'modules':modules}
        if mesh:e['renderers']=[{'class':'Mesh','material':DEST+'/Materials/MI_DBA_IceArrow_'+mat}]
        emitters.append(e)
    p.niagara('create_system_from_spec',save_path=path,spec={'emitters':emitters})
    for name,mode,count,life,mat,mesh in SYSTEMS[stage]:
        renderers=p.niagara('list_renderers',asset_path=path,emitter=name)['renderers']
        if mesh:
            for r in reversed(renderers):
                if r['type']=='sprite':p.niagara('remove_renderer',asset_path=path,emitter=name,renderer_index=r['index'])
            mesh_path='/Engine/BasicShapes/Plane' if mesh=='Plane' else DEST+'/Meshes/SM_DBA_IceArrow_'+mesh
            p.niagara('set_renderer_mesh',asset_path=path,emitter=name,renderer_index=0,mesh=mesh_path)
        p.niagara('set_renderer_material',asset_path=path,emitter=name,renderer_index=0,material=DEST+'/Materials/MI_DBA_IceArrow_'+mat)
        p.niagara('set_module_input_value',asset_path=path,emitter=name,module_node='InitializeParticle',input='Lifetime',value=str(life))
        p.niagara('set_static_switch_value',asset_path=path,emitter=name,module_node='EmitterState',input='Life Cycle Mode',value='Self')
        p.niagara('set_emitter_loop_profile',asset_path=path,emitter=name,loop_behavior='Once',loop_duration=.15)
        extent=350 if stage=='Flight' else 300
        p.niagara('set_fixed_bounds',asset_path=path,emitter=name,min=[-extent]*3,max=[extent]*3,enabled=True)
    p.niagara('set_fixed_bounds',asset_path=path,min=[-400]*3,max=[400]*3,enabled=True)
    diagnostic=p.niagara('get_system_diagnostics',asset_path=path,compile_first=True)
    if diagnostic.get('error_count',1) or diagnostic.get('warning_count',1):raise RuntimeError('冰箭Niagara编译门禁失败：'+json.dumps(diagnostic,ensure_ascii=False))
    p.niagara('save_system',asset_path=path,only_if_dirty=False)
    print('ICE_ARROW_SYSTEM_COMPILED',stage,'emitters='+str(len(SYSTEMS[stage])))
    (p.EVIDENCE/(stage+'_Result.json')).write_text(json.dumps({'system':path,'diagnostics':diagnostic,'nativeSaved':True,'manualAccepted':False},ensure_ascii=False,indent=2),encoding='utf-8')

def main():
    """默认只展示阶段；--apply才调用已拥有的编辑器进行真实资产制作。"""
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--apply',action='store_true');parser.add_argument('--system',choices=tuple(SYSTEMS));args=parser.parse_args()
    stages=[args.system] if args.system else list(SYSTEMS)
    if not args.apply:print(json.dumps({'stages':stages,'write':False}));return
    p=protocol();ensure_module(p)
    for stage in stages:author_one(p,stage)
    print('ICE_ARROW_NIAGARA_PHASES_DONE',','.join(stages))
if __name__=='__main__':main()
