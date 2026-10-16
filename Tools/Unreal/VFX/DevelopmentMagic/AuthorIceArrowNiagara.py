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
// UE5.8的CustomHlsl分词不把%视为分隔符；输入Index两侧必须独立，才能被替换为In_Index。
float a=max(0.0,Age); int i=Index % 128; float q=i*2.399963+a*1.7;
float fade=saturate(a/0.065)*saturate((Life-a)/0.18);
float3 position=float3(0,0,0); float3 scale=float3(1,1,1);
float4 rotation=float4(0,0,0,1); float2 size=float2(38,38);
float3 tint=float3(0.65,0.85,1.0); float opacity=fade;
// 用户指定施法仅保留两层地面法阵；内外层分开高度避免共面，原图RGB由专用材质读取。
if(Mode==0){float turn=a*.20;position=float3(0,0,6);scale=float3(3.8,3.8,1)*(.25+.75*saturate(a/.24));rotation=float4(0,0,sin(turn*.5),cos(turn*.5));tint=1;opacity=fade*.95;}
else if(Mode==1){float turn=-a*.25;position=float3(0,0,8);scale=float3(3.05,3.05,1)*(.30+.70*saturate(a/.30));rotation=float4(0,0,sin(turn*.5),cos(turn*.5));tint=1;opacity=fade*.62;}
else if(Mode==6){float turn=-a*.25;position=float3(0,0,295);scale=float3(2.4,2.4,1)*(.25+.75*saturate(a/.30));rotation=float4(0,0,sin(turn*.5),cos(turn*.5));tint=1;opacity=fade*.50;}
else if(Mode==2){position=float3(66,0,130+sin(a*4)*3);scale=float3(.30,.30,.95)*(.12+saturate(a/.65)*.88);rotation=float4(0,.70710678,0,.70710678);}
else if(Mode==3){float angle=i*6.2831853/7+a*.65;float r=110-saturate(a/.70)*18;position=float3(cos(angle)*r,sin(angle)*r,90+((i % 3)*56)+sin(a*3+i)*9);scale=float3(.35,.35,.62)*fade;rotation=normalize(float4(sin(angle)*.12,cos(angle)*.12,0,1));}
else if(Mode==4){float r=45+(i % 24)*3.4-saturate(a)*20;position=float3(18+cos(q)*r,sin(q)*r,15+((i*17) % 170)+a*45);size=float2(2+(i % 4),2+(i % 4));opacity*=.45;}
else if(Mode==5){position=float3(62,0,130);size=float2(74,74)*(.15+saturate(a/.65));opacity*=.18;}
// 飞行：自定义晶体箭身、三个弯曲流带、寒雾与碎晶组成可读体积，不用基础圆锥。
else if(Mode==10){scale=float3(1.20,1.20,1.20);position=float3(0,0,0);opacity=1;}
else if(Mode==11){float t=(i % 3)*2.094395+a*.65;rotation=float4(sin(t*.5),0,0,cos(t*.5));position=float3(-52,0,0);scale=float3(2.05,.30,.65);opacity=.58*fade;}
else if(Mode==12){float f=(i % 18)/17.0;position=float3(-25-f*395,cos(q)*15*(.25+f),sin(q)*15*(.25+f));size=float2(48+f*22,32+f*25);opacity*=.10*(1-f);}
else if(Mode==13){float f=(i % 14)/13.0;position=float3(-20-f*270,cos(q)*25,sin(q)*25);scale=float3(.055,.055,.17);rotation=normalize(float4(.2,sin(q),cos(q),1));opacity*=.70;}
else if(Mode==14){size=float2(95,95)*(1+a*3);opacity=saturate(1-a/.14);}
else if(Mode==15){position=float3(-25,0,0);scale=float3(.94,.94,1);rotation=float4(0,.70710678,0,.70710678);tint=1;opacity=.75*fade;}
// 命中：极短白芯→外推碎晶→短寿命冰冠→寒雾。局部范围只表达单次命中的冲量。
else if(Mode==20){size=float2(165,165)*(0.45+a*2);opacity=saturate(1-a/.14);tint=float3(1.3,1.5,1.7);}
else if(Mode==21){float angle=i*2.399963;float r=30+a*310;position=float3(cos(angle)*r,sin(angle)*r,15+a*(240+(i % 5)*36)-210*a*a);scale=float3(.18,.18,.55)*saturate((Life-a)/.30);rotation=normalize(float4(sin(q),cos(q),sin(q*.7),1));}
else if(Mode==22){position=float3(0,0,-65);float grow=saturate(a/.14);scale=float3(4.2,4.2,3.6)*grow;opacity=saturate((Life-a)/.55);}
else if(Mode==23){float r=18+a*155;position=float3(cos(q)*r,sin(q)*r,8+a*26);size=float2(115,115)*(.45+a*.9);opacity=fade*.16;}
// 地面残霜由独立向下静态地形查询放置，不把漂浮命中点伪装为地面。
else if(Mode==24){position=float3(0,0,4);scale=float3(6.8,6.8,1)*(.18+saturate(a/.28)*.82);opacity=fade*.80;}
else if(Mode==25){position=float3(0,0,7);float turn=-a*.12;rotation=float4(0,0,sin(turn*.5),cos(turn*.5));scale=float3(6.2,6.2,1)*(.25+saturate(a/.28)*.75);tint=1;opacity=fade*.65;}
Particles.Position=position;Particles.Scale=scale;Particles.MeshOrientation=rotation;
Particles.SpriteSize=size;Particles.SpriteRotation=(i*137.5)+Age*27;
Particles.Color=float4(tint,opacity);
#endif'''
# 名称、模式、数量、寿命秒、材质实例、网格；空网格表示Niagara原生Sprite。
SYSTEMS={
 'Cast':[('GroundOuter',0,1,2.1,'GroundOuter','Plane'),('GroundInner',1,1,2.1,'GroundInner','Plane')],
 'GroundRune':[('Outer',0,1,2.1,'GroundOuter','Plane'),('Inner',1,1,2.1,'GroundInner','Plane')],
 'Launch':[('LaunchFlash',14,1,.18,'ImpactFlash',None),('ReleaseShards',21,7,.35,'Crystal','CrystalShard')],
 'Flight':[('CrystalArrow',10,1,3.0,'Crystal','CrystalLance'),('FrostRibbons',11,3,3.0,'Trail','TrailRibbon'),('ColdMist',12,18,3.0,'Mist',None),('OrbitShards',13,14,3.0,'Crystal','CrystalShard'),('LaunchFlash',14,1,.18,'ImpactFlash',None)],
 'Impact':[('ImpactCore',20,1,.18,'ImpactFlash',None),('BurstShards',21,28,1.35,'ImpactCrystal','CrystalShard'),('IceCrown',22,1,1.45,'ImpactCrystal','IceCrown'),('ImpactMist',23,18,1.55,'Mist',None)],
 'FrostResidue':[('FrozenGround',24,1,3.0,'FrozenGround','Plane'),('FrostRing',25,1,2.4,'GroundOuter','Plane'),('FloorMist',23,10,1.6,'Mist',None)]
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
    # 模块ParameterMap必须公开且必需；隐藏输入会在保存重编后使调用节点InputMap成为孤立引脚。
    p.editor_python("import unreal\na=unreal.load_asset('"+MODULE+"')\nassert a is not None\n"
        "nodes=[n for n in unreal.ObjectIterator(unreal.NiagaraNodeInput) if n.get_path_name().startswith('"+MODULE+".')]\n"
        "assert len(nodes)==1\nn=nodes[0]\no=n.get_editor_property('exposure_options')\n"
        "o.set_editor_property('exposed',True)\no.set_editor_property('required',True)\no.set_editor_property('hidden',False)\n"
        "n.set_editor_property('exposure_options',o)\nassert unreal.EditorAssetLibrary.save_loaded_asset(a,False)")

def author_one(p,stage):
    """先真实创建发射器，再绑定材质/网格和寿命，GPU包围盒有限，编译后保存。"""
    path=DEST+'/Niagara/NS_DBA_IceArrow_'+stage
    p.editor_python("import unreal\nassert not unreal.EditorAssetLibrary.does_asset_exist('"+path+"'),'阶段资产已存在，拒绝盲目重建'")
    emitters=[]
    for name,mode,count,life,mat,mesh in SYSTEMS[stage]:
        props={'SimTarget':'GPUComputeSim','bLocalSpace':'True'}
        # 同一副作用CustomHlsl不得跨Spawn/Update复用：UE会缓存首个MapSpawn正文，使后续Update写入失效。
        # 原生GPU会对新生粒子执行Update，唯一Update调用完整设置位置/颜色/尺寸，不需要重复Spawn调用。
        modules=[{'stage':'emitter_update','script':'/Niagara/Modules/Emitter/SpawnBurst_Instantaneous.SpawnBurst_Instantaneous','inputs':{'Spawn Count':str(count)}},
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
