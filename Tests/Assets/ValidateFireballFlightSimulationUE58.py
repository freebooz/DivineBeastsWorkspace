"""只读验证当前真实帧RHI/SimCache捕获报告；不导入作者、不修改资产、不模拟网络验收。"""
from pathlib import Path
import json,hashlib,math,sys
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'Tools/Unreal/VFX/SpellProjectiles'))
from SpellPresentationContract import load_manifest

def validate(report_path):
    """缺帧、旧SHA、无粒子、错误寿命/分组/质量均失败；不使用退休的固定SpawnRate假设。"""
    report=Path(report_path).resolve()
    assert report.is_relative_to(ROOT/'Saved/Validation/FireballFlight')
    d=json.loads(report.read_text('utf-8'));cfg=load_manifest()['fireball_appearance']
    assert d['status']=='CompletePendingHumanReview',d.get('error',d['status'])
    assert d['engine_cap']==0 and d['frames'],'不是已完成的不限帧原生捕获'
    for file in d['frames']:assert Path(file).is_file() and Path(file).stat().st_size>1000
    assert d['manifest_sha256']==hashlib.sha256((ROOT/'Tools/Unreal/VFX/SpellProjectiles/PresentationManifest.json').read_bytes()).hexdigest(),'捕获合同已过期'
    system=ROOT/'Game/Plugins/DivineBeasts/ContentPacks/Presentation/DBAPresentationPack_Core/Content/VFX/SpellProjectiles/Niagara/NS_DBA_FireballFlight.uasset'
    assert d['system_sha256']==hashlib.sha256(system.read_bytes()).hexdigest(),'捕获System已过期'
    assert d.get('artifact_hashes'),'缺少材质/纹理/网格SHA'
    for path,digest in d['artifact_hashes'].items():assert hashlib.sha256((ROOT/path).read_bytes()).hexdigest()==digest,'捕获资产已变更:'+path
    s=d['simulation'];q=d['quality'];speed=d['speed_cm_s']
    assert s['engine_quality']==q,'质量档未生效'
    assert len(s['core_scale'])==1 and all(abs(v-cfg['head_diameter_cm']/100)<.001 for v in s['core_scale'][0]),'圆球主体非唯一或尺寸错误'
    assert len(s['glow_diameter'])==1 and all(abs(v-cfg['glow_diameter_cm'])<.001 for v in s['glow_diameter'][0]),'柔光尺寸错误'
    if speed>1:
        length=min(cfg['trail_length_cm'],speed*cfg['trail_max_lifetime_seconds'])
        assert length*.7<=s['trail_length']<=length*1.08,('尾长不符合动态速度合同',s['trail_length'],length)
        assert s['trail_count']>=math.floor(length/cfg['trail_spacing_cm']*.65),'按距离出生点数不足'
        assert s['trail_width_min_max'][1]>=cfg['trail_root_width_cm']*.75,'尾根宽度不足'
        if q>0:assert s['filament_ids']==list(range(cfg['filament_count'])),'缺少独立NiagaraID丝带'
    else:assert s['trail_count']==0,'静止时仍堆积世界尾迹'
    alpha=(0,.5,1,1.5)[min(q,3)]
    assert (q==0 and all(abs(a)<.001 for a in s['heat_alpha'])) or (q>0 and len(s['heat_alpha'])==1 and abs(s['heat_alpha'][0]-alpha)<.001),'热扰动实际强度不符'
    assert len(s['light_radius'])<=1,'超过一个真实局部光源粒子'
    result={'Passed':True,'Source':str(report),'Quality':q,'SpeedCmPerSecond':speed,'Simulation':s,'RuntimeNetworkAcceptance':False,'GPUBudgetMeasured':False,'ManualVisualAccepted':False}
    report.with_name('Simulation.Verified.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),'utf-8')
    return result

if __name__=='__main__':print(json.dumps(validate(sys.argv[1]),ensure_ascii=False))
